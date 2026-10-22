#include "SettingsScene.h"
#include "../Common.h"
#include "../Audio/sfx.h"
#include <malloc.h>
#include <wchar.h>
#include <wiiuse/wpad.h>
#include "../Patching/tinyxml2.h"

using namespace tinyxml2;

extern char codesBasePath[ISFS_MAXPATH];

CSettingsScene::CSettingsScene(f32 w, f32 h)
{
	m_eNextScreen = SCENE_SETTINGS;

	m_fScreenWidth = w;
	m_fScreenHeight = h;
	m_bIsLoaded = false;
	m_iMenuSelectionAnimationFrames = 15;
	m_fMaxNewsScrollFrames = f32(60 * 25);
	m_fCurrentNewsScrollFrame = f32(-1);
	m_bShowScrollbar = false;
	m_fScrollbarHeight = 0.0f;
	m_fScrollbarY = 0.0f;

	swprintf(m_wszInfoText, 512, L"Reading settings files...");
}

CSettingsScene::~CSettingsScene()
{
	Unload();
}

void CSettingsScene::Load()
{
	if (m_bIsLoaded)
		return;

	m_iMenuSelectedIndex = -2;
	m_iMenuSelectionFrame = 0;
	m_iDrawFrameNumber = 0;
	m_sPrevDStickX = s8(0);
	m_sPrevDStickY = s8(0);
	m_eNextScreen = SCENE_SETTINGS;

	InitializeSettings();
	LoadConfigXml();
	LoadLoaderCfg();

	m_iMenuSelectedIndex = 0;
	m_bIsLoaded = true;

	showAutoBootInfoPopup = false;
	autoBootInfoPopup = new Popup(m_fScreenWidth, m_fScreenHeight, 0.90f, 0.55f, 0.40f, 0.75f);
	autoBootInfoPopup->setAnimationFrames(15, true);
	autoBootInfoPopup->setSelectionTextItems(0, 1, L"OK");
	autoBootInfoPopup->setLineTextItems(3, L"Auto Boot is enabled.", L"Hold A during boot to access", L"the launcher menu.");

	showResetMessage = false;
}

void CSettingsScene::Unload()
{
	if (!m_bIsLoaded)
		return;

	m_vSettings.clear();
	delete autoBootInfoPopup;
	m_bIsLoaded = false;
}

void CSettingsScene::InitializeSettings()
{
	m_vSettings.clear();

	if (codesBasePath[0] == '\0') {
		return;
	}

	SSetting loadMethod;
	loadMethod.type = SETTING_LOAD_METHOD;
	loadMethod.name = "Load Method";
	loadMethod.currentValue = 0;
	loadMethod.options.push_back("Auto");
	loadMethod.options.push_back("Disc");
	loadMethod.options.push_back("USB");
	m_vSettings.push_back(loadMethod);

	SSetting autoBoot;
	autoBoot.type = SETTING_AUTO_BOOT;
	autoBoot.name = "Auto Boot";
	autoBoot.currentValue = 0;
	autoBoot.options.push_back("OFF");
	autoBoot.options.push_back("ON");
	m_vSettings.push_back(autoBoot);

	SSetting videoMode;
	videoMode.type = SETTING_VIDEO_MODE;
	videoMode.name = "Video Mode";
	videoMode.currentValue = 1;
	videoMode.options.push_back("");					// 0 - System Default (removed)
	videoMode.options.push_back("Auto");				// 1 - Disc Default
	videoMode.options.push_back("");					// 2 - Force PAL50 (removed)
	videoMode.options.push_back("Force PAL 480i");		// 3 - Force PAL60
	videoMode.options.push_back("Force NTSC 480i");		// 4 - Force NTSC
	videoMode.options.push_back("");					// 5 - Region Patch (removed)
	videoMode.options.push_back("Force PAL 480p");		// 6 - Force PAL480p
	videoMode.options.push_back("Force NTSC 480p");		// 7 - Force NTSC480p
	m_vSettings.push_back(videoMode);

	SSetting deflicker;
	deflicker.type = SETTING_DEFLICKER;
	deflicker.name = "Deflicker Filter";
	deflicker.currentValue = 0;
	deflicker.options.push_back("Auto");            	// 0
	deflicker.options.push_back("OFF (Safe)");      	// 1
	deflicker.options.push_back("OFF (Extended)");  	// 2
	deflicker.options.push_back("ON (Low)");        	// 3
	deflicker.options.push_back("ON (Medium)");     	// 4
	deflicker.options.push_back("ON (High)");       	// 5
	m_vSettings.push_back(deflicker);

	SSetting aspectRatio;
	aspectRatio.type = SETTING_GAME_ASPECT_RATIO;
	aspectRatio.name = "Aspect Ratio";
	aspectRatio.currentValue = 2;
	aspectRatio.options.push_back("Force 4:3");      	// 0
	aspectRatio.options.push_back("Force 16:9");     	// 1
	aspectRatio.options.push_back("System"); 			// 2
	m_vSettings.push_back(aspectRatio);

	SSetting language;
	language.type = SETTING_LANGUAGE;
	language.name = "Game Language";
	language.currentValue = 0;
	language.options.push_back("System"); 				// 0
	language.options.push_back("English");      		// 1
	language.options.push_back("German");       		// 2
	language.options.push_back("French");       		// 3
	language.options.push_back("Spanish");      		// 4
	language.options.push_back("Italian");      		// 5
	language.options.push_back("Japanese");      		// 6 - saves as 0
	language.options.push_back("Korean");       		// 7 - saves as 6
	m_vSettings.push_back(language);

	SSetting loaderIOS;
	loaderIOS.type = SETTING_LOADERIOS_CIOS;
	loaderIOS.name = "Loader IOS";
	loaderIOS.currentValue = 1;
	loaderIOS.options.push_back("248");					// 0
	loaderIOS.options.push_back("249");					// 1
	loaderIOS.options.push_back("250");					// 2
	loaderIOS.options.push_back("251");					// 3
	m_vSettings.push_back(loaderIOS);

	SSetting usbPort;
	usbPort.type = SETTING_USBPORT;
	usbPort.name = "USB Port";
	usbPort.currentValue = 0;
	usbPort.options.push_back("USB Port 0");			// 0
	usbPort.options.push_back("USB Port 1");			// 1
	usbPort.options.push_back("Both Ports");			// 2
	m_vSettings.push_back(usbPort);

	SSetting multiplePartitions;
	multiplePartitions.type = SETTING_MULTIPLE_PARTITIONS;
	multiplePartitions.name = "Multiple Partitions";
	multiplePartitions.currentValue = 0;
	multiplePartitions.options.push_back("OFF");
	multiplePartitions.options.push_back("ON");
	m_vSettings.push_back(multiplePartitions);

	SSetting nandEmuMode;
	nandEmuMode.type = SETTING_NANDEMUMODE;
	nandEmuMode.name = "EmuNAND Save Mode";
	nandEmuMode.currentValue = 0;
	nandEmuMode.options.push_back("OFF");				// 0
	nandEmuMode.options.push_back("Partial");			// 1
	nandEmuMode.options.push_back("Full");				// 2
	m_vSettings.push_back(nandEmuMode);

	SSetting nandEmuPath;
	nandEmuPath.type = SETTING_NANDEMUPATH;
	nandEmuPath.name = "EmuNAND Save Path";
	nandEmuPath.currentValue = 0;
	nandEmuPath.options.push_back("SD");
	nandEmuPath.options.push_back("USB");
	m_vSettings.push_back(nandEmuPath);

	SSetting debugToFile;
	debugToFile.type = SETTING_DEBUG_TO_FILE;
	debugToFile.name = "Debug";
	debugToFile.currentValue = 0;
	debugToFile.options.push_back("OFF");
	debugToFile.options.push_back("ON");
	m_vSettings.push_back(debugToFile);

	SSetting resetToDefaults;
	resetToDefaults.type = SETTING_RESET_TO_DEFAULTS;
	resetToDefaults.name = "Default Settings";
	resetToDefaults.currentValue = 0;
	m_vSettings.push_back(resetToDefaults);
}

