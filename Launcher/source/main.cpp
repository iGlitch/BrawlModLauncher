#include <stdio.h>
#include <stdlib.h>
#include <gccore.h>
#include <wiiuse/wpad.h>
#include <debug.h>
#include <string.h>
#include <malloc.h>

#include <ogc/machine/processor.h>
#include <sys/iosupport.h>
#include <sys/dir.h>
#include <sys/stat.h>
//#include "tools.h"  // Dead code moved to unused/ folder

#include <unistd.h>
#include <asndlib.h>
#include <ogc/system.h>
#include <unistd.h>
#include <fat.h>

#include "FileHolder.h"
#include "Audio/BrstmPlayer.h"
#include "Audio/sfx.h"
#include "Common.h"

#include "Graphics/textures.h"
#include "Graphics/video.h"
#include "Graphics/GraphicsScene.h"
#include "Graphics/MainMenuScene.h"
//#include "Graphics/ToolsScene.h"  // Dead code moved to unused/ folder
#include "Graphics/AddonsScene.h"
#include "Graphics/SettingsScene.h"
//#include "Graphics/TestScene.h"  // Dead code moved to unused/ folder

#include "Graphics/UpdateScene.h"
#include "Graphics/FreeTypeGX.h"
#include "IOSLoader/sys.h"

#include "USB/usbadapter.h"
#include "Network/networkloader.h"
#include "Patching/tinyxml2.h"

#include "vera_bold_ttf.h"
#include "fatmounter.h"
//#include "dolloader.h"  // Dead code moved to unused/ folder
#include "app_booter_bin.h"
#include <limits.h>

const int THREAD_SLEEP_MS(100);
static bool g_bHaltGUI = false;
static bool g_bGUIThreadIsSleeping = false;
static bool g_bExitRequested = false;
static bool g_networkStarted = false;
static bool g_powerOffRequested = false;
static bool g_powerButtonPressed = false;
static bool g_resetButtonPressed = false;

#define HBC_LULZ            0x000100014c554c5aULL
#define HBC_108             0x00010001af1bf516ULL
#define HBC_JODI            0x0001000148415858ULL
#define HBC_HAXX            0x000100014a4f4449ULL
#define RETURN_CHANNEL      0x0001000857494948ULL
#define SYSTEM_MENU         0x0000000100000002ULL

#define EXECUTE_ADDR ((u8*)0x92000000)
#define BOOTER_ADDR  ((u8*)0x93000000)
#define ARGS_ADDR    ((u8*)0x93200000)

u8 * configFileData;
int configFileSize;
u8 * infoFileData;
int infoFileSize;

extern "C" void __exception_closeall(void) __attribute__((weak));
extern "C" void build_argv(struct __argv *);

#define MAX_CMDLINE 4096
#define MAX_ARGV    1000
struct __argv args;
char *a_argv[MAX_ARGV];
char *meta_buf = NULL;
char customDolPath[ISFS_MAXPATH] = "";
char codesBasePath[ISFS_MAXPATH] = "";
char projectName[64] = "Brawl Mod";
char updateUrl[512] = "";

void arg_init()
{
	memset(&args, 0, sizeof(args));
	memset(a_argv, 0, sizeof(a_argv));
	args.argvMagic = ARGV_MAGIC;
	args.length = 1; // double \0\0
	args.argc = 0;
	//! Put the argument into mem2 too, to avoid overwriting it
	args.commandLine = (char *) ARGS_ADDR + sizeof(args);
	args.argv = a_argv;
	args.endARGV = a_argv;
}

char* strcopy(char *dest, const char *src, int size)
{
	strncpy(dest,src,size);
	dest[size-1] = 0;
	return dest;
}
int arg_addl(char *arg, int len)
{
	if (args.argc >= MAX_ARGV) 
		return -1;
	if (args.length + len + 1 > MAX_CMDLINE) 
		return -1;
	strcopy(args.commandLine + args.length - 1, arg, len+1);
	args.length += len + 1; // 0 term.
	args.commandLine[args.length - 1] = 0; // double \0\0
	args.argc++;
	args.endARGV = args.argv + args.argc;
	return 0;
}

