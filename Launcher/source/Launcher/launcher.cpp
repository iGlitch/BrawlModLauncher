#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <gccore.h>
#include <wiiuse/wpad.h>
#include <network.h>
#include <sdcard/wiisd_io.h>
#include <sys/dir.h>

#include "launcher.h"
#include "codehandler.h"
#include "apploader.h"
#include "codehandleronly.h"
#include "multidol.h"
#include "patchcode.h"        // exports: extern u8 configbytes[2]; extern u32 hooktype; extern u8 debuggerselect;
#include "disc.h"
#include "wdvd.h"
#include "video_tinyload.h"

/* ---------- Paths & ID ---------- */
#define GAMECONFIG   "sd:/Project+/gc.txt"
#define Disc_ID      ((vu32*)0x80000000)

/* ---------- Global config/state (kept from your original) ---------- */
u8  config_bytes[16] ATTRIBUTE_ALIGN(32);
int codes_state = 0;

u8 *tempcodelist  = (u8 *)0x90080000;   // temp buffer in MEM2
u8 *codelist      = NULL;               // Ocarina codelist location in MEM1
u8 *codelistend   = (u8 *)0x80003000;   // end of codelist region
u32 codelistsize  = 0;
char gameidbuffer[8];

int   patch_state = 0;
char *tempgameconf      = (char *)0x90080000;
u32  *gameconf          = NULL;
u32   tempgameconfsize  = 0;
u32   gameconfsize      = 0;
u8    configwarn        = 0;

/* tiny, valid empty GCT: 00D0C0DE 00D0C0DE ... F0000000 00000000 */
static const u32 empty_gct_words[4] = {
    0x00D0C0DE, 0x00D0C0DE, 0xF0000000, 0x00000000
};

const unsigned char defaultgameconfig[] = {
    'd','e','f','a','u','l','t',':','\n',
    'c','o','d','e','l','i','s','t','e','n','d',' ','=',' ','8','0','0','0','3','0','0','0'
};
const int defaultgameconfig_size = sizeof(defaultgameconfig);

/* ---------- apploader/boot globals (from your code) ---------- */
u32 GameIOS = 0;
u32 vmode_reg = 0;
u32 AppEntrypoint = 0;
GXRModeObj *vmode = NULL;
u8  vidMode = 0;
u8 *code_buf = NULL;
u32 code_size = 0;
int patchVidModes = 0x0;
u8  vipatchselect = 0;
u8  countryString = 0;
int aspectRatio = -1;
u8  debuggerselect = 0;     // keep this one definition (do NOT redefine later)

/* these are used by apploader/patcher */
extern "C" {
void __Sys_ResetCallback(void);
}

/* ---------- fwd decls ---------- */
static void sd_copy_gameconfig(char *gameid);
static void sd_copy_codes(char *filename);
void LaunchTitle();

/* ---------- init defaults ---------- */
bool valuesInitialized = false;
static void initValues()
{
    // Defaults copied from your file
    config_bytes[0]  = 0xCD; // language (system)
    config_bytes[1]  = 0x00; // video mode (auto)
    config_bytes[2]  = 0x07; // hook type (AXNextFrame)
    config_bytes[3]  = 0x00; // file patcher
    config_bytes[4]  = 0x01; // cheats
    config_bytes[5]  = 0x00; // no paused start
    config_bytes[6]  = 0x00; // gecko slot b
    config_bytes[7]  = 0x00; // debugger
    config_bytes[8]  = 0x00; // recovery hook
    config_bytes[9]  = 0x00; // region free
    config_bytes[10] = 0x00; // no copy
    config_bytes[11] = 0x00; // button skip
    config_bytes[12] = 0x00; // video modes patch
    config_bytes[13] = 0x00; // country string patch
    config_bytes[14] = 0x00; // aspect ratio patch
    config_bytes[15] = 0x00; // reserved
    valuesInitialized = true;
}