void CSettingsScene::LoadLoaderCfg()
{
	char configPath[ISFS_MAXPATH];
	snprintf(configPath, sizeof(configPath), "%s/launcher/loader.cfg", codesBasePath);

	FILE *file = fopen(configPath, "r");
	if (!file)
		return;

	fseek(file, 0, SEEK_END);
	long fileSize = ftell(file);
	fseek(file, 0, SEEK_SET);

	if (fileSize <= 0) {
		fclose(file);
		return;
	}

	char *fileBuffer = (char*)malloc(fileSize + 1);
	if (!fileBuffer) {
		fclose(file);
		return;
	}

	size_t bytesRead = fread(fileBuffer, 1, fileSize, file);
	fileBuffer[bytesRead] = '\0';
	fclose(file);

	char *line = fileBuffer;
	char *nextLine;

	while (line < fileBuffer + bytesRead)
	{
		nextLine = strchr(line, '\n');
		if (!nextLine) nextLine = fileBuffer + bytesRead;
		*nextLine = '\0';

		if (line[0] != '#' && line[0] != '\n' && line[0] != '\r' && line[0] != '\0')
		{
			char *equals = strchr(line, '=');
			if (equals)
			{
				*equals = '\0';
				char *key = line;
				char *value = equals + 1;

				while (*key == ' ' || *key == '\t') key++;
				while (*value == ' ' || *value == '\t') value++;

				// Properly trim trailing whitespace from key
				char *keyEnd = key + strlen(key) - 1;
				while (keyEnd > key && (*keyEnd == ' ' || *keyEnd == '\t' || *keyEnd == '\n' || *keyEnd == '\r'))
					*keyEnd-- = '\0';

				if (strcmp(key, "videomode") == 0 || strcmp(key, "video_mode") == 0)
				{
					int loadedValue = atoi(value);
					m_vSettings[SETTING_VIDEO_MODE].currentValue = loadedValue;
				}
				else if (strcmp(key, "deflicker") == 0)
				{
					int loadedValue = atoi(value);
					m_vSettings[SETTING_DEFLICKER].currentValue = loadedValue;
				}
				else if (strcmp(key, "GameAspectRatio") == 0)
				{
					int loadedValue = atoi(value);
					m_vSettings[SETTING_GAME_ASPECT_RATIO].currentValue = loadedValue;
				}
				else if (strcmp(key, "language") == 0)
				{
					int langValue = atoi(value);
					// Handle special language mappings
					if (langValue == 10)
						m_vSettings[SETTING_LANGUAGE].currentValue = 0; // Console Default (index 0)
					else if (langValue == 0)
						m_vSettings[SETTING_LANGUAGE].currentValue = 6; // Japanese (index 6)
					else if (langValue == 6)
						m_vSettings[SETTING_LANGUAGE].currentValue = 7; // Korean (index 7)
					else if (langValue >= 1 && langValue <= 5)
						m_vSettings[SETTING_LANGUAGE].currentValue = langValue; // English, German, French, Spanish, Italian
					else
						m_vSettings[SETTING_LANGUAGE].currentValue = 0; // Default to Console Default (index 0)
				}
				else if (strcmp(key, "LoaderIOS") == 0 || strcmp(key, "cios") == 0)
				{
					int iosValue = atoi(value);
					// Convert IOS number (248-251) to index (0-3)
					if (iosValue >= 248 && iosValue <= 251)
						m_vSettings[SETTING_LOADERIOS_CIOS].currentValue = iosValue - 248;
					else
						m_vSettings[SETTING_LOADERIOS_CIOS].currentValue = 1; // Default to 249 (index 1)
				}
				else if (strcmp(key, "USBPort") == 0)
					m_vSettings[SETTING_USBPORT].currentValue = atoi(value);
				else if (strcmp(key, "MultiplePartitions") == 0)
					m_vSettings[SETTING_MULTIPLE_PARTITIONS].currentValue = atoi(value);
				else if (strcmp(key, "NandEmuMode") == 0)
					m_vSettings[SETTING_NANDEMUMODE].currentValue = atoi(value);
				else if (strcmp(key, "NandEmuPath") == 0)
				{
					// Parse path and set index (sd=0, usb=1)
					if (strncmp(value, "usb:", 4) == 0)
						m_vSettings[SETTING_NANDEMUPATH].currentValue = 1;
					else
						m_vSettings[SETTING_NANDEMUPATH].currentValue = 0;
				}
			}
		}

		line = nextLine + 1;
	}

	free(fileBuffer);
}