int arg_add(char *arg)
{
	return arg_addl(arg, strlen(arg));
}

void load_meta(const char *exe_path)
{
	char meta_path[ISFS_MAXPATH] ATTRIBUTE_ALIGN(32);
	memset(meta_path, 0, ISFS_MAXPATH);
	const char *p;

	p = strrchr(exe_path, '/');
	snprintf(meta_path, sizeof(meta_path), "%.*smeta.xml", p ? p-exe_path+1 : 0, exe_path);

	s32 fd = ISFS_Open(meta_path, ISFS_OPEN_READ);
	if(fd >= 0)
	{
		s32 filesize = ISFS_Seek(fd, 0, SEEK_END);
		ISFS_Seek(fd, 0, SEEK_SET);
		if(filesize)
		{
			meta_buf = (char*)memalign(32, filesize + 1);
			memset(meta_buf, 0, filesize + 1);
			ISFS_Read(fd, meta_buf, filesize);
		}
		ISFS_Close(fd);
	}
	else
	{
		FILE *exeFile = fopen(meta_path ,"rb");
		if(exeFile)
		{
			fseek(exeFile, 0, SEEK_END);
			u32 exeSize = ftell(exeFile);
			rewind(exeFile);
			if(exeSize)
			{
				meta_buf = (char*)memalign(32, exeSize + 1);
				memset(meta_buf, 0, exeSize + 1);
				fread(meta_buf, 1, exeSize, exeFile);
			}
			fclose(exeFile);
		}
	}
}

void strip_comments(char *buf)
{
	char *p = buf; // start of comment
	char *e; // end of comment
	int len;
	while (p && *p) 
	{
		p = strstr(p, "<!--");
		if (!p) 
			break;
		e = strstr(p, "-->");
		if (!e) 
		{
			*p = 0; // terminate
			break;
		}
		e += 3;
		len = strlen(e);
		memmove(p, e, len + 1); // +1 for 0 termination
	}
}

void parse_meta()
{
	char *p;
	char *e, *end;
	if (meta_buf == NULL) 
		return;
	strip_comments(meta_buf);
	if (!strstr(meta_buf, "<app") || !strstr(meta_buf, "</app>"))
		return;

	p = strstr(meta_buf, "<arguments>");
	if (!p) 
		return;

	end = strstr(meta_buf, "</arguments>");
	if (!end) 
		return;

	do 
	{
		p = strstr(p, "<arg>");
		if (!p) 
			return;
		p += 5; //strlen("<arg>");
		e = strstr(p, "</arg>");
		if (!e) 
			return;
		arg_addl(p, e-p);
		p = e + 6;
	} 
	while (p < end);

	if (meta_buf)
	{ 
		free(meta_buf); 
		meta_buf = NULL; 
	}
}

void parse_arguments(int argc, char **argv)
{
    for (int i = 0; i < argc; i++)
    {
        if (strncasecmp(argv[i], "codespath=", 10) == 0)
        {
            const char *val = argv[i] + 10;
            strncpy(codesBasePath, val, sizeof(codesBasePath) - 1);
            codesBasePath[sizeof(codesBasePath) - 1] = '\0';
            
            // Remove trailing slash
            size_t len = strlen(codesBasePath);
            if (len > 1 && codesBasePath[len-1] == '/') {
                codesBasePath[len-1] = '\0';
            }
        }
    }
}

LoadMethod loadMethod = LOAD_AUTO;

static void DeInitDevices() {
    WPAD_Flush(0);
    WPAD_Disconnect(0);
    WPAD_Shutdown();
    // If you mounted FAT, these are safe no-ops if not mounted:
    fatUnmount("sd:/");
    fatUnmount("usb:/");
}
typedef void (*entrypoint)(void);