/* ---------- gameconfig parser (same logic as your file; typo fixed) ---------- */
static void sd_copy_gameconfig(char *gameid)
{
    FILE* fp;
    u32 ret;
    u32 filesize = 0;
    s32 gameidmatch, maxgameidmatch = -1, maxgameidmatch2 = -1;
    u32 i, numnonascii, parsebufpos;
    u32 codeaddr, codeval, codeaddr2, codeval2, codeoffset;
    u32 temp, tempoffset;
    char parsebuffer[18];

    memcpy(tempgameconf, defaultgameconfig, defaultgameconfig_size);
    tempgameconf[defaultgameconfig_size] = '\n';
    tempgameconfsize = defaultgameconfig_size + 1;

    fp = fopen(GAMECONFIG, "rb");
    if (fp) {
        fseek(fp, 0, SEEK_END);
        filesize = ftell(fp);
        fseek(fp, 0, SEEK_SET);
        ret = fread((void*)tempgameconf + tempgameconfsize, 1, filesize, fp);
        fclose(fp);
        if (ret == filesize)
            tempgameconfsize += filesize;
    }

    /* Remove non-ASCII */
    numnonascii = 0;
    for (i = 0; i < tempgameconfsize; i++) {
        if (tempgameconf[i] < 9 || tempgameconf[i] > 126) numnonascii++;
        else tempgameconf[i - numnonascii] = tempgameconf[i];
    }
    tempgameconfsize -= numnonascii;

    *(tempgameconf + tempgameconfsize) = 0;
    gameconf = (u32 *)((tempgameconf + tempgameconfsize) + (4 - (((u32)(tempgameconf + tempgameconfsize)) % 4)));

    for (maxgameidmatch = 0; maxgameidmatch <= 6; maxgameidmatch++) {
        i = 0;
        while (i < tempgameconfsize) {
            maxgameidmatch2 = -1;
            while (maxgameidmatch != maxgameidmatch2) {
                while (i != tempgameconfsize && tempgameconf[i] != ':') i++;
                if (i == tempgameconfsize) break;
                while ((tempgameconf[i] != 10 && tempgameconf[i] != 13) && (i != 0)) i--;
                if (i != 0) i++;
                parsebufpos = 0;
                gameidmatch = 0;
                while (tempgameconf[i] != ':') {
                    if (tempgameconf[i] == '?') {
                        parsebuffer[parsebufpos] = gameid[parsebufpos];
                        parsebufpos++;
                        gameidmatch--;
                        i++;
                    } else if (tempgameconf[i] != 0 && tempgameconf[i] != ' ')
                        parsebuffer[parsebufpos++] = tempgameconf[i++];
                    else if (tempgameconf[i] == ' ')
                        break;
                    else i++;
                    if (parsebufpos == 8) break;
                }
                parsebuffer[parsebufpos] = 0;

                if (strncasecmp("DEFAULT", parsebuffer, strlen(parsebuffer)) == 0 && strlen(parsebuffer) == 7) {
                    gameidmatch = 0;
                    goto idmatch;
                }
                if (strncmp(gameid, parsebuffer, strlen(parsebuffer)) == 0) {
                    gameidmatch += strlen(parsebuffer);
                idmatch:
                    if (gameidmatch > maxgameidmatch2)
                        maxgameidmatch2 = gameidmatch;
                }
                while ((i != tempgameconfsize) && (tempgameconf[i] != 10 && tempgameconf[i] != 13)) i++;
            }
            while (i != tempgameconfsize && tempgameconf[i] != ':') {
                parsebufpos = 0;
                while ((i != tempgameconfsize) && (tempgameconf[i] != 10 && tempgameconf[i] != 13)) {
                    if (tempgameconf[i] != 0 && tempgameconf[i] != ' ' && tempgameconf[i] != '(' && tempgameconf[i] != ':')
                        parsebuffer[parsebufpos++] = tempgameconf[i++];
                    else if (tempgameconf[i] == ' ' || tempgameconf[i] == '(' || tempgameconf[i] == ':')
                        break;
                    else i++;
                    if (parsebufpos == 17) break;
                }
                parsebuffer[parsebufpos] = 0;

                if (strncasecmp("codeliststart", parsebuffer, 13) == 0 && strlen(parsebuffer) == 13) {
                    sscanf(tempgameconf + i, " = %x", (u32 *)&codelist);
                }
                if (strncasecmp("codelistend", parsebuffer, 11) == 0 && strlen(parsebuffer) == 11) {
                    sscanf(tempgameconf + i, " = %x", (u32 *)&codelistend);
                }
                if (strncasecmp("hooktype", parsebuffer, 8) == 0 && strlen(parsebuffer) == 8) {
                    u32 hv; if (sscanf(tempgameconf + i, " = %u", &hv) == 1) if (hv <= 7) config_bytes[2] = hv;
                }
                if (strncasecmp("poke", parsebuffer, 4) == 0 && strlen(parsebuffer) == 4) {
                    u32 codeaddr, codeval;
                    if (sscanf(tempgameconf + i, "( %x , %x", &codeaddr, &codeval) == 2) {
                        *(gameconf + (gameconfsize / 4)) = 0;            gameconfsize += 4;
                        *(gameconf + (gameconfsize / 4)) = (u32)NULL;    gameconfsize += 8;
                        *(gameconf + (gameconfsize / 4)) = codeaddr;     gameconfsize += 4;
                        *(gameconf + (gameconfsize / 4)) = codeval;      gameconfsize += 4;
                        DCFlushRange((void *)(gameconf + (gameconfsize / 4) - 5), 20);
                    }
                }
                if (strncasecmp("pokeifequal", parsebuffer, 11) == 0 && strlen(parsebuffer) == 11) {
                    u32 codeaddr, codeval, codeaddr2, codeval2;
                    if (sscanf(tempgameconf + i, "( %x , %x , %x , %x", &codeaddr, &codeval, &codeaddr2, &codeval2) == 4) {
                        *(gameconf + (gameconfsize / 4)) = 0;            gameconfsize += 4;
                        *(gameconf + (gameconfsize / 4)) = codeaddr;     gameconfsize += 4;
                        *(gameconf + (gameconfsize / 4)) = codeval;      gameconfsize += 4;
                        *(gameconf + (gameconfsize / 4)) = codeaddr2;    gameconfsize += 4;
                        *(gameconf + (gameconfsize / 4)) = codeval2;     gameconfsize += 4;
                        DCFlushRange((void *)(gameconf + (gameconfsize / 4) - 5), 20);
                    }
                }
                if (strncasecmp("searchandpoke", parsebuffer, 13) == 0 && strlen(parsebuffer) == 13) {
                    u32 codeval, tempoffset, tempcnt=0, codeaddr, codeaddr2, codeoffset, codeval2;
                    if (sscanf(tempgameconf + i, "( %x%n", &codeval, (int*)&tempoffset) == 1) {
                        gameconfsize += 4;
                        while (1) {
                            *(gameconf + (gameconfsize / 4)) = codeval; gameconfsize += 4; tempcnt++;
                            i += tempoffset;
                            if (sscanf(tempgameconf + i, " %x%n", &codeval, (int*)&tempoffset) != 1) break;
                        }
                        *(gameconf + (gameconfsize / 4) - tempcnt - 1) = tempcnt;
                        if (sscanf(tempgameconf + i, " , %x , %x , %x , %x", &codeaddr, &codeaddr2, &codeoffset, &codeval2) == 4) {
                            *(gameconf + (gameconfsize / 4)) = codeaddr;   gameconfsize += 4;
                            *(gameconf + (gameconfsize / 4)) = codeaddr2;  gameconfsize += 4;
                            *(gameconf + (gameconfsize / 4)) = codeoffset; gameconfsize += 4;
                            *(gameconf + (gameconfsize / 4)) = codeval2;   gameconfsize += 4;
                            DCFlushRange((void *)(gameconf + (gameconfsize / 4) - tempcnt - 5), tempcnt * 4 + 20);
                        } else gameconfsize -= tempcnt * 4 + 4;
                    }
                }
                if (strncasecmp("videomode", parsebuffer, 9) == 0 && strlen(parsebuffer) == 9) {
                    u32 mv; if (sscanf(tempgameconf + i, " = %u", &mv) == 1) {
                        if      (mv == 0) { if (config_bytes[1] != 0x00) configwarn |= 1; config_bytes[1] = 0x00; }
                        else if (mv == 1) { if (config_bytes[1] != 0x01) configwarn |= 1; config_bytes[1] = 0x01; }
                        else if (mv == 2) { if (config_bytes[1] != 0x02) configwarn |= 1; config_bytes[1] = 0x02; }
                        else if (mv == 3) { if (config_bytes[1] != 0x03) configwarn |= 1; config_bytes[1] = 0x03; }
                    }
                }
                if (strncasecmp("language", parsebuffer, 8) == 0 && strlen(parsebuffer) == 8) {
                    u32 lg; if (sscanf(tempgameconf + i, " = %u", &lg) == 1) {
                        if (lg == 0) {
                            if (config_bytes[0] != 0xCD) configwarn |= 2;
                            config_bytes[0] = 0xCD;
                        } else if (lg > 0 && lg <= 10) {
                            if (config_bytes[0] != lg - 1) configwarn |= 2;
                            config_bytes[0] = lg - 1;
                        }
                    }
                }
                if (tempgameconf[i] != ':') {
                    while ((i != tempgameconfsize) && (tempgameconf[i] != 10 && tempgameconf[i] != 13)) i++;
                    if (i != tempgameconfsize) i++;
                }
            }
            if (i != tempgameconfsize) while ((tempgameconf[i] != 10 && tempgameconf[i] != 13) && (i != 0)) i--;
        }
    }

    tempcodelist = ((u8 *)gameconf) + gameconfsize;
}