void CSettingsScene::LoadConfigXml()
{
	// Use the already-loaded values from main.cpp to avoid duplicate XML parsing
	m_vSettings[SETTING_LOAD_METHOD].currentValue = (int)loadMethod;
	m_vSettings[SETTING_AUTO_BOOT].currentValue = autoBoot ? 1 : 0;
	m_vSettings[SETTING_DEBUG_TO_FILE].currentValue = debugToFile ? 1 : 0;
}

void CSettingsScene::SaveSettings()
{
	if (codesBasePath[0] == '\0') {
		return;
	}

	// Save loader.cfg using Read-Modify-Write approach
	char configPath[ISFS_MAXPATH];
	snprintf(configPath, sizeof(configPath), "%s/launcher/loader.cfg", codesBasePath);

	std::vector<std::string> fileLines;
	bool fileExists = false;

	FILE *readFile = fopen(configPath, "r");
	if (readFile)
	{
		fileExists = true;
		char line[256];
		while (fgets(line, sizeof(line), readFile))
		{
			fileLines.push_back(std::string(line));
		}
		fclose(readFile);
	}

	const int numManagedKeys = 11;
	bool updatedKeys[numManagedKeys] = {false};
	const char* managedKeys[] = {
		"videomode",
		"videopatch",
		"deflicker",
		"language",
		"LoaderIOS",
		"cios",
		"MultiplePartitions",
		"USBPort",
		"NandEmuMode",
		"NandEmuPath",
		"GameAspectRatio"
	};

	const char* nandEmuPathValue = m_vSettings[SETTING_NANDEMUPATH].currentValue == 1 ? "usb:/nands/01/" : "sd:/nands/01/";

	int managedIntValues[numManagedKeys];
	managedIntValues[0] = m_vSettings[SETTING_VIDEO_MODE].currentValue;
	// Set videopatch = 2 (VIDEO_PATCH_DOL_ON) only when Force PAL video mode is set (3 or 6), otherwise videopatch = 0
	int videoModeValue = m_vSettings[SETTING_VIDEO_MODE].currentValue;
	if (videoModeValue == 3 || videoModeValue == 6) {
		managedIntValues[1] = 2; // Force PAL 480i or Force PAL 480p
	} else {
		managedIntValues[1] = 0; // Default for all other modes
	}
	managedIntValues[2] = m_vSettings[SETTING_DEFLICKER].currentValue;
	int langValue = m_vSettings[SETTING_LANGUAGE].currentValue;
	switch (langValue) {
		case 0:  managedIntValues[3] = 10; break; // Console Default (index 0)
		case 6:  managedIntValues[3] = 0; break;  // Japanese (index 6)
		case 7:  managedIntValues[3] = 6; break;  // Korean (index 7)
		default: managedIntValues[3] = langValue; break;
	}
	managedIntValues[4] = 248 + m_vSettings[SETTING_LOADERIOS_CIOS].currentValue;
	managedIntValues[5] = managedIntValues[4]; // cios uses same value as LoaderIOS
	managedIntValues[6] = m_vSettings[SETTING_MULTIPLE_PARTITIONS].currentValue;
	managedIntValues[7] = m_vSettings[SETTING_USBPORT].currentValue;
	managedIntValues[8] = m_vSettings[SETTING_NANDEMUMODE].currentValue;
	managedIntValues[9] = 0; // NandEmuPath is handled separately with string value
	managedIntValues[10] = m_vSettings[SETTING_GAME_ASPECT_RATIO].currentValue;

	for (size_t i = 0; i < fileLines.size(); i++)
	{
		std::string &line = fileLines[i];

		if (line.empty() || line[0] == '#' || line[0] == '\n' || line[0] == '\r')
			continue;

		size_t equalsPos = line.find('=');
		if (equalsPos != std::string::npos)
		{
			char keyBuffer[256];
			strncpy(keyBuffer, line.c_str(), equalsPos);
			keyBuffer[equalsPos] = '\0';

			char *key = keyBuffer;
			while (*key == ' ' || *key == '\t') key++;
			char *keyEnd = key + strlen(key) - 1;
			while (keyEnd > key && (*keyEnd == ' ' || *keyEnd == '\t' || *keyEnd == '\n' || *keyEnd == '\r'))
				*keyEnd-- = '\0';

			for (int j = 0; j < numManagedKeys; j++)
			{
				if (strcmp(key, managedKeys[j]) == 0)
				{
					if (j == 9) // NandEmuPath index
					{
						char newLine[256];
						snprintf(newLine, sizeof(newLine), "%s = %s\n", managedKeys[j], nandEmuPathValue);
						fileLines[i] = std::string(newLine);
					}
					else
					{
						char newLine[256];
						snprintf(newLine, sizeof(newLine), "%s = %d\n", managedKeys[j], managedIntValues[j]);
						fileLines[i] = std::string(newLine);
					}
					updatedKeys[j] = true;
					break;
				}
			}
		}
	}

	FILE *writeFile = fopen(configPath, "w");
	if (writeFile)
	{
		if (!fileExists)
		{
			fprintf(writeFile, "# Brawl Mod Loader\n");
			fprintf(writeFile, "# Note: This file is automatically generated\n");
		}

		for (size_t i = 0; i < fileLines.size(); i++)
		{
			fputs(fileLines[i].c_str(), writeFile);
		}

		for (int i = 0; i < numManagedKeys; i++)
		{
			if (!updatedKeys[i])
			{
				if (i == 9) // NandEmuPath index
					fprintf(writeFile, "%s = %s\n", managedKeys[i], nandEmuPathValue);
				else
					fprintf(writeFile, "%s = %d\n", managedKeys[i], managedIntValues[i]);
			}
		}

		fclose(writeFile);
	}

	snprintf(configPath, sizeof(configPath), "%s/launcher/config.xml", codesBasePath);

	XMLDocument doc;
	if (doc.LoadFile(configPath) == XML_SUCCESS)
	{
		XMLElement *root = doc.RootElement();
		if (root)
		{
			XMLElement *launcher = root->FirstChildElement("launcher");
			if (launcher)
			{
				XMLElement *config = launcher->FirstChildElement("config");
				if (!config)
				{
					config = doc.NewElement("config");
					launcher->InsertEndChild(config);
				}

				XMLElement *section = config->FirstChildElement("global");
				if (!section)
				{
					section = config->FirstChildElement("wii");
					if (!section)
					{
						section = doc.NewElement("global");
						config->InsertEndChild(section);
					}
				}

				XMLElement *elem = section->FirstChildElement("loadMethod");
				if (!elem)
				{
					elem = doc.NewElement("loadMethod");
					section->InsertEndChild(elem);
				}
				elem->DeleteChildren();
				const char *loadMethodValue = m_vSettings[SETTING_LOAD_METHOD].currentValue == 1 ? "disc" : m_vSettings[SETTING_LOAD_METHOD].currentValue == 2 ? "usb" : "auto";
				XMLText *loadMethodText = doc.NewText(loadMethodValue);
				elem->InsertEndChild(loadMethodText);

				elem = section->FirstChildElement("autoBoot");
				if (!elem)
				{
					elem = doc.NewElement("autoBoot");
					section->InsertEndChild(elem);
				}
				elem->DeleteChildren();
				const char *autoBootValue = m_vSettings[SETTING_AUTO_BOOT].currentValue == 1 ? "true" : "false";
				XMLText *autoBootText = doc.NewText(autoBootValue);
				elem->InsertEndChild(autoBootText);

				elem = section->FirstChildElement("debugToFile");
				if (!elem)
				{
					elem = doc.NewElement("debugToFile");
					section->InsertEndChild(elem);
				}
				elem->DeleteChildren();
				const char *debugValue = m_vSettings[SETTING_DEBUG_TO_FILE].currentValue == 1 ? "true" : "false";
				XMLText *debugText = doc.NewText(debugValue);
				elem->InsertEndChild(debugText);

				doc.SaveFile(configPath);
			}
		}
	}
	else
	{
		extern bool useGCPads;
		extern bool useWiiPads;
		extern bool useNetwork;
		extern bool useSFX;
		extern bool useMusic;

		XMLDeclaration *declaration = doc.NewDeclaration();
		doc.InsertEndChild(declaration);

		XMLElement *root = doc.NewElement("root");
		doc.InsertEndChild(root);
/*
		XMLElement *game = doc.NewElement("game");
		root->InsertEndChild(game);

		XMLElement *gameConfig = doc.NewElement("config");
		game->InsertEndChild(gameConfig);
*/
		XMLElement *launcher = doc.NewElement("launcher");
		root->InsertEndChild(launcher);

		XMLElement *config = doc.NewElement("config");
		launcher->InsertEndChild(config);

		XMLElement *global = doc.NewElement("global");
		config->InsertEndChild(global);

		XMLElement *elem = doc.NewElement("loadMethod");
		global->InsertEndChild(elem);
		const char *loadMethodValue = m_vSettings[SETTING_LOAD_METHOD].currentValue == 1 ? "disc" : m_vSettings[SETTING_LOAD_METHOD].currentValue == 2 ? "usb" : "auto";
		XMLText *loadMethodText = doc.NewText(loadMethodValue);
		elem->InsertEndChild(loadMethodText);

		elem = doc.NewElement("autoBoot");
		global->InsertEndChild(elem);
		XMLText *autoBootText = doc.NewText(m_vSettings[SETTING_AUTO_BOOT].currentValue == 1 ? "true" : "false");
		elem->InsertEndChild(autoBootText);

		elem = doc.NewElement("useGCPads");
		global->InsertEndChild(elem);
		XMLText *useGCPadsText = doc.NewText(useGCPads ? "true" : "false");
		elem->InsertEndChild(useGCPadsText);

		elem = doc.NewElement("useWiiPads");
		global->InsertEndChild(elem);
		XMLText *useWiiPadsText = doc.NewText(useWiiPads ? "true" : "false");
		elem->InsertEndChild(useWiiPadsText);

		elem = doc.NewElement("useNetwork");
		global->InsertEndChild(elem);
		XMLText *useNetworkText = doc.NewText(useNetwork ? "true" : "false");
		elem->InsertEndChild(useNetworkText);

		elem = doc.NewElement("useSoundEffects");
		global->InsertEndChild(elem);
		XMLText *useSoundEffectsText = doc.NewText(useSFX ? "true" : "false");
		elem->InsertEndChild(useSoundEffectsText);

		elem = doc.NewElement("useMusic");
		global->InsertEndChild(elem);
		XMLText *useMusicText = doc.NewText(useMusic ? "true" : "false");
		elem->InsertEndChild(useMusicText);

		elem = doc.NewElement("debugToFile");
		global->InsertEndChild(elem);
		XMLText *debugText = doc.NewText(m_vSettings[SETTING_DEBUG_TO_FILE].currentValue == 1 ? "true" : "false");
		elem->InsertEndChild(debugText);

		XMLElement *wii = doc.NewElement("wii");
		config->InsertEndChild(wii);

		XMLElement *dolphin = doc.NewElement("dolphin");
		config->InsertEndChild(dolphin);

		XMLComment *audioComment = doc.NewComment(" Use of Audio on Dolphin requires LLE ");
		dolphin->InsertEndChild(audioComment);

		elem = doc.NewElement("useSoundEffects");
		dolphin->InsertEndChild(elem);
		XMLText *dolphinSoundEffectsText = doc.NewText("false");
		elem->InsertEndChild(dolphinSoundEffectsText);

		elem = doc.NewElement("useMusic");
		dolphin->InsertEndChild(elem);
		XMLText *dolphinMusicText = doc.NewText("false");
		elem->InsertEndChild(dolphinMusicText);

		doc.SaveFile(configPath);
	}

	// Update global variables from main.cpp so changes are reflected in the main application
	loadMethod = (LoadMethod)m_vSettings[SETTING_LOAD_METHOD].currentValue;
	autoBoot = m_vSettings[SETTING_AUTO_BOOT].currentValue == 1;
	debugToFile = m_vSettings[SETTING_DEBUG_TO_FILE].currentValue == 1;
}