static const char* PickGameID() {
    struct stat st;
    char path[ISFS_MAXPATH];
    
    snprintf(path, sizeof(path), "%s/RSBP01.gct", codesBasePath);
    if (stat(path, &st) == 0) return "RSBP01";
    
    snprintf(path, sizeof(path), "%s/RSBJ01.gct", codesBasePath);
    if (stat(path, &st) == 0) return "RSBJ01";
    
    snprintf(path, sizeof(path), "%s/RSBK01.gct", codesBasePath);
    if (stat(path, &st) == 0) return "RSBK01";
    
    return "RSBE01";
}

static GraphicsScene * g_pCurrentScene = nullptr;
extern s32 wu_fd;
void BackToLoader(void);
static s32 iosVersion = 58;
f32 g_LauncherVersion = 2.1f;
extern bool useDefaultTextures;
extern bool isBadBoot;

void WiimotePowerPressed(s32 chan)
{
    g_powerOffRequested = true;
}

void ConsolePowerPressed(void)
{
    g_powerButtonPressed = true;
}

void WiiResetPressed()
{
    g_resetButtonPressed = true;
}

namespace UIThread
{
    static lwp_t guithread = LWP_THREAD_NULL;

    void ResumeGUI()
    {

        g_bHaltGUI = false;
        while (g_bGUIThreadIsSleeping)
            usleep(THREAD_SLEEP_MS);
    }

    void HaltGUI()
    {
        g_bHaltGUI = true;

        // wait for thread to finish
        while (!g_bGUIThreadIsSleeping)
            usleep(THREAD_SLEEP_MS);
    }


    void DrawDefaultBackground()
    {
        f32 fScreenWidth = getScreenWidth();
        f32 fScreenHeight = getScreenHeight();

        if (useDefaultTextures && isTexturesLoaded())
        {
            for (int i = 0; i < 4; i++)
            {
                for (int j = 0; j < 4; j++)
                {
                    drawTexturedBox(&backgroundTileTexture, 176 * i, 176 * j, 176, 176, 255, 255, 255, 255);
                }
            }

            //stylishM
            drawTexturedBox(&stylishmTexture, fScreenWidth - 316, fScreenHeight - 341, 316, 341, 255, 255, 255, 165);
        }

        //spotlight
        drawCircle(fScreenWidth / 2.0f, fScreenHeight / 2.0f, fScreenWidth / 2.0f, 255, 255, 255, 32, 255, 255, 255, 0);
    }


    void* UpdateGUI(void*)
    {
        while (true)
        {
            while (g_bHaltGUI)
            {
                g_bGUIThreadIsSleeping = true;
                if (g_bExitRequested) { return nullptr; }
                usleep(1000);
            }
			
            if (g_powerOffRequested) {
                SYS_ResetSystem(SYS_POWEROFF, 0, 0);
            }

            g_bGUIThreadIsSleeping = false;
            DrawDefaultBackground();

            PAD_ScanPads();
            WPAD_ScanPads();
            UPAD_ScanPads();

            //get input from all pads
            u32 gcPressed(0);
            u32 wiiPressed(0);
            s8 dStickX(0);
            s8 dStickY(0);
            s8 cStickX(0);
            s8 cStickY(0);
            for (int i(0); i < PAD_CHANMAX; ++i) gcPressed |= PAD_ButtonsDown(i);
            for (int i(0); i < PAD_CHANMAX; ++i) gcPressed |= UPAD_ButtonsDown(i);

            for (int i(0); i < PAD_CHANMAX; ++i) wiiPressed |= WPAD_ButtonsDown(i);

            for (int i(0); i < PAD_CHANMAX && !dStickX; ++i) dStickX = PAD_StickX(i);
            for (int i(0); i < PAD_CHANMAX && !dStickX; ++i) dStickX = UPAD_StickX(i);

            for (int i(0); i < PAD_CHANMAX && !dStickY; ++i) dStickY = PAD_StickY(i);
            for (int i(0); i < PAD_CHANMAX && !dStickY; ++i) dStickY = UPAD_StickY(i);

            for (int i(0); i < PAD_CHANMAX && !cStickX; ++i) cStickX = PAD_SubStickX(i);
            for (int i(0); i < PAD_CHANMAX && !cStickX; ++i) cStickX = UPAD_SubStickX(i);

            for (int i(0); i < PAD_CHANMAX && !cStickY; ++i) cStickY = PAD_SubStickY(i);
            for (int i(0); i < PAD_CHANMAX && !cStickY; ++i) cStickY = UPAD_SubStickY(i);

            if (!g_pCurrentScene) { usleep(1000); continue; }

            g_pCurrentScene->HandleInputs(gcPressed, dStickX, dStickY, cStickX, cStickY, wiiPressed, g_powerButtonPressed, g_resetButtonPressed);
            g_powerButtonPressed = false;
            g_resetButtonPressed = false;
            g_pCurrentScene->Draw();

            Menu_Render();

            if (g_bExitRequested) { break; }
        }
        //exit(0);
        return nullptr;
    }
}

