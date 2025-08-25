#include <stdio.h>
#include <stdlib.h>
#include <gccore.h>
#include <wiiuse/wpad.h>
#include <debug.h>
#include <string.h>
#include <malloc.h>

//#include "MediaDevices/DeviceHandler.hpp"

#include <sys/iosupport.h>
#include <sys/dir.h>

#include "tools.h"

#include <unistd.h>
#include <asndlib.h>
#include <ogc/system.h>
#include <unistd.h>
#include <fat.h>
//#include <sdcard/wiisd_io.h>

#include "BootHomebrew.h"
#include "FileHolder.h"
#include "Audio\BrstmPlayer.h"
#include "Audio\sfx.h"
#include "Common.h"

#include "Graphics\textures.h"
#include "Graphics\video.h"
#include "Graphics\GraphicsScene.h"
#include "Graphics\MainMenuScene.h"
#include "Graphics\ToolsScene.h"
#include "Graphics\AddonsScene.h"
#include "Graphics\TestScene.h"

#include "Graphics\UpdateScene.h"
#include "Graphics\FreeTypeGX.h"
#include "IOSLoader\sys.h"

#include "Launcher\launcher.h"
#include "USB\usbadapter.h"
#include "Network\networkloader.h"
#include "Patching\tinyxml2.h"

#include "tex_default_tpl.h"
#include "tex_default.h"
#include "vera_bold_ttf.h"

const int THREAD_SLEEP_MS(100);
static bool g_bHaltGUI = false;
static bool g_bGUIThreadIsSleeping = false;
static bool g_bExitRequested = false;
#define HBC_LULZ			0x000100014c554c5aULL
#define HBC_108				0x00010001af1bf516ULL
#define HBC_JODI			0x0001000148415858ULL
#define HBC_HAXX			0x000100014a4f4449ULL
#define RETURN_CHANNEL		0x0001000857494948ULL
#define SYSTEM_MENU			0x0000000100000002ULL
u8 * configFileData;
int configFileSize;
u8 * infoFileData;
int infoFileSize;

static GraphicsScene * g_pCurrentScene = nullptr;
static GXTexObj texBackgroundTile;
static GXTexObj texStylishMTexture;
TPLFile defaultTPL;
extern s32 wu_fd;
void BackToLoader(void);
static s32 iosVersion = 58;
f32 g_LauncherVersion = 1.15f;

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
		for (int i = 0; i < 4; i++)
		{
			for (int j = 0; j < 4; j++)
			{
				drawTexturedBox(&texBackgroundTile, 176 * i, 176 * j, 176, 176, 255, 255, 255, 255);
			}
		}

		//stylishM
		f32 fScreenWidth = getScreenWidth();
		f32 fScreenHeight = getScreenHeight();
		drawTexturedBox(&texStylishMTexture, fScreenWidth - 316, fScreenHeight - 341, 316, 341, 255, 255, 255, 165);

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
				if (g_bExitRequested)
					return nullptr;
				usleep(1000);

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

			g_pCurrentScene->HandleInputs(gcPressed, dStickX, dStickY, cStickX, cStickY, wiiPressed);
			g_pCurrentScene->Draw();

			Menu_Render();

			if (g_bExitRequested)
			{
				break;
			}

		}
		//exit(0);
		return nullptr;
	}
}
bool sdhcSupport = true;
bool usb2Support = true;
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
bool autoBoot = false;

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
	FileHolder configFile("sd:/Project+/launcher/config.xml", "r");
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

	tinyxml2::XMLDocument doc;

	if (configFileSize != 0 && doc.Parse((char *)configFileData, configFileSize) == (int)tinyxml2::XML_NO_ERROR)
	{
		tinyxml2::XMLElement* cur = doc.RootElement();
		if (!cur)
			return;

		tinyxml2::XMLElement* xeProjectM = cur;

		cur = xeProjectM->FirstChildElement("game");
		if (cur)
		{
			cur = cur->FirstChildElement("config");
			if (cur)
			{
				
			}
		}

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
					setConfigBoolValue(cur->FirstChildElement("useGCPads"), useGCPads);
					setConfigBoolValue(cur->FirstChildElement("useWiiPads"), useWiiPads);
					setConfigBoolValue(cur->FirstChildElement("useNetwork"), useNetwork);
					setConfigBoolValue(cur->FirstChildElement("useSoundEffects"), useSFX);
					setConfigBoolValue(cur->FirstChildElement("useMusic"), useMusic);
					setConfigBoolValue(cur->FirstChildElement("autoBoot"), autoBoot);
				}

				if (IsDolphin())
					cur = xeLauncherConfig->FirstChildElement("dolphin");
				else
					cur = xeLauncherConfig->FirstChildElement("wii");

				if (cur)
				{
					setConfigBoolValue(cur->FirstChildElement("useGCPads"), useGCPads);
					setConfigBoolValue(cur->FirstChildElement("useWiiPads"), useWiiPads);
					setConfigBoolValue(cur->FirstChildElement("useNetwork"), useNetwork);
					setConfigBoolValue(cur->FirstChildElement("useSoundEffects"), useSFX);
					setConfigBoolValue(cur->FirstChildElement("useMusic"), useMusic);
					setConfigBoolValue(cur->FirstChildElement("autoBoot"), autoBoot);
				}
			}
		}
	}
}