void CSettingsScene::CycleSettingValue(int index)
{
	if (index < 0 || index >= (int)m_vSettings.size())
		return;

	int originalValue = m_vSettings[index].currentValue;

	// Find next valid value (skipping removed options)
	do {
		m_vSettings[index].currentValue++;
		if (m_vSettings[index].currentValue >= (int)m_vSettings[index].options.size())
			m_vSettings[index].currentValue = 0;

		if (!m_vSettings[index].options[m_vSettings[index].currentValue].empty())
			break;

	} while (m_vSettings[index].currentValue != originalValue);

	// Show autoboot info popup when enabling autoboot
	if (m_vSettings[index].type == SETTING_AUTO_BOOT && 
		originalValue == 0 && m_vSettings[index].currentValue == 1)
	{
		showAutoBootInfoPopup = true;
	}
}

void CSettingsScene::ResetSettingsToDefaults()
{
	m_vSettings[SETTING_LOAD_METHOD].currentValue = 0;           // Auto
	m_vSettings[SETTING_AUTO_BOOT].currentValue = 0;             // OFF
	m_vSettings[SETTING_VIDEO_MODE].currentValue = 1;            // Disc Default
	m_vSettings[SETTING_DEFLICKER].currentValue = 0;             // Auto
	m_vSettings[SETTING_GAME_ASPECT_RATIO].currentValue = 2;     // System Default
	m_vSettings[SETTING_LANGUAGE].currentValue = 0;              // Console Default
	m_vSettings[SETTING_LOADERIOS_CIOS].currentValue = 1;      	 // 249
	m_vSettings[SETTING_USBPORT].currentValue = 0;               // USB Port 0
	m_vSettings[SETTING_MULTIPLE_PARTITIONS].currentValue = 0;   // OFF
	m_vSettings[SETTING_NANDEMUMODE].currentValue = 0;           // OFF
	m_vSettings[SETTING_NANDEMUPATH].currentValue = 0;           // SD
	m_vSettings[SETTING_DEBUG_TO_FILE].currentValue = 0;         // OFF

	SaveSettings();
}