bool sdhcSupport = true;
bool usb2Support = true;
bool isBadBoot = false;
bool autoBoot = false;
bool useGCPads = true;
bool useWiiPads = true;
bool useVideo = true;
bool useNetwork = true;
bool useUSBAdapter = true;
bool useSFX = true;
bool useMusic = true;
bool useFonts = true;
bool useDefaultTextures = true;
bool useMenus = true;
bool debugToFile = false;

void setConfigLoadMethodValue(tinyxml2::XMLElement * xe, LoadMethod & setting)
{
    if (xe && xe->FirstChild() && xe->FirstChild()->ToText())
    {
        const char* value = xe->FirstChild()->ToText()->Value();
        if (strcasecmp(value, "auto") == 0)
            setting = LOAD_AUTO;
        else if (strcasecmp(value, "disc") == 0)
            setting = LOAD_DISC;
        else if (strcasecmp(value, "usb") == 0)
            setting = LOAD_USB;
    }
}

void setConfigBoolValue(tinyxml2::XMLElement * xe, bool & setting)
{
    if (xe && xe->FirstChild() && xe->FirstChild()->ToText())
    {
        const char* value = xe->FirstChild()->ToText()->Value();
        if (strcasecmp(value, "true") == 0
            || strcasecmp(value, "yes") == 0
            || strcasecmp(value, "y") == 0
            || strcasecmp(value, "1") == 0)
            setting = true;
        else if (strcasecmp(value, "false") == 0
            || strcasecmp(value, "no") == 0
            || strcasecmp(value, "n") == 0
            || strcasecmp(value, "0") == 0)
            setting = false;
    }
}

void loadConfigFile()
{
    char cfgpath[ISFS_MAXPATH];
    snprintf(cfgpath, sizeof(cfgpath), "%s/launcher/config.xml", codesBasePath);

    FileHolder configFile(cfgpath, "r");
    if (configFile.IsOpen())
    {
        configFileSize = configFile.Size();
        configFileData = (u8*)malloc(configFileSize);
        memset(configFileData, 0, configFileSize);

        configFile.FRead(configFileData, configFileSize, 1);
        configFile.FClose();
    }
}