/* ---------- gct loader ---------- */
static void sd_copy_codes(char *filename)
{
    FILE* fp;
    u32 ret, filesize;
    char filepath[256];

    DIR *pdir = opendir("/Project+/");
    if (pdir == NULL) { codes_state = 1; return; }
    closedir(pdir);

    sprintf(filepath, "sd:/Project+/%s.gct", filename);
    fp = fopen(filepath, "rb");
    if (!fp) { codes_state = 1; return; }

    fseek(fp, 0, SEEK_END);
    filesize = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    codelistsize = filesize;
    if ((codelist + codelistsize) > codelistend) { fclose(fp); codes_state = 4; return; }

    ret = fread((void*)tempcodelist, 1, filesize, fp);
    fclose(fp);
    if (ret != filesize) { codelistsize = 0; codes_state = 1; return; }

    DCFlushRange((void*)tempcodelist, filesize);
    codes_state = 2;
}

/* ---------- Reset callback reboots game ---------- */
void __Sys_ResetCallback(void)
{
    LaunchTitle();
}

void LaunchTitle()
{
    if (!valuesInitialized)
        initValues();

    Disc_SetLowMemPre();
    WDVD_Init();
    if (Disc_Open() != 0) {
        sleep(2);
        return; // can't open disc
    }

    // Only do GameConfig / GCT setup for Brawl (RSB*01)
    if ( ((*(u32*)Disc_ID) & 0xFFFFFF00) == 0x52534200 ) {
        memset(gameidbuffer, 0, 8);
        memcpy(gameidbuffer, (char*)0x80000000, 6);

        // Where the Ocarina code list will live in MEM1
        codelist = (u8 *)0x800028B8;

        // Build default config and (optionally) merge sd:/Project+/gc.txt if present
        sd_copy_gameconfig(gameidbuffer);  // safe if gc.txt missing

        // Try to load sd:/Project+/<GAMEID>.gct; if missing, we’ll just launch without codes
        sd_copy_codes(gameidbuffer);

        __io_wiisd.shutdown();
        printf("\x1b[2J");

        // Push a few settings into the existing globals used by your apploader/patcher
        configbytes[0] = config_bytes[0];
        configbytes[1] = config_bytes[1];
        hooktype       = config_bytes[2];
        debuggerselect = config_bytes[7];

        // Set pointers to the temp buffer gathered by sd_copy_codes()
        code_buf  = tempcodelist;
        code_size = codelistsize;

        // IMPORTANT CHANGE:
        // If no codes, DON'T return — just clear pointers and continue to boot the disc.
        if (code_size <= 0) {
            code_buf  = NULL;
            code_size = 0;
        }
        if (code_size > (u32)codelistend - (u32)codelist) {
            code_buf  = NULL;
            code_size = 0;
        }

        // Video mode mapping from your config bytes
        countryString = config_bytes[13];
        switch (config_bytes[1]) {
            case 0: vidMode = 0; break;
            case 1: vidMode = 3; break;
            case 2: vidMode = 2; break;
            case 3: vidMode = 4; break;
        }
        switch (config_bytes[12]) {
            case 0: break;
            case 1: vipatchselect = 1; break;
            case 2: patchVidModes = 0; break;
            case 3: patchVidModes = 1; break;
            case 4: patchVidModes = 2; break;
        }
        if (config_bytes[14] > 0)
            aspectRatio = (int)config_bytes[14] - 1;
    }

    // Normal disc boot flow (unchanged)
    u32 offset = 0;
    Disc_FindPartition(&offset);
    WDVD_OpenPartition(offset, &GameIOS);
    vmode = Disc_SelectVMode(0, &vmode_reg);
    AppEntrypoint = Apploader_Run(vidMode, vmode, vipatchselect, countryString, patchVidModes, aspectRatio);

    // Install handler shell and only wire codes if we actually have some
    memcpy((void*)0x80001800, codehandleronly, codehandleronly_size);
    if (code_size > 0 && code_buf) {
        // point the handler to codelist
        memcpy((void*)0x80001906, &codelist, 2);
        memcpy((void*)0x8000190A, ((u8*)&codelist) + 2, 2);
    }
    DCFlushRange((void*)0x80001800, codehandleronly_size);
    ICInvalidateRange((void*)0x80001800, codehandleronly_size);

    if (codelist) memset(codelist, 0, (u32)codelistend - (u32)codelist);
    if (code_size > 0 && code_buf) {
        memcpy(codelist, code_buf, code_size);
        DCFlushRange(codelist, (u32)codelistend - (u32)codelist);
    }

    // (gameconf patches kept as-is)
    if (gameconfsize != 0) {
        u32 *codeaddr, *codeaddr2, *addrfound = NULL;
        for (u32 i = 0; i < (gameconfsize / 4); i++) {
            if (*(gameconf + i) == 0) {
                if (((u32 *)(*(gameconf + i + 1))) == NULL ||
                    *((u32 *)(*(gameconf + i + 1))) == *(gameconf + i + 2)) {
                    *((u32 *)(*(gameconf + i + 3))) = *(gameconf + i + 4);
                    DCFlushRange((void *)*(gameconf + i + 3), 4);
                }
                i += 4;
            } else {
                codeaddr  = (u32 *)(*(gameconf + i + *(gameconf + i) + 1));
                codeaddr2 = (u32 *)(*(gameconf + i + *(gameconf + i) + 2));
                if (codeaddr == 0 && addrfound != NULL)        codeaddr = addrfound;
                else if (codeaddr == 0 && codeaddr2 != 0)      codeaddr = (u32 *)((((u32)codeaddr2) >> 28) << 28);
                else if (codeaddr == 0 && codeaddr2 == 0) {    i += *(gameconf + i) + 4; continue; }
                if (codeaddr2 == 0)                             codeaddr2 = codeaddr + *(gameconf + i);
                addrfound = NULL;
                while (codeaddr <= (codeaddr2 - *(gameconf + i))) {
                    if (memcmp(codeaddr, gameconf + i + 1, (*(gameconf + i)) * 4) == 0) {
                        *(codeaddr + ((*(gameconf + i + *(gameconf + i) + 3)) / 4)) = *(gameconf + i + *(gameconf + i) + 4);
                        if (addrfound == NULL) addrfound = codeaddr;
                        DCFlushRange((void *)(codeaddr + ((*(gameconf + i + *(gameconf + i) + 3)) / 4)), 4);
                    }
                    codeaddr++;
                }
                i += *(gameconf + i) + 4;
            }
        }
    }

    // Final jump to game
    WDVD_Close();
    Disc_SetLowMem(GameIOS);
    Disc_SetTime();
    Disc_SetVMode(vmode, vmode_reg);

    u32 level = IRQ_Disable();
    __IOS_ShutdownSubsystems();
    *(vu32*)0xCC003024 = 1;
    SYS_SetResetCallback(__Sys_ResetCallback);

    if (AppEntrypoint == 0x3400) {
        if (hooktype) {
            asm volatile (
                "lis %r3, returnpointdisc@h\n"
                "ori %r3, %r3, returnpointdisc@l\n"
                "mtlr %r3\n"
                "lis %r3, 0x8000\n"
                "ori %r3, %r3, 0x18A8\n"
                "nop\n"
                "mtctr %r3\n"
                "bctr\n"
                "returnpointdisc:\n"
                "bl DCDisable\n"
                "bl ICDisable\n"
                "li %r3, 0\n"
                "mtsrr1 %r3\n"
                "lis %r4, AppEntrypoint@h\n"
                "ori %r4,%r4,AppEntrypoint@l\n"
                "lwz %r4, 0(%r4)\n"
                "mtsrr0 %r4\n"
                "rfi\n"
            );
        }
    } else {
        asm volatile (
            "lis %r3, AppEntrypoint@h\n"
            "ori %r3, %r3, AppEntrypoint@l\n"
            "lwz %r3, 0(%r3)\n"
            "mtlr %r3\n"
            "lis %r3, 0x8000\n"
            "ori %r3, %r3, 0x18A8\n"
            "nop\n"
            "mtctr %r3\n"
            "bctr\n"
        );
    }
    IRQ_Restore(level);
}