const wchar_t* CSettingsScene::GetCurrentValueText(int index)
{
	if (index < 0 || index >= (int)m_vSettings.size())
		return L"";

	SSetting &setting = m_vSettings[index];
	if (setting.currentValue < 0 || setting.currentValue >= (int)setting.options.size())
		return L"";

	const char *value = setting.options[setting.currentValue].c_str();
	mbstowcs(m_wszValueBuffer, value, 64);
	return m_wszValueBuffer;
}

const char* CSettingsScene::GetDynamicDescription(int index)
{
	if (index < 0 || index >= (int)m_vSettings.size())
		return "";

	SSetting &setting = m_vSettings[index];
	int value = setting.currentValue;

	switch (setting.type)
	{
	case SETTING_LOAD_METHOD:
		switch (value) {
			case 0: snprintf(m_szDescriptionBuffer, 512, "Automatically load %s with disc or USB device.", projectName); break;
			case 1: snprintf(m_szDescriptionBuffer, 512, "Load %s with disc.", projectName); break;
			default: snprintf(m_szDescriptionBuffer, 512, "Load %s with USB device.", projectName); break;
		}
		break;

	case SETTING_AUTO_BOOT:
		switch (value) {
			case 0: snprintf(m_szDescriptionBuffer, 512, "Show launcher menu on startup."); break;
			default: snprintf(m_szDescriptionBuffer, 512, "Skip launcher menu on startup. Hold A during boot to access menu."); break;
		}
		break;

	case SETTING_VIDEO_MODE:
		switch (value)
		{
	//	case 0: snprintf(m_szDescriptionBuffer, 512, "Use system video mode from Wii settings."); break;
		case 1: snprintf(m_szDescriptionBuffer, 512, "Use Wii system video setting. 480i/480p with auto-region patch."); break;
	//	case 2: snprintf(m_szDescriptionBuffer, 512, "Force PAL 50Hz (576i) interlaced video mode."); break;
		case 3: snprintf(m_szDescriptionBuffer, 512, "Force PAL 60Hz (480i) interlaced video mode."); break;
		case 4: snprintf(m_szDescriptionBuffer, 512, "Force NTSC 480i interlaced video mode."); break;
	//	case 5: snprintf(m_szDescriptionBuffer, 512, "Automatically detect and patch video mode."); break;
		case 6: snprintf(m_szDescriptionBuffer, 512, "Force PAL 480p progressive scan. Requires component cables."); break;
		case 7: snprintf(m_szDescriptionBuffer, 512, "Force NTSC 480p progressive scan. Requires component cables."); break;
		default: snprintf(m_szDescriptionBuffer, 512, "%s", setting.description.c_str()); break;
		}
		break;

	case SETTING_DEFLICKER:
		switch (value)
		{
		case 0: snprintf(m_szDescriptionBuffer, 512, "Automatically determine deflicker filter based on video mode."); break;
		case 1: snprintf(m_szDescriptionBuffer, 512, "No deflicker filter (Safe). Use for progressive scan displays."); break;
		case 2: snprintf(m_szDescriptionBuffer, 512, "No deflicker filter (Extended). May cause issues on some displays."); break;
		case 3: snprintf(m_szDescriptionBuffer, 512, "Low deflicker filter. Slight reduction in flickering."); break;
		case 4: snprintf(m_szDescriptionBuffer, 512, "Medium deflicker filter. Balanced flickering reduction."); break;
		case 5: snprintf(m_szDescriptionBuffer, 512, "High deflicker filter. Maximum flickering reduction for interlaced."); break;
		default: snprintf(m_szDescriptionBuffer, 512, "%s", setting.description.c_str()); break;
		}
		break;

	case SETTING_GAME_ASPECT_RATIO:
		switch (value)
		{
		case 0: snprintf(m_szDescriptionBuffer, 512, "Force 4:3 aspect ratio."); break;
		case 1: snprintf(m_szDescriptionBuffer, 512, "Force 16:9 widescreen aspect ratio."); break;
		case 2: snprintf(m_szDescriptionBuffer, 512, "Use aspect ratio from Wii system settings."); break;
		default: snprintf(m_szDescriptionBuffer, 512, "%s", setting.description.c_str()); break;
		}
		break;

	case SETTING_LANGUAGE:
		switch (value)
		{
		case 0: snprintf(m_szDescriptionBuffer, 512, "Set game language from Wii system settings."); break;
		case 1: snprintf(m_szDescriptionBuffer, 512, "Set game language to English."); break;
		case 2: snprintf(m_szDescriptionBuffer, 512, "Set game language to German."); break;
		case 3: snprintf(m_szDescriptionBuffer, 512, "Set game language to French."); break;
		case 4: snprintf(m_szDescriptionBuffer, 512, "Set game language to Spanish."); break;
		case 5: snprintf(m_szDescriptionBuffer, 512, "Set game language to Italian."); break;
		case 6: snprintf(m_szDescriptionBuffer, 512, "Set game language to Japanese."); break;
		case 7: snprintf(m_szDescriptionBuffer, 512, "Set game language to Korean."); break;
		default: snprintf(m_szDescriptionBuffer, 512, "%s", setting.description.c_str()); break;
		}
		break;

	case SETTING_LOADERIOS_CIOS:
		snprintf(m_szDescriptionBuffer, 512, "Use custom IOS %d for game loading. Required for USB loading.", 248 + value);
		break;

	case SETTING_USBPORT:
		switch (value)
		{
		case 0: snprintf(m_szDescriptionBuffer, 512, "Use USB port 0 for loading."); break;
		case 1: snprintf(m_szDescriptionBuffer, 512, "Use USB port 1 for loading."); break;
		case 2: snprintf(m_szDescriptionBuffer, 512, "Search both USB ports for game."); break;
		default: snprintf(m_szDescriptionBuffer, 512, "%s", setting.description.c_str()); break;
		}
		break;

	case SETTING_MULTIPLE_PARTITIONS:
		switch (value) {
			case 0: snprintf(m_szDescriptionBuffer, 512, "Only use the first partition on USB device."); break;
			default: snprintf(m_szDescriptionBuffer, 512, "Search multiple partitions on USB device for game."); break;
		}
		break;

	case SETTING_NANDEMUMODE:
		switch (value)
		{
		case 0: snprintf(m_szDescriptionBuffer, 512, "NAND emulation disabled. Use real Wii NAND."); break;
		case 1: snprintf(m_szDescriptionBuffer, 512, "Partial NAND emulation. Emulate save files only. USB loading only."); break;
		case 2: snprintf(m_szDescriptionBuffer, 512, "Full NAND emulation. Complete NAND redirect. USB loading only."); break;
		default: snprintf(m_szDescriptionBuffer, 512, "%s", setting.description.c_str()); break;
		}
		break;

	case SETTING_NANDEMUPATH:
		switch (value) {
			case 0: snprintf(m_szDescriptionBuffer, 512, "Use SD card for NAND emulation: sd:/nands/01/"); break;
			default: snprintf(m_szDescriptionBuffer, 512, "Use USB device for NAND emulation: usb:/nands/01/"); break;
		}
		break;

	case SETTING_DEBUG_TO_FILE:
		switch (value) {
			case 0: snprintf(m_szDescriptionBuffer, 512, "No debug logging."); break;
			default: snprintf(m_szDescriptionBuffer, 512, "Write debug information to sd:/debug.txt"); break;
		}
		break;

	case SETTING_RESET_TO_DEFAULTS:
		snprintf(m_szDescriptionBuffer, 512, "Reset all settings to their default values.");
		break;

	default:
		snprintf(m_szDescriptionBuffer, 512, "%s", setting.description.c_str());
		break;
	}

	return m_szDescriptionBuffer;
}