void loadConfig()
{
    //Load defaults
    useSFX = !IsDolphin(); //HLE Support
    useMusic = !IsDolphin(); //HLE Support
    //useNetwork = !IsDolphin();

    tinyxml2::XMLDocument doc;

    if (configFileSize != 0 && doc.Parse((char *)configFileData, configFileSize) == (int)tinyxml2::XML_NO_ERROR)
    {
    tinyxml2::XMLElement* cur = doc.RootElement();
    if (!cur)
        return;

        tinyxml2::XMLElement* xeProjectM = cur;
/*
        cur = xeProjectM->FirstChildElement("game");
        if (cur)
        {
    cur = cur->FirstChildElement("config");
            if (cur)
            {

            }
        }
*/
        cur = xeProjectM->FirstChildElement("launcher");
        if (cur)
        {
            cur = cur->FirstChildElement("config");
            if (cur)
            {
    tinyxml2::XMLElement* xeLauncherConfig = cur;
    cur = xeLauncherConfig->FirstChildElement("global");
    if (cur)
    {
        setConfigLoadMethodValue(cur->FirstChildElement("loadMethod"), loadMethod);
        setConfigBoolValue(cur->FirstChildElement("autoBoot"), autoBoot);
        setConfigBoolValue(cur->FirstChildElement("useGCPads"), useGCPads);
        setConfigBoolValue(cur->FirstChildElement("useWiiPads"), useWiiPads);
        setConfigBoolValue(cur->FirstChildElement("useNetwork"), useNetwork);
        setConfigBoolValue(cur->FirstChildElement("useSoundEffects"), useSFX);
        setConfigBoolValue(cur->FirstChildElement("useMusic"), useMusic);
        setConfigBoolValue(cur->FirstChildElement("debugToFile"), debugToFile);
    }

    if (IsDolphin())
        cur = xeLauncherConfig->FirstChildElement("dolphin");
    else
        cur = xeLauncherConfig->FirstChildElement("wii");

    if (cur)
    {
        setConfigLoadMethodValue(cur->FirstChildElement("loadMethod"), loadMethod);
        setConfigBoolValue(cur->FirstChildElement("autoBoot"), autoBoot);
        setConfigBoolValue(cur->FirstChildElement("useGCPads"), useGCPads);
        setConfigBoolValue(cur->FirstChildElement("useWiiPads"), useWiiPads);
        setConfigBoolValue(cur->FirstChildElement("useNetwork"), useNetwork);
        setConfigBoolValue(cur->FirstChildElement("useSoundEffects"), useSFX);
        setConfigBoolValue(cur->FirstChildElement("useMusic"), useMusic);
        setConfigBoolValue(cur->FirstChildElement("debugToFile"), debugToFile);
                }
            }
        }
    }
}

static bool IsUSBMounted() {
    DIR *d = opendir("usb:/wbfs/");
    if (d) {
        struct dirent *pent;
        while ((pent = readdir(d)) != NULL) {
            if (strstr(pent->d_name, "Super Smash Bros. Brawl") != NULL ||
                strstr(pent->d_name, "RSBE01") != NULL ||
                strstr(pent->d_name, "RSBP01") != NULL ||
                strstr(pent->d_name, "RSBJ01") != NULL ||
                strstr(pent->d_name, "RSBK01") != NULL) {
                closedir(d);
                return true;
            }
        }
        closedir(d);
    }
    return false;
}

static u8 *homebrewbuffer = (u8*)0x92000000;  // EXECUTE_ADDR
static u32 homebrewsize = 0;

static inline bool IsDollZ(const u8 *buf) {
    return (buf[0x100] == 0x3C);
}