void launchUsbDol()
    {
    ClearArguments();
    FreeHomebrewBuffer();
                
    struct stat st;
    if (stat("sd:/Project+/RSBP01.gct", &st) == 0){AddBootArgument("RSBP01");}
    else if (stat("sd:/Project+/RSBJ01.gct", &st) == 0){AddBootArgument("RSBJ01");}
    else if (stat("sd:/Project+/RSBK01.gct", &st) == 0){AddBootArgument("RSBK01");}
    else {AddBootArgument("RSBE01");}
  
    FileHolder fBootElf("sd:/apps/projplus/usb.dol", "rb");
    int len = fBootElf.Size();
    u8 *dBootElf = (u8*)malloc(len);
   
    memset(dBootElf, 0, len);
    fBootElf.FRead(dBootElf, len, 1);
    fBootElf.FClose();
                
    CopyHomebrewMemory(dBootElf, 0, len);
    free(dBootElf);
    BootHomebrew();
}

int main(int argc, char **argv)
{

	//Required for LetterBomb and SDHC after Smashstack 
	if (!IsDolphin())
	{
		iosVersion = IOS_GetVersion();
		sdhcSupport = usb2Support = (iosVersion == 58) || (iosVersion == 80) || (IOS_ReloadIOS(80) >= 0) || (IOS_ReloadIOS(58) >= 0);
		if (!sdhcSupport)
			sdhcSupport = iosVersion == 60 || iosVersion == 70 || (IOS_ReloadIOS(70) >= 0) || (IOS_ReloadIOS(60) >= 0);
	}
VIDEO_Init();
	//  	VIDEO_Init();
		f32 fScreenWidth = getScreenWidth();
		f32 fScreenHeight = getScreenHeight();
	//devHandler.MountSD();
	//devHandler.MountSD();
	//devHandler.MountSD();
	//fatInit(0, true);
	fatInitDefault();
	//mountSDCard();
	loadConfigFile();
	loadInfoFile();
	loadConfig();

	WPAD_Init();
	WPAD_SetDataFormat(WPAD_CHAN_ALL, WPAD_FMT_BTNS_ACC_IR);
	WPAD_SetIdleTimeout(60 * 5); // idle after 5 minutes
	WPAD_SetVRes(WPAD_CHAN_0, fScreenWidth, fScreenHeight);
    PAD_Init();
	
	if (autoBoot){
		
		if (!IsDolphin()){sleep(3);}
		while(1){
			PAD_ScanPads();
			WPAD_ScanPads();
			u32 pad_down = PAD_ButtonsDown(0) | PAD_ButtonsDown(1) | PAD_ButtonsDown(2) | PAD_ButtonsDown(3);
			u32 wpad_down = WPAD_ButtonsDown(0) | WPAD_ButtonsDown(1) | WPAD_ButtonsDown(2) | WPAD_ButtonsDown(3);
		if ((wpad_down & WPAD_BUTTON_A) || (pad_down & PAD_BUTTON_A)){goto Menu;}
		break;}		//hold A while launching with autoBoot on, go to menu 
	/*	if ((wpad_down & WPAD_BUTTON_B) || (pad_down & PAD_BUTTON_B)){    ClearArguments();
    FreeHomebrewBuffer();
               
    FileHolder fBootElf("sd:/apps/projplus/usb.dol", "rb");
    int len = fBootElf.Size();
    u8 *dBootElf = (u8*)malloc(len);
   
    memset(dBootElf, 0, len);
    fBootElf.FRead(dBootElf, len, 1);
    fBootElf.FClose();
                
    CopyHomebrewMemory(dBootElf, 0, len);
    free(dBootElf);
    BootHomebrew();}*/
			if (!IsDolphin())WPAD_Shutdown();
			LaunchTitle();
			if (__io_usbstorage.startup() && __io_usbstorage.isInserted()){launchUsbDol();}
			else{BackToLoader();}
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

	if (useSFX)
		SFX_Init();


	if (useVideo && useFonts)
	{
		setLoadFlags(FT_LOAD_NO_HINTING);
		setRenderMode(FT_RENDER_MODE_NORMAL);
		InitFreeType(vera_bold_ttf, vera_bold_ttf_size);
	}

	if (useVideo && useDefaultTextures)
	{
		loadTextures();
		TPL_OpenTPLFromMemory(&defaultTPL, (void *)tex_default_tpl, tex_default_tpl_size);
		TPL_GetTexture(&defaultTPL, backgroundtile, &texBackgroundTile);
		TPL_GetTexture(&defaultTPL, stylishm, &texStylishMTexture);
	}

	BrstmPlayer* pMusicPlayer = NULL;
	if (useMusic)
	{
		FileHolder brstmFile("sd:/Project+/launcher/menu_music.brstm", "rb");
		if (brstmFile.IsOpen())
		{
			int len = brstmFile.Size();
			u8 * brstmData = (u8*)malloc(len);
			memset(brstmData, 0, len);

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

		//UIThread::ResumeGUI();

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
				case SCENE_TOOLS:
					g_pCurrentScene = new CToolsScene(fScreenWidth, fScreenHeight);;
					break;
				case SCENE_ADDONS:
					g_pCurrentScene = new CAddonsScene(fScreenWidth, fScreenHeight);;
					break;
				case SCENE_TEST:
					g_pCurrentScene = new CTestScene(fScreenWidth, fScreenHeight);;
					break;
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

		g_bExitRequested = true;


		for (int i = 0; i <= 255; i += 15)
		{
			UIThread::DrawDefaultBackground();
			//g_pCurrentScene->Draw();
			Menu_DrawRectangle(0, 0, screenwidth, screenheight, (GXColor){ 0, 0, 0, u8(i) }, 1);
			Menu_Render();
		}
		delete g_pCurrentScene;
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
		if (useFonts)
			DeinitFreeType();
		StopGX();
	}

	if (useNetwork){Network_Stop();}

	if (eNextScene == SCENE_LAUNCHTITLE)
	{
		USBAdapter_ReadBackground();
		LaunchTitle();
		if (__io_usbstorage.startup() && __io_usbstorage.isInserted()){launchUsbDol();}
	}
	else if (!IsDolphin()) //won't crash dolphin
	{
		if (useUSBAdapter)
			USBAdapter_Stop();
		BackToLoader();
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