void CSettingsScene::HandleInputs(u32 gcPressed, s8 stickX, s8 stickY, s8 substickX, s8 substickY, u32 wiiPressed, bool powerButtonPressed, bool resetButtonPressed)
{
	if (m_iDrawFrameNumber < 30 || m_iMenuSelectedIndex == -2)
		return;

	if ((gcPressed & PAD_BUTTON_B) || (wiiPressed & WPAD_BUTTON_B) || (powerButtonPressed && m_iMenuSelectedIndex == (int)m_vSettings.size() - 1))
	{
		SaveSettings();
		m_eNextScreen = SCENE_MAIN_MENU;
		playSFX(SFX_BACK);
		return;
	}

	if (codesBasePath[0] == '\0') {
		return;
	}

	if (showAutoBootInfoPopup)
	{
		if ((gcPressed & PAD_BUTTON_A) || (wiiPressed & WPAD_BUTTON_A) || resetButtonPressed)
		{
			if (autoBootInfoPopup->getSelectedIndex() == 0)
			{
				showAutoBootInfoPopup = false;
				autoBootInfoPopup->reset();
				playSFX(SFX_CONFIRM);
			}
		}
		else if ((gcPressed & PAD_BUTTON_B) || (wiiPressed & WPAD_BUTTON_B))
		{
			showAutoBootInfoPopup = false;
			autoBootInfoPopup->reset();
			playSFX(SFX_BACK);
		}
		return;
	}

	bool stickMovedUp = false;
	bool stickMovedDown = false;

	if (stickY > STICK_DEADZONE && m_sPrevDStickY <= STICK_DEADZONE)
		stickMovedUp = true;
	if (stickY < -STICK_DEADZONE && m_sPrevDStickY >= -STICK_DEADZONE)
		stickMovedDown = true;

	if ((gcPressed & PAD_BUTTON_UP) || (wiiPressed & WPAD_BUTTON_UP) || stickMovedUp)
	{
		if (m_iMenuSelectedIndex == 0)
			m_iMenuSelectedIndex = m_vSettings.size() - 1;
		else
			m_iMenuSelectedIndex--;
		m_iMenuSelectionFrame = 0;
		playSFX(SFX_SELECT);
	}

	if ((gcPressed & PAD_BUTTON_DOWN) || (wiiPressed & WPAD_BUTTON_DOWN) || stickMovedDown || powerButtonPressed)
	{
		if (m_iMenuSelectedIndex == (int)m_vSettings.size() - 1)
			m_iMenuSelectedIndex = 0;
		else
			m_iMenuSelectedIndex++;
		m_iMenuSelectionFrame = 0;
		playSFX(SFX_SELECT);
	}

	if ((gcPressed & PAD_BUTTON_A) || (wiiPressed & WPAD_BUTTON_A) || resetButtonPressed)
	{
		if (m_iMenuSelectedIndex >= 0 && m_iMenuSelectedIndex < (int)m_vSettings.size() &&
			m_vSettings[m_iMenuSelectedIndex].type == SETTING_RESET_TO_DEFAULTS)
		{
			ResetSettingsToDefaults();
			showResetMessage = true;
			playSFX(SFX_CONFIRM);
		}
		else
		{
			CycleSettingValue(m_iMenuSelectedIndex);
			playSFX(SFX_SELECT);
		}
	}

	m_sPrevDStickX = stickX;
	m_sPrevDStickY = stickY;
}

void CSettingsScene::Draw()
{
	if (!m_bIsLoaded)
		return;

	f32 yPos = 46.0f;
	f32 offset = 0.0F;
	if (m_iDrawFrameNumber < 15)
		offset = 11.5f * (m_iDrawFrameNumber - 15);

	drawNewsBox(yPos, offset, 65);

	yPos += 20.0f;

	drawSelectionMenu(yPos - 5);
	yPos += 288.0f;

	if (m_iDrawFrameNumber < 15)
		offset = ((yPos - m_fScreenHeight) / 10.0f) * (m_iDrawFrameNumber - 15);
	else
		offset = 0.0f;

	if (codesBasePath[0] == '\0')
	{
		swprintf(m_wszInfoText, 512, L"No codes path specified in meta.xml.");
	}
	else if (m_iMenuSelectedIndex >= 0 && m_iMenuSelectedIndex < (int)m_vSettings.size())
	{
		if (showResetMessage && m_vSettings[m_iMenuSelectedIndex].type == SETTING_RESET_TO_DEFAULTS)
		{
			swprintf(m_wszInfoText, 512, L"Settings have been reset to their default values.");
		}
		else
		{
			const char* description = GetDynamicDescription(m_iMenuSelectedIndex);
			mbstowcs(m_wszInfoText, description, 512);

			if (m_vSettings[m_iMenuSelectedIndex].type != SETTING_RESET_TO_DEFAULTS)
			{
				showResetMessage = false;
			}
		}
	}

	drawInfoBox(yPos + offset, 36.0F, m_wszInfoText);

	if (showAutoBootInfoPopup)
		autoBootInfoPopup->draw();

	if (m_iDrawFrameNumber <= 30)
		m_iDrawFrameNumber++;
}

void CSettingsScene::drawSelectionMenu(float yPos)
{
	f32 animationFrame = (f32)m_iDrawFrameNumber;
	f32 animationFrames = 20.0f;
	if (animationFrame > animationFrames)
		animationFrame = animationFrames;

	f32 animationRatio = animationFrame / animationFrames;
	animationRatio = (1.0f - (1.0f - animationRatio) * (1.0f - animationRatio));

	f32 initialSizeRatio = 0.10f;
	f32 widthRatio = 0.85f;
	f32 heightRatio = 1.00f;

	f32 baseWidth = m_fScreenWidth * widthRatio;
	f32 finalHeight = 280 * heightRatio;

	UpdateScrollbarState(yPos, finalHeight);

	f32 scrollbarWidth = m_bShowScrollbar ? 12.0f : 0.0f;
	f32 finalWidth = baseWidth - scrollbarWidth;

	f32 xPos = (m_fScreenWidth * 0.50f) * (1.0f - (widthRatio * initialSizeRatio) - (widthRatio * (1.0f - initialSizeRatio) * animationRatio));
	yPos += (finalHeight * 0.50f) * (1.0f - (heightRatio * initialSizeRatio) - (heightRatio * (1.0f - initialSizeRatio) * animationRatio));

	f32 width = initialSizeRatio * finalWidth + ((1.0f - initialSizeRatio) * finalWidth * animationRatio);
	f32 height = initialSizeRatio * finalHeight + ((1.0f - initialSizeRatio) * finalHeight * animationRatio);

	Menu_DrawRectangle(xPos, yPos, width, height, (GXColor){0, 0, 0, u8(178 * animationRatio)}, true);

	int separatorCount = 5;
	f32 rowHeight = height / (f32)separatorCount;

	ChangeFontSize(20);
	if (!fontSystem[20])
		fontSystem[20] = new FreeTypeGX(20);

	if (m_iMenuSelectedIndex >= 0)
	{
		int initialPosition = m_iMenuSelectedIndex / separatorCount;
		initialPosition *= 5;
		int modPosition = m_iMenuSelectedIndex % separatorCount;
		int endPosition = initialPosition + 5;
		if (endPosition > (int)m_vSettings.size())
			endPosition = m_vSettings.size();

		for (int i = initialPosition; i < endPosition; i++)
		{
			int rowIndex = i % separatorCount;

			if (m_iMenuSelectedIndex == i && m_iDrawFrameNumber >= 20)
			{
				Menu_DrawRectangle(xPos, yPos + ((f32)rowIndex * height / (f32)separatorCount),
								   width, rowHeight,
								   (GXColor){selectionColor.r, selectionColor.g, selectionColor.b, u8(255 * animationRatio)}, true);
			}

			wchar_t wszName[128];
			mbstowcs(wszName, m_vSettings[i].name.c_str(), 128);

			GXColor textColor = (m_iMenuSelectedIndex == i && m_iDrawFrameNumber >= 20) ?
								(GXColor){0x00, 0x00, 0x00, 0xff} :
								(GXColor){0xff, 0xff, 0xff, 0xff};

			fontSystem[20]->drawText(xPos + (width * 0.01f),
									 yPos + ((f32)rowIndex * height / (f32)separatorCount) + (rowHeight * 0.5f),
									 wszName,
									 textColor,
									 FTGX_JUSTIFY_LEFT | FTGX_ALIGN_MIDDLE);

			const wchar_t *valueText = GetCurrentValueText(i);
			f32 valueWidth = fontSystem[20]->getWidth(valueText);
			fontSystem[20]->drawText(xPos + width - (width * 0.01f) - valueWidth,
									 yPos + ((f32)rowIndex * height / (f32)separatorCount) + (rowHeight * 0.5f),
									 valueText,
									 textColor,
									 FTGX_JUSTIFY_LEFT | FTGX_ALIGN_MIDDLE);
		}
	}

	for (int i = 1; i < separatorCount; i++)
	{
		Menu_DrawRectangle(xPos, yPos + ((f32)i * height / (f32)separatorCount),
						   width, 1,
						   (GXColor){255, 255, 255, u8(255 * animationRatio)}, true);
	}

	Menu_DrawRectangle(xPos, yPos, width, height, (GXColor){255, 255, 255, u8(255 * animationRatio)}, false);

	UpdateScrollbarState(yPos, height);
	drawScrollbar(xPos, yPos, width, height, animationRatio);
}

void CSettingsScene::UpdateScrollbarState(f32 menuY, f32 menuHeight)
{
	m_bShowScrollbar = (m_vSettings.size() > 5);

	if (!m_bShowScrollbar)
	{
		m_fScrollbarHeight = 0.0f;
		m_fScrollbarY = 0.0f;
		return;
	}

	f32 visibleRatio = 5.0f / (f32)m_vSettings.size();
	m_fScrollbarHeight = menuHeight * visibleRatio;

	if (m_fScrollbarHeight < 10.0f)
		m_fScrollbarHeight = 10.0f;

	if (m_iMenuSelectedIndex >= 0)
	{
		int currentPage = m_iMenuSelectedIndex / 5;
		int totalPages = (m_vSettings.size() + 4) / 5;

		if (currentPage >= totalPages)
			currentPage = totalPages - 1;

		f32 scrollableHeight = menuHeight - m_fScrollbarHeight;
		f32 scrollPosition = (f32)currentPage / (f32)(totalPages - 1);

		m_fScrollbarY = menuY + (scrollPosition * scrollableHeight);
	}
	else
	{
		m_fScrollbarY = menuY;
	}
}

void CSettingsScene::drawScrollbar(f32 menuX, f32 menuY, f32 menuWidth, f32 menuHeight, f32 animationRatio)
{
	if (!m_bShowScrollbar || animationRatio <= 0.0f)
		return;

	f32 originalMenuWidth = menuWidth + 12.0f;

	f32 scrollbarWidth = 12.0f;
	f32 scrollbarX = menuX + originalMenuWidth - scrollbarWidth;

	Menu_DrawRectangle(scrollbarX, menuY, scrollbarWidth, menuHeight, 
					   (GXColor){255, 255, 255, u8(255 * animationRatio)}, false);

	Menu_DrawRectangle(scrollbarX, m_fScrollbarY, scrollbarWidth, m_fScrollbarHeight,
					   (GXColor){255, 255, 255, u8(255 * animationRatio)}, true);
}