int BootHomebrew(char **argv) {
    u32 cpu_isr;

    // set path to loader.dol from boot.dol executable path
    const char* lastSlash = strrchr(argv[0], '/');
    if (lastSlash) {
        snprintf(customDolPath, ISFS_MAXPATH, "%.*sloader.dol", (int)(lastSlash - argv[0] + 1), argv[0]);
    }

    FILE* f = fopen(customDolPath, "rb");
    if (!f) {
        // Nothing found, go back to SysMenu
        if (useDefaultTextures) { unloadTextures(); }
        SDCard_deInit();
        USBDevice_deInit();
        SYS_ResetSystem(SYS_RETURNTOMENU, 0, 0);
        return -1;
    }

    fseek(f, 0, SEEK_END);
    u32 homebrewsize = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (homebrewsize == 0) {
        fclose(f);
        if (useDefaultTextures) { unloadTextures(); }
        SDCard_deInit();
        USBDevice_deInit();
        SYS_ResetSystem(SYS_RETURNTOMENU, 0, 0);
        return -1;
    }

    size_t read = fread(EXECUTE_ADDR, 1, homebrewsize, f);
    fclose(f);

    if (read != homebrewsize) {
        if (useDefaultTextures) { unloadTextures(); }
        SDCard_deInit();
        USBDevice_deInit();
        SYS_ResetSystem(SYS_RETURNTOMENU, 0, 0);
        return -1;
    }

    if (!IsDollZ(EXECUTE_ADDR)) {
        switch (loadMethod) {
            case LOAD_AUTO:
                if (IsUSBMounted()) {
                    const char *gid = PickGameID();
                    if (gid && *gid) {
                        arg_addl(gid, strlen(gid));
                    }
                } else {
                    arg_add("-sdmode=1");
                }
                break;
            case LOAD_DISC:
                arg_add("-sdmode=1");
                break;
            case LOAD_USB:
                const char *gid = PickGameID();
                if (gid && *gid) {
                    arg_addl(gid, strlen(gid));
                }
                break;
        }

        char codesPath[ISFS_MAXPATH];
        snprintf(codesPath, sizeof(codesPath), "-codespath=%s", codesBasePath);
        arg_add(codesPath);

        if (debugToFile)
            arg_add("-debug=1");
    }

    DCFlushRange(EXECUTE_ADDR, homebrewsize);

    memcpy(BOOTER_ADDR, app_booter_bin, app_booter_bin_size);
    DCFlushRange(BOOTER_ADDR, app_booter_bin_size);
    ICInvalidateRange(BOOTER_ADDR, app_booter_bin_size);

    entrypoint entry = (entrypoint)BOOTER_ADDR;

    if (!IsDollZ(EXECUTE_ADDR))
        memcpy((void*)ARGS_ADDR, &args, sizeof(struct __argv));
    else
        memset((void*)ARGS_ADDR, 0, sizeof(struct __argv));

    DCFlushRange((void*)ARGS_ADDR, sizeof(struct __argv) + args.length);

    SDCard_deInit();
    USBDevice_deInit();

    WPAD_Flush(0);
    WPAD_Disconnect(0);
    WPAD_Shutdown();

    // Required: libogc USB storage leaks internal IOS file descriptors (libogc issue #109)
    // that cannot be closed via __io_usbstorage.shutdown(). 
    // Without this reload, loader.dol fails to mount USB due to stale handles.
    IOS_ReloadIOS(IOS_GetVersion());
    SYS_ResetSystem(SYS_SHUTDOWN, 0, 0);
    _CPU_ISR_Disable(cpu_isr);
    if (__exception_closeall) __exception_closeall();
    entry();
    _CPU_ISR_Restore(cpu_isr);

    return 0;
}

int main(int argc, char **argv)
{
    VIDEO_Init();
    f32 fScreenWidth = getScreenWidth();
    f32 fScreenHeight = getScreenHeight();
    //devHandler.MountSD();
    //devHandler.MountSD();
    //devHandler.MountSD();
    //fatInit(0, true);
    arg_init();
    arg_add(argv[0]);
    load_meta(argv[0]);
    parse_meta();
    parse_arguments(argc, argv);
    build_argv(&args);

    // A normal launch should always have the first arg be the path
    char *ptr = strrchr(argv[0], '/');
    if (!ptr || (argv[0][2] != ':' && argv[0][3] != ':'))
        isBadBoot = true;

    fatInitDefault();
    //mountSDCard();
    loadConfigFile();
    loadInfoFile();
    loadConfig();

    WPAD_Init();
    WPAD_SetDataFormat(WPAD_CHAN_ALL, WPAD_FMT_BTNS_ACC_IR);
    WPAD_SetIdleTimeout(60 * 5); // idle after 5 minutes
    WPAD_SetVRes(WPAD_CHAN_0, fScreenWidth, fScreenHeight);
    WPAD_SetPowerButtonCallback(WiimotePowerPressed);
    SYS_SetPowerCallback(ConsolePowerPressed);
    SYS_SetResetCallback(WiiResetPressed);
    PAD_Init();

if (autoBoot)
{
    if (!IsDolphin()) {
        for (int i = 0; i < 30; i++) {
            PAD_ScanPads();
            WPAD_ScanPads();
            u32 pad_down  = PAD_ButtonsDown(0)  | PAD_ButtonsDown(1)  | PAD_ButtonsDown(2)  | PAD_ButtonsDown(3);
            u32 wpad_down = WPAD_ButtonsDown(0) | WPAD_ButtonsDown(1) | WPAD_ButtonsDown(2) | WPAD_ButtonsDown(3);
            if ((wpad_down & WPAD_BUTTON_A) || (pad_down & PAD_BUTTON_A) || SYS_ResetButtonDown()) {
                goto Menu;
            }
            usleep(100000); // 100ms sleep, check 30 times = 3 seconds total
        }
    }
    WPAD_Shutdown();
    BootHomebrew(argv);
    //BackToLoader();
    return 0;
}
        else{
Menu:

    if (useVideo)
    {
        InitVideo();
        fScreenWidth = getScreenWidth();
        fScreenHeight = getScreenHeight();
    }

    if (useNetwork)
    {
        Network_Init();
        Network_Start();
        g_networkStarted = true;
    }

    if (useUSBAdapter)
    {
        USBAdapter_Init();
        USBAdapter_Start();
    }

    if (useSFX || useMusic)
    {
        AUDIO_Init(NULL);
        ASND_Init();
        ASND_Pause(0);
    }

    if (useSFX) { SFX_Init(); }

    if (useVideo && useFonts || isBadBoot)
    {
        setLoadFlags(FT_LOAD_NO_HINTING);
        setRenderMode(FT_RENDER_MODE_NORMAL);
        InitFreeType(vera_bold_ttf, vera_bold_ttf_size);
    }

    if (useVideo && useDefaultTextures)
    {
        setTexturesBasePath(argv[0]);
        if (loadTextures() != 0)
            useDefaultTextures = false;  // Disable textures if load failed
    }

	BrstmPlayer* pMusicPlayer = NULL;
	if (useMusic)
	{
		// Build the path to launcher.brstm using codespath
		char brstmPath[ISFS_MAXPATH];
		if (infoMusicPath[0])
			snprintf(brstmPath, sizeof(brstmPath), "%s/pf/sound/strm/%s", codesBasePath, infoMusicPath);
		else
			snprintf(brstmPath, sizeof(brstmPath), "%s/pf/sound/strm/project m/launcher.brstm", codesBasePath);
	
		FileHolder brstmFile(brstmPath, "rb");
		if (brstmFile.IsOpen())
		{
			int len = brstmFile.Size();
			u8 *brstmData = (u8*)malloc(len);
	
			brstmFile.FRead(brstmData, len, 1);
			brstmFile.FClose();
			pMusicPlayer = new BrstmPlayer(brstmData, len);
			pMusicPlayer->Init();
			pMusicPlayer->Play();
		}
		else
		{
			pMusicPlayer = new BrstmPlayer(NULL, 0);
		}
	}


    ESceneType eCurrentScene(SCENE_MAIN_MENU);
    ESceneType eNextScene(SCENE_LAUNCHTITLE);

    if (useVideo && useMenus)
    {
        eNextScene = SCENE_MAIN_MENU;
        //GraphicsScreen* pMenu =
        //GraphicsScreen* pTools = new CToolsScene(fScreenWidth, fScreenHeight);
        //GraphicsScreen* pPatcher = new CPatchScene(fScreenWidth, fScreenHeight);
        //GraphicsScreen* pUpdater = new CUpdateScene(fScreenWidth, fScreenHeight);
        g_pCurrentScene = new CMainMenuScene(fScreenWidth, fScreenHeight);
        g_pCurrentScene->Load();


        LWP_CreateThread(&UIThread::guithread, UIThread::UpdateGUI, NULL, NULL, 0, 64);

        UIThread::ResumeGUI();

        while (true)
        {
            g_pCurrentScene->Work();
            eNextScene = g_pCurrentScene->GetNextSceneType();


            if (eCurrentScene != eNextScene)
            {
                UIThread::HaltGUI();
                g_pCurrentScene->Unload();
                if (eNextScene == SCENE_EXIT || eNextScene == SCENE_LAUNCHTITLE || eNextScene == SCENE_LAUNCHELF)
                    break;
                delete g_pCurrentScene;
                switch (eNextScene)
                {
                case SCENE_MAIN_MENU:
                    g_pCurrentScene = new CMainMenuScene(fScreenWidth, fScreenHeight);
                    break;
                //case SCENE_TOOLS:
                //    g_pCurrentScene = new CToolsScene(fScreenWidth, fScreenHeight);;
                //    break;
                case SCENE_ADDONS:
                    g_pCurrentScene = new CAddonsScene(fScreenWidth, fScreenHeight);;
                    break;
                case SCENE_SETTINGS:
                    g_pCurrentScene = new CSettingsScene(fScreenWidth, fScreenHeight);;
                    break;
                //case SCENE_TEST:
                //    g_pCurrentScene = new CTestScene(fScreenWidth, fScreenHeight);;
                //    break;
                case SCENE_UPDATE:
                    g_pCurrentScene = new CUpdateScene(fScreenWidth, fScreenHeight);;
                    break;

                default:
                    break;
                }
                eCurrentScene = eNextScene;
                g_pCurrentScene->Load();
                UIThread::ResumeGUI();
            }
            usleep(THREAD_SLEEP_MS);
        }

        UIThread::HaltGUI();
        g_bExitRequested = true;
        LWP_JoinThread(UIThread::guithread, NULL);

        for (int i = 0; i <= 255; i += 15)
        {
            UIThread::DrawDefaultBackground();
            //g_pCurrentScene->Draw();
            Menu_DrawRectangle(0, 0, screenwidth, screenheight, (GXColor){ 0, 0, 0, u8(i) }, 1);
            Menu_Render();
        }
        delete g_pCurrentScene;
        g_pCurrentScene = nullptr;
        //LWP_SuspendThread(UIThread::guithread);

        //delete g_pCurrentScene;
        //delete pMenu;
        //delete pTools;
        //delete pPatcher;
        //delete pUpdater;
    }

    if (useMusic && pMusicPlayer)
    {
        pMusicPlayer->Stop();
        delete pMusicPlayer;
    }

    if (useSFX || useMusic){ASND_End();}

    if (useWiiPads)
    {
        //u32 cnt;

        /* Disconnect Wiimotes */
        //for (cnt = 0; cnt < 4; cnt++) WPAD_Disconnect(cnt);

        if (!IsDolphin()) //Will lock Dolphin when Play is selected
            WPAD_Shutdown();
    }

    if (useVideo)
    {
        if (useFonts) { DeinitFreeType(); }
        if (useDefaultTextures) { unloadTextures(); }
        StopGX();
    }

    if (useNetwork){ Network_Stop(); }

    if (eNextScene == SCENE_LAUNCHTITLE)
    {
        BootHomebrew(argv);
        //BackToLoader();  // if no disc, return to menu
    }
}
}

void BackToLoader(void)
{
    WII_Initialize();
    WII_LaunchTitle(HBC_LULZ);
    WII_LaunchTitle(HBC_108);
    WII_LaunchTitle(HBC_JODI);
    WII_LaunchTitle(HBC_HAXX);
    exit(0);
    //SYS_ResetSystem(SYS_RETURNTOMENU, 0, 0);
}