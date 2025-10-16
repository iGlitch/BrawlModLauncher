#include <stdlib.h>
#include <math.h>
#include <gccore.h>
#include <unistd.h>
#include <fat.h>
#include <ogcsys.h>
#include <string.h>
#include <wiiuse/wpad.h>
#include <dirent.h>
#include <vector>
#include <memory>


#include "video.h"
#include "tex_logo_tpl.h"
#include "tex_logo.h"
#include "UpdateScene.h"
#include "FreeTypeGX.h"
#include "textures.h"

#include "..\Network\http.h"
#include "..\Network\networkloader.h"
#include "..\Audio\sfx.h"
#include "..\Patching\Patcher.h"
#include "..\Patching\tinyxml2.h"
#include "..\Patching\md5.h"
#include "..\Patching\7z\CreateSubfolder.h"
#include "..\Patching\7z\7ZipFile.h"
#include "..\IOSLoader\sys.h"
#include "..\FileHolder.h"
#include "..\Common.h"

extern f32 g_LauncherVersion;

bool setDownloadProgress(void *obj, int size, int position);

CUpdateScene::CUpdateScene(f32 w, f32 h)
{

	m_eNextScreen = SCENE_UPDATE;

	m_fScreenWidth = w;
	m_fScreenHeight = h;
	m_bIsLoaded = false;
	m_iMenuSelectionAnimationFrames = 15;
	m_fMaxNewsScrollFrames = 1800.0F;
	m_fCurrentNewsScrollFrame = -1.0F;

	fProgressPercentage = 0.0F;
	/*FreeTypeGX *tmpFontSystem = new FreeTypeGX(14, GX_TF_IA8);
	fontSystem = tmpFontSystem;

	//fontSystem->loadFont(vera_bold_ttf, vera_bold_ttf_size, fontSize, true);	// Initialize the font system with the font parameters from rursus_compact_mono_ttf.h*/
};

CUpdateScene::~CUpdateScene()
{
}

void CUpdateScene::Load()
{
	m_iMenuSelectedIndex = -1;
	m_iMenuSelectionFrame = 0;
	m_iDrawFrameNumber = 0;
	m_sPrevDStickX = s8(0);
	m_sPrevDStickY = s8(0);
	m_eNextScreen = SCENE_UPDATE;

	showInfoFileMissingPopup = false;
	infoFileMissingPopup = new Popup(m_fScreenWidth, m_fScreenHeight, 0.90f, 0.55f, 0.40f, 0.75f);
	infoFileMissingPopup->setAnimationFrames(15, true);
	infoFileMissingPopup->setSelectionTextItems(0, 2, L"Yes", L"No");
	infoFileMissingPopup->setLineTextItems(3, L"The Project+ info.xml file was not", L"found. Do you have an unmodified ", L"installation of Project+ 2.2?");

	showCancelPopup = false;
	cancelPopup = new Popup(m_fScreenWidth, m_fScreenHeight, 0.90f, 0.55f, 0.40f, 0.75f);
	cancelPopup->setAnimationFrames(15, true);
	cancelPopup->setSelectionTextItems(1, 2, L"Yes", L"No");
	cancelPopup->setLineTextItems(2, L"Are you sure you want to", L"cancel the update process?");

	showNoUpdatePopup = false;
	noUpdatePopup = new Popup(m_fScreenWidth, m_fScreenHeight, 0.90f, 0.55f, 0.40f, 0.75f);
	noUpdatePopup->setAnimationFrames(15, true);
	noUpdatePopup->setSelectionTextItems(0, 1, L"OK");
	noUpdatePopup->setLineTextItems(1, L"No updates available.");

	showConfirmUpdatePopup = false;
	confirmUpdatePopup = new Popup(m_fScreenWidth, m_fScreenHeight, 0.90f, 0.55f, 0.40f, 0.75f);
	confirmUpdatePopup->setAnimationFrames(15, true);
	confirmUpdatePopup->setSelectionTextItems(0, 2, L"Yes", L"No");
	confirmUpdateLine3Text = (wchar_t*)malloc(sizeof(wchar_t)* 50);
	memset(confirmUpdateLine3Text, 0, sizeof(wchar_t)* 50);
	confirmUpdatePopup->setLineTextItems(3, L"An update is available. Would you", L"like to perform this update now?", confirmUpdateLine3Text);

	showConfirmLaunchPopup = false;
	confirmLaunchPopup = new Popup(m_fScreenWidth, m_fScreenHeight, 0.90f, 0.55f, 0.40f, 0.75f);
	confirmLaunchPopup->setAnimationFrames(15, true);
	confirmLaunchPopup->setSelectionTextItems(0, 2, L"Yes", L"No");
	confirmLaunchPopup->setLineTextItems(3, L"The launcher update was successfully", L"installed. Do you want to restart", L"the launcher now? (Recommended)");

	showPatchVerificationFailedPopup = false;
	patchVerificationFailedPopup = new Popup(m_fScreenWidth, m_fScreenHeight, 0.90f, 0.55f, 0.40f, 0.75f);
	patchVerificationFailedPopup->setAnimationFrames(15, true);
	patchVerificationFailedPopup->setSelectionTextItems(1, 2, L"Yes", L"No");
	patchVerificationFailedPopup->setLineTextItems(4,
		L"The update verification failed. This",
		L"can happen if your installation is",
		L"modified. Do you want force the",
		L"installation? (NOT RECOMMENDED)");

	m_bCancelUpdate = false;
	m_bRestartLauncher = false;
	m_bInstallUpdate = false;
	m_bForcePMVersion300 = false;


	sInfoText = malloc(sizeof(wchar_t)* 255);
	memset(sInfoText, 0, sizeof(wchar_t)* 255);

	fProgressPercentage = 0.0f;
};

void CUpdateScene::Unload()
{
	m_iDrawFrameNumber = 0;
	m_sPrevDStickX = s8(0);
	m_sPrevDStickY = s8(0);

	delete infoFileMissingPopup;
	delete cancelPopup;
	delete noUpdatePopup;
	delete confirmUpdatePopup;
	delete confirmLaunchPopup;
	delete confirmUpdateLine3Text;
	delete patchVerificationFailedPopup;
	delete sInfoText;

};

void CUpdateScene::HandleInputs(u32 gcPressed, s8 dStickX, s8 dStickY, s8 cStickX, s8 cStickY, u32 wiiPressed)
{
	if (m_bCancelUpdate)
		return;
	if (m_iDrawFrameNumber < 15)
		return;


	if ((dStickX < -STICK_DEADZONE && m_sPrevDStickX >= -STICK_DEADZONE) || gcPressed & PAD_BUTTON_LEFT || wiiPressed & WPAD_BUTTON_LEFT)
	{
		if (showCancelPopup && cancelPopup->decrementSelection())
			playSFX(SFX_SELECT);
		else if (showInfoFileMissingPopup && infoFileMissingPopup->decrementSelection())
			playSFX(SFX_SELECT);
		else if (showConfirmLaunchPopup && confirmLaunchPopup->decrementSelection())
			playSFX(SFX_SELECT);
		else if (showConfirmUpdatePopup&& confirmUpdatePopup->decrementSelection())
			playSFX(SFX_SELECT);
		else if (showPatchVerificationFailedPopup&& patchVerificationFailedPopup->decrementSelection())
			playSFX(SFX_SELECT);



	}

	if ((dStickX > STICK_DEADZONE && m_sPrevDStickX <= STICK_DEADZONE) || gcPressed & PAD_BUTTON_RIGHT || wiiPressed & WPAD_BUTTON_RIGHT)
	{
		if (showCancelPopup && cancelPopup->incrementSelection())
			playSFX(SFX_SELECT);
		else if (showInfoFileMissingPopup && infoFileMissingPopup->incrementSelection())
			playSFX(SFX_SELECT);
		else if (showConfirmLaunchPopup && confirmLaunchPopup->incrementSelection())
			playSFX(SFX_SELECT);
		else if (showConfirmUpdatePopup && confirmUpdatePopup->incrementSelection())
			playSFX(SFX_SELECT);
		else if (showPatchVerificationFailedPopup && patchVerificationFailedPopup->incrementSelection())
			playSFX(SFX_SELECT);

	}


	if (gcPressed & PAD_BUTTON_A || wiiPressed & WPAD_BUTTON_A || gcPressed & PAD_BUTTON_START)
	{
		if (showCancelPopup)
		{
			if (cancelPopup->getSelectedIndex() == 1)
			{
				showCancelPopup = false;
				cancelPopup->reset();
				playSFX(SFX_CONFIRM);

			}
			else if (cancelPopup->getSelectedIndex() == 0)
			{
				m_eNextScreen = SCENE_MAIN_MENU;
				m_bCancelUpdate = true;
				playSFX(SFX_CONFIRM);
			}
		}
		else if (showInfoFileMissingPopup)
		{
			switch (infoFileMissingPopup->getSelectedIndex())
			{
			default:
			case -1:
				break;
			case 0:
				m_bForcePMVersion300 = true;
				showInfoFileMissingPopup = false;
				infoFileMissingPopup->reset();
				playSFX(SFX_CONFIRM);
				break;
			case 1:
				showInfoFileMissingPopup = false;
				infoFileMissingPopup->reset();
				playSFX(SFX_CONFIRM);
				break;
			}
		}
		else if (showConfirmLaunchPopup)
			switch (confirmLaunchPopup->getSelectedIndex())
		{
			default:
			case -1:
				break;
			case 0:
				m_eNextScreen = SCENE_LAUNCHELF;
				m_bRestartLauncher = true;
				showConfirmLaunchPopup = false;
				//confirmLaunchPopup->reset();
				playSFX(SFX_CONFIRM);
				break;
			case 1:
				showConfirmLaunchPopup = false;
				confirmLaunchPopup->reset();
				playSFX(SFX_CONFIRM);
				break;
		}
		else if (showConfirmUpdatePopup)
			switch (confirmUpdatePopup->getSelectedIndex())
		{
			default:
			case -1:
				break;
			case 0: //Yes
				m_bInstallUpdate = true;
				showConfirmUpdatePopup = false;
				confirmUpdatePopup->reset();
				playSFX(SFX_CONFIRM);
				break;
			case 1: //No
				m_bInstallUpdate = false;
				showConfirmUpdatePopup = false;
				confirmUpdatePopup->reset();
				playSFX(SFX_CONFIRM);
		}
		else if (showNoUpdatePopup && noUpdatePopup->getSelectedIndex() == 0)
		{
			//m_eNextScreen = SCENE_MAIN_MENU;
			//m_bCancelUpdate = true;
			showNoUpdatePopup = false;
			noUpdatePopup->reset();
			playSFX(SFX_CONFIRM);
		}
		else if (showPatchVerificationFailedPopup)
			switch (patchVerificationFailedPopup->getSelectedIndex())
		{
			default:
			case -1:
				break;
			case 0: //Yes
				m_bInstallUpdate = true;
				showPatchVerificationFailedPopup = false;
				patchVerificationFailedPopup->reset();
				playSFX(SFX_CONFIRM);
				break;
			case 1: //No
				m_bInstallUpdate = false;
				showPatchVerificationFailedPopup = false;
				patchVerificationFailedPopup->reset();
				playSFX(SFX_CONFIRM);
		}


	}

	if (gcPressed & PAD_BUTTON_B || wiiPressed & WPAD_BUTTON_B)
	{
		if (showInfoFileMissingPopup)
		{
			showInfoFileMissingPopup = false;
			infoFileMissingPopup->reset();
			playSFX(SFX_BACK);
		}
		else if (showConfirmLaunchPopup)
		{
			showConfirmLaunchPopup = false;
			confirmLaunchPopup->reset();
			//m_eNextScreen = SCENE_MAIN_MENU;
			//m_bCancelUpdate = true;
			playSFX(SFX_BACK);
		}
		else if (showConfirmUpdatePopup)
		{
			//m_eNextScreen = SCENE_MAIN_MENU;
			//m_bCancelUpdate = true;
			showConfirmUpdatePopup = false;
			confirmUpdatePopup->reset();
			playSFX(SFX_BACK);
		}
		else if (showNoUpdatePopup)
		{
			showNoUpdatePopup = false;
			noUpdatePopup->reset();
			//m_eNextScreen = SCENE_MAIN_MENU;
			//m_bCancelUpdate = true;
			playSFX(SFX_BACK);
		}
		else if (showPatchVerificationFailedPopup)
		{
			m_bInstallUpdate = false;
			showPatchVerificationFailedPopup = false;
			patchVerificationFailedPopup->reset();
			playSFX(SFX_BACK);
		}
		else if (showCancelPopup)
		{
			showCancelPopup = false;
			cancelPopup->reset();
			playSFX(SFX_BACK);
		}

		else
		{
			showCancelPopup = true;
			playSFX(SFX_BACK);
		}
	}

	m_sPrevDStickX = dStickX;
	m_sPrevDStickY = dStickY;

}

void CUpdateScene::Draw()
{
	f32 yPos = 46.0f;
	f32 offset = 0.0F;
	if (m_iDrawFrameNumber < 15)
		offset = 11.5f * (m_iDrawFrameNumber - 15);


	drawNewsBox(yPos, offset, 65);

	yPos += 20.0f;

	drawProgressBar();

	yPos += 288.0f;

	if (m_iDrawFrameNumber < 15)
		offset = ((yPos - m_fScreenHeight) / 10.0f) * (m_iDrawFrameNumber - 15);
	else
		offset = 0.0f;

	drawInfoBox(yPos + offset, 36.0F, sInfoText);

	if (showInfoFileMissingPopup)
		infoFileMissingPopup->draw();

	if (showConfirmLaunchPopup)
		confirmLaunchPopup->draw();

	if (showConfirmUpdatePopup)
		confirmUpdatePopup->draw();

	if (showNoUpdatePopup)
		noUpdatePopup->draw();

	if (showCancelPopup)
		cancelPopup->draw();

	if (showPatchVerificationFailedPopup)
		patchVerificationFailedPopup->draw();

	if (m_iDrawFrameNumber <= 90)
		m_iDrawFrameNumber++;

}

void CUpdateScene::SleepDuringWork(int milliseconds)
{
	for (int i = 0; i < milliseconds; i++)
	{
		usleep(1000);
		if (m_bCancelUpdate)
			return;
	}
}

bool CUpdateScene::Work()
{
	//if (showInfoFileMissingPopup)return false;

	//if (showConfirmLaunchPopup)return false;
	m_eNextScreen = SCENE_MAIN_MENU;
	swprintf(sInfoText, 255, L"Reading local info.xml file...");
	//Look for sd:/Project+/info.xml
	f32 pmCurrentVersion = 0.0f;
	f32 launcherCurrentVersion = g_LauncherVersion;

	tinyxml2::XMLDocument infoDoc;
	if (infoFileSize != 0 && infoDoc.Parse((char *)infoFileData, infoFileSize) == (int)tinyxml2::XML_NO_ERROR)
	{
		tinyxml2::XMLElement* cur = infoDoc.RootElement();
		if (cur)
		{
			cur = cur->FirstChildElement("game");
			if (cur)
			{
				cur = cur->FirstChildElement("version");
				if (cur && cur->FirstChild() && cur->FirstChild()->ToText())
				{
					const char* text = cur->FirstChild()->ToText()->Value();
					pmCurrentVersion = (f32)atof(text);
				}
			}
		}
	}
	if (pmCurrentVersion == 0.0F)
	{
		m_bForcePMVersion300 = false;
		showInfoFileMissingPopup = true;
		while (showInfoFileMissingPopup)
			SleepDuringWork(1);
		if (m_bForcePMVersion300)
			pmCurrentVersion = 3.0f;
	}

	swprintf(sInfoText, 255, L"Searching for local updates...");
	char pmUpdateFileName[255] = "";
	char launcherUpdateFileName[255] = "";

	f32 pmUpdateVersion = 0.0f;

	f32 launcherUpdateVersion = 0.0f;
	do
	{
		pmUpdateVersion = 0.0f;
		launcherUpdateVersion = 0.0f;
		struct dirent *pent;
		struct stat statbuf;
		DIR * pmUpdateFolder = opendir("sd:/Project+/launcher/updates/");
		while ((pent = readdir(pmUpdateFolder)) != NULL) {
			stat(pent->d_name, &statbuf);
			if (strcmp(".", pent->d_name) == 0 ||
				strcmp("..", pent->d_name) == 0)
				continue;
			if (S_ISDIR(statbuf.st_mode) == 0)
				continue;

			int len = strlen(pent->d_name);
			char filename[255];
			strcpy(filename, pent->d_name);

			if (strcasecmp(filename + len - 3, ".7z") != 0) //check extension
				continue;

			filename[len - 3] = '\0'; //strip extension;

			char * tmpFileName = strtok(filename, "-");

			if (!tmpFileName)
				continue;

			if (pmCurrentVersion != 0.0f && strcasecmp(tmpFileName, "pmupdate") == 0)
			{
				//pmupdate-3.00-3.01.7z
				tmpFileName = strtok(NULL, "-");
				f32 tmpCurrentversion = (f32)atof(tmpFileName);
				if (!tmpFileName || tmpCurrentversion != pmCurrentVersion)
					continue;

				tmpFileName = strtok(NULL, "-");

				if (!tmpFileName)
					continue;

				f32 tmpNextVersion = (f32)atof(tmpFileName);
				if (tmpNextVersion <= pmUpdateVersion)
					continue;

				pmUpdateVersion = tmpNextVersion;
				sprintf(pmUpdateFileName, pent->d_name);
			}
			else if (strcasecmp(tmpFileName, "launcher") == 0)
			{
				//launcher-1.5.7z
				tmpFileName = strtok(NULL, "-");

				if (!tmpFileName)
					continue;

				f32 tmpNextVersion = (f32)atof(tmpFileName);
				
				if (tmpNextVersion <= launcherCurrentVersion || tmpNextVersion <= launcherUpdateVersion)
					continue;

				launcherUpdateVersion = tmpNextVersion;
				sprintf(launcherUpdateFileName, pent->d_name);
			}

		}
		if (pmUpdateFolder)
			closedir(pmUpdateFolder);
		//delete pmUpdateFolder;

		if (launcherUpdateVersion != 0.0f)
		{
			swprintf(sInfoText, 255, L"Local update found: %s", launcherUpdateFileName);
			swprintf(confirmUpdateLine3Text, 50, L"Launcher: v%4.2f -> v%4.2f", launcherCurrentVersion, launcherUpdateVersion);
			m_bInstallUpdate = false;
			showConfirmUpdatePopup = true;
			while (showConfirmUpdatePopup)
				SleepDuringWork(1);
			if (m_bInstallUpdate)
			{
				char fullFilePath[255];
				sprintf(fullFilePath, "sd:/Project+/launcher/updates/%s", launcherUpdateFileName);


				if (m_bCancelUpdate)
					return false;
				PMPatch(fullFilePath, sInfoText, m_bCancelUpdate, fProgressPercentage);
				if (m_bCancelUpdate)
					return false;

				launcherCurrentVersion = launcherUpdateVersion;

				m_bRestartLauncher = false;
				showConfirmLaunchPopup = true;
				while (showConfirmLaunchPopup)
					SleepDuringWork(1);
				fProgressPercentage = 0.0f;
				if (m_bRestartLauncher)
					return true;
			}
			else
			{
				launcherUpdateVersion = 0.0f;
			}


		}


		if (pmUpdateVersion != 0.0f)
		{
			swprintf(sInfoText, 255, L"Local update found: %s", pmUpdateFileName);
			swprintf(confirmUpdateLine3Text, 50, L"Project+: v%4.2f -> v%4.2f", pmCurrentVersion, pmUpdateVersion);
			m_bInstallUpdate = false;
			showConfirmUpdatePopup = true;
			while (showConfirmUpdatePopup && !m_bCancelUpdate)
				SleepDuringWork(1);
			if (m_bInstallUpdate)
			{
				char fullFilePath[255];
				sprintf(fullFilePath, "sd:/Project+/launcher/updates/%s", pmUpdateFileName);
				if (!PMPatchVerify(fullFilePath, sInfoText, m_bCancelUpdate, fProgressPercentage))
				{
					m_bInstallUpdate = false;
					showPatchVerificationFailedPopup = true;
					while (showPatchVerificationFailedPopup)
						SleepDuringWork(1000);

					if (!m_bInstallUpdate || m_bCancelUpdate)
						return false;

				}
				if (m_bCancelUpdate)
					return false;
				PMPatch(fullFilePath, sInfoText, m_bCancelUpdate, fProgressPercentage);

				if (m_bCancelUpdate)
					return false;

				swprintf(sInfoText, 50, L"Project+: v%4.2f installed", pmUpdateVersion);
				loadInfoFile();
				pmCurrentVersion = pmUpdateVersion;
				m_bInstallUpdate = false;
				fProgressPercentage = 0.0f;
			}
			else
				pmUpdateVersion = 0.0f;
		}
	} while (pmCurrentVersion != 0.0f && pmUpdateVersion != 0.0f && !m_bCancelUpdate);

	if (m_bCancelUpdate)
		return false;


	swprintf(sInfoText, 255, L"Checking for online updates...");
	int xmlBufferSize = 0;
	char * xmlBuffer = NULL;
	bool noOnlinePMUpdates = false;
	bool noOnlineLauncherUpdates = false;
	do{
		launcherUpdateVersion = 0.0f;
		swprintf(sInfoText, 255, L"Downloading update file...");
		//random lock ups
		if (xmlBuffer) { free(xmlBuffer); xmlBuffer = NULL; }
		xmlBufferSize = downloadFileToBuffer("https://launcher.brawlminus.net/projplus/updater/update.xml", &xmlBuffer, sInfoText, m_bCancelUpdate, fProgressPercentage);
		if (xmlBufferSize <= 0)
		{
			swprintf(sInfoText, 255, L"Download failed. Retrying...");
			SleepDuringWork(3000);
			continue;
		}
		swprintf(sInfoText, 255, L"Download complete.");

		swprintf(sInfoText, 255, L"Parsing...");

		bool failed = false;
		tinyxml2::XMLDocument doc;

		if (doc.Parse(xmlBuffer, xmlBufferSize) != (int)tinyxml2::XML_NO_ERROR)
		{
			swprintf(sInfoText, 255, L"Parse failed. Retrying...");
			SleepDuringWork(3000);
			continue;
		}

		tinyxml2::XMLElement* cur = doc.RootElement();

		if (!cur)
		{
			swprintf(sInfoText, 255, L"Parse failed. Retrying...");
			SleepDuringWork(3000);
			continue;
		}

		cur = cur->FirstChildElement("launcher");
		tinyxml2::XMLElement* launcherElement = cur;
		if (launcherElement)
		{
			tinyxml2::XMLElement * xeLauncherUpdate = launcherElement->FirstChildElement("update");
			if (!xeLauncherUpdate)
				noOnlineLauncherUpdates = true;
			else
			{
				launcherUpdateVersion = 0.0f;
				int updateLength;
				const char * updateMD5;
				std::vector<const char*> vURLs;
				int count = 0;
				do
				{
					f32 tmpNextVersion = (f32)atof(xeLauncherUpdate->Attribute("updateVersion"));
					if (tmpNextVersion > launcherCurrentVersion && tmpNextVersion > launcherUpdateVersion)
					{
						tinyxml2::XMLElement* xeBaseVersionSupported = xeLauncherUpdate->FirstChildElement("baseVersionsSupported");
						if (xeBaseVersionSupported)
						{
							tinyxml2::XMLElement* xeBaseVersion = xeBaseVersionSupported->FirstChildElement("baseVersion");
							while (xeBaseVersion)
							{

								const char * sBaseVersion = xeBaseVersion->FirstChild()->ToText()->Value();
								if (sBaseVersion[0] == '*' || launcherCurrentVersion == (f32)atof(sBaseVersion))
								{
									tinyxml2::XMLElement * xeUpdateUrl = xeLauncherUpdate->FirstChildElement("url");
									int urlCount = 0;
									while (xeUpdateUrl)
									{
										urlCount++;
										xeUpdateUrl = xeUpdateUrl->NextSiblingElement("url");
									}
									vURLs.clear();
									vURLs.reserve(urlCount);

									xeUpdateUrl = xeLauncherUpdate->FirstChildElement("url");
									while (xeUpdateUrl)
									{
										const char * sURL = xeUpdateUrl->FirstChild()->ToText()->Value();
										vURLs.push_back(sURL);
										xeUpdateUrl = xeUpdateUrl->NextSiblingElement("url");
									}

									if (urlCount != 0)
									{
										launcherUpdateVersion = tmpNextVersion;
										updateLength = (int)atoi(xeLauncherUpdate->Attribute("length"));
										updateMD5 = xeLauncherUpdate->Attribute("md5");
									}
									break;
								}
								xeBaseVersion = xeBaseVersion->NextSiblingElement("baseVersion");

							}
						}

					}
					xeLauncherUpdate = xeLauncherUpdate->NextSiblingElement("update");

				} while (xeLauncherUpdate);
				if (launcherUpdateVersion == 0.0f)
				{
					noOnlineLauncherUpdates = true;
				}
				else // update found
				{
					swprintf(sInfoText, 255, L"Online update found!");
					swprintf(confirmUpdateLine3Text, 50, L"Launcher: v%4.2f -> v%4.2f", launcherCurrentVersion, launcherUpdateVersion);
					m_bInstallUpdate = false;
					showConfirmUpdatePopup = true;
					while (showConfirmUpdatePopup && !m_bCancelUpdate)
						SleepDuringWork(1);
					if (!m_bInstallUpdate)
					{
						noOnlineLauncherUpdates = true;
						break;
					}

					int fileBufferSize = 0;
					char * fileBuffer = NULL;
					swprintf(sInfoText, 255, L"Downloading update... (0/%dKB)", fileBufferSize / 1024);

					do
					{
						int urlSelection = rand() % vURLs.size();
						if (fileBuffer) { free(fileBuffer); fileBuffer = NULL; }

						fileBufferSize = downloadFileToBuffer(vURLs[urlSelection], &fileBuffer, sInfoText, m_bCancelUpdate, fProgressPercentage);
						if (fileBufferSize <= 0)
						{
							swprintf(sInfoText, 255, L"Download failed. Retrying...");
							SleepDuringWork(3000);
							continue;
						}

						if (m_bCancelUpdate)
							break;


						swprintf(sInfoText, 255, L"Checking file integrity...");
						fProgressPercentage = 0.0f;
						char result[33];
						char hash[16];
						md5_state_t state;
						md5_init(&state);
						fProgressPercentage = 0.1f;
						md5_append(&state, fileBuffer, fileBufferSize);

						fProgressPercentage = 0.9f;
						md5_finish(&state, hash);

						memset(result, 0, 33);
						for (int i = 0; i < 16; i++)
							sprintf(result + i * 2, "%02x", hash[i]);

						fProgressPercentage = 1.0f;
						if (strcasecmp(result, updateMD5) == 0)
						{
							swprintf(sInfoText, 255, L"Storing update...");

							int len = strlen(vURLs[urlSelection]);
							int offset = -1;
							int fileNameLength = 0;
							for (int i = len - 1; i >= 0; i--)
							{

								if (vURLs[urlSelection][i] == '/')
								{
									offset = vURLs[urlSelection] + i + 1;
									break;
								}
								fileNameLength++;
							}
							const char * fileName = offset;
							//char directoryPath = "/Project+/launcher/updates/"; // fix this bane :)
							char fullPath[fileNameLength + 27]; ///why use strlen? i can count!
							sprintf(fullPath, "sd:/Project+/launcher/updates/%s", fileName);
							CreateSubfolder("sd:/Project+/launcher/updates/");

							FileHolder localUpdateFile(fullPath, "wb");
							if (!localUpdateFile.IsOpen())
							{
								swprintf(sInfoText, 255, L"Storing update failed! Retrying...");
								SleepDuringWork(3000);
								continue;
							}
							fProgressPercentage = 0.0f;
							localUpdateFile.FSeek(0, SEEK_SET);
							localUpdateFile.FWrite(fileBuffer, fileBufferSize, 1);
							fProgressPercentage = 0.9f;
							localUpdateFile.FSync();
							fProgressPercentage = 0.95f;
							localUpdateFile.FClose();

							swprintf(sInfoText, 255, L"Clearing buffers...");
							fProgressPercentage = 1.0f;

							swprintf(sInfoText, 255, L"Applying update...");

							fProgressPercentage = 0.0f;
							PMPatch(fullPath, sInfoText, m_bCancelUpdate, fProgressPercentage);
							if (m_bCancelUpdate)
								break;

							launcherCurrentVersion = launcherUpdateVersion;

							m_bRestartLauncher = false;
							showConfirmLaunchPopup = true;
							while (showConfirmLaunchPopup && !m_bCancelUpdate)
								SleepDuringWork(1);
							if (m_bCancelUpdate)
								break;
							if (m_bRestartLauncher)
							{								
								if (xmlBuffer)
								{
									delete xmlBuffer;
									xmlBuffer = NULL;
								}
								if (fileBuffer)
								{
									delete fileBuffer;
									fileBuffer = NULL;
								}
								
								return true;
							}

							break;

						}
						else
						{
							swprintf(sInfoText, 255, L"MD5 Mismatch! Retrying...");
							SleepDuringWork(3000);
							continue;
						}

					} while (!m_bCancelUpdate);
					if (fileBuffer) { free(fileBuffer); fileBuffer = NULL; }
				}
			}
		}

		cur = doc.RootElement();

		cur = cur->FirstChildElement("projectm");
		tinyxml2::XMLElement* projectmElement = cur;
		if (projectmElement)
		{
			tinyxml2::XMLElement * xePMUpdate = projectmElement->FirstChildElement("update");
			if (!xePMUpdate)
				noOnlinePMUpdates = true;
			else
			{
				pmUpdateVersion = 0.0f;
				int updateLength;
				const char * updateMD5;
				std::vector<const char*> vURLs;
				int count = 0;
				do
				{
					f32 tmpNextVersion = (f32)atof(xePMUpdate->Attribute("updateVersion"));
					if (tmpNextVersion > pmCurrentVersion && tmpNextVersion > pmUpdateVersion)
					{
						tinyxml2::XMLElement* xeBaseVersionSupported = xePMUpdate->FirstChildElement("baseVersionsSupported");
						if (xeBaseVersionSupported)
						{
							tinyxml2::XMLElement* xeBaseVersion = xeBaseVersionSupported->FirstChildElement("baseVersion");
							while (xeBaseVersion)
							{

								const char * sBaseVersion = xeBaseVersion->FirstChild()->ToText()->Value();
								if (sBaseVersion[0] == '*' || pmCurrentVersion == (f32)atof(sBaseVersion))
								{
									tinyxml2::XMLElement * xeUpdateUrl = xePMUpdate->FirstChildElement("url");
									int urlCount = 0;
									while (xeUpdateUrl)
									{
										urlCount++;
										xeUpdateUrl = xeUpdateUrl->NextSiblingElement("url");
									}
									vURLs.clear();
									vURLs.reserve(urlCount);

									xeUpdateUrl = xePMUpdate->FirstChildElement("url");
									while (xeUpdateUrl)
									{
										const char * sURL = xeUpdateUrl->FirstChild()->ToText()->Value();
										vURLs.push_back(sURL);
										xeUpdateUrl = xeUpdateUrl->NextSiblingElement("url");
									}

									if (urlCount != 0)
									{
										pmUpdateVersion = tmpNextVersion;
										updateLength = (int)atoi(xePMUpdate->Attribute("length"));
										updateMD5 = xePMUpdate->Attribute("md5");
									}
									break;
								}
								xeBaseVersion = xeBaseVersion->NextSiblingElement("baseVersion");

							}
						}

					}
					xePMUpdate = xePMUpdate->NextSiblingElement("update");

				} while (xePMUpdate);
				if (pmUpdateVersion == 0.0f)
				{
					noOnlinePMUpdates = true;
				}
				else // update found
				{
					swprintf(sInfoText, 255, L"Online update found!");
					swprintf(confirmUpdateLine3Text, 50, L"Project+: v%4.2f -> v%4.2f", pmCurrentVersion, pmUpdateVersion);
					m_bInstallUpdate = false;
					showConfirmUpdatePopup = true;
					while (showConfirmUpdatePopup && !m_bCancelUpdate)
						SleepDuringWork(1);
					if (!m_bInstallUpdate)
					{
						noOnlinePMUpdates = true;
						break;
					}

					do
					{
						int urlSelection = rand() % vURLs.size();

						int len = strlen(vURLs[urlSelection]);
						int offset = -1;
						int fileNameLength = 0;
						for (int i = len - 1; i >= 0; i--)
						{

							if (vURLs[urlSelection][i] == '/')
							{
								offset = vURLs[urlSelection] + i + 1;
								break;
							}
							fileNameLength++;
						}
						const char * fileName = offset;
						//char directoryPath = "/Project+/launcher/updates/"; // fix this bane :)
						char tmpPath[255]; ///why use strlen? i can count! EDIT: NOPE!
						sprintf(tmpPath, "sd:/Project+/launcher/updates/%s.tmp", fileName);
						CreateSubfolder("sd:/Project+/launcher/updates/");
						char *tmpPathPointer = tmpPath;

						remove(tmpPath);

						bool downloadResponse = downloadFileToDisk(vURLs[urlSelection], tmpPathPointer, sInfoText, m_bCancelUpdate, fProgressPercentage);
						if (!downloadResponse)
							remove(tmpPath);

						if (m_bCancelUpdate)
							break;

						if (!downloadResponse)
						{
							swprintf(sInfoText, 255, L"Download failed! Retrying...");
							SleepDuringWork(3000);
							continue;
						}

						swprintf(sInfoText, 255, L"Checking file integrity...");

						fProgressPercentage = 0.0f;
						FileHolder fhTmpFile(tmpPath, "rb");

						u8 * tmpBuffer = (u8*)malloc(4096);
						char result[33];
						char hash[16];
						md5_state_t state;
						md5_init(&state);
						int bytesRead = 0;
						int fileSize = fhTmpFile.Size();

						while (bytesRead < fileSize)
						{
							memset(tmpBuffer, 0, 4096);
							int r = fhTmpFile.FRead(tmpBuffer, 1, 4096);
							if (r == -1)
								break;
							md5_append(&state, tmpBuffer, r);
							bytesRead += r;

							fProgressPercentage = (f32)bytesRead / (f32)fileSize;
						}
						free(tmpBuffer);
						fProgressPercentage = 1.0f;

						fhTmpFile.FClose();
						md5_finish(&state, hash);

						memset(result, 0, 33);
						for (int i = 0; i < 16; i++)
							sprintf(result + i * 2, "%02x", hash[i]);

						fProgressPercentage = 1.0f;
						if (strcasecmp(result, updateMD5) == 0)
						{
							swprintf(sInfoText, 255, L"Storing update...");
							fProgressPercentage = 0.0f;

							char fullPath[255]; ///why use strlen? i can count! EDIT: NOPE! I CAN'T!
							sprintf(fullPath, "sd:/Project+/launcher/updates/%s", fileName);
							remove(fullPath);
							fProgressPercentage = 0.5f;
							rename(tmpPath, fullPath);
							fProgressPercentage = 1.0f;

							swprintf(sInfoText, 255, L"Verifying conditions...");

							if (!PMPatchVerify(fullPath, sInfoText, m_bCancelUpdate, fProgressPercentage))
							{
								m_bInstallUpdate = false;
								showPatchVerificationFailedPopup = true;
								while (showPatchVerificationFailedPopup)
									SleepDuringWork(1000);

								if (!m_bInstallUpdate)
									m_bCancelUpdate = true;


							}
							if (m_bCancelUpdate)
								break;

							swprintf(sInfoText, 255, L"Applying update...");

							PMPatch(fullPath, sInfoText, m_bCancelUpdate, fProgressPercentage);

							if (m_bCancelUpdate)
								break;

							swprintf(sInfoText, 50, L"Project+: v%4.2f installed", pmUpdateVersion);
							loadInfoFile();
							pmCurrentVersion = pmUpdateVersion;

							break;
						}
						else
						{
							swprintf(sInfoText, 255, L"MD5 Mismatch! Retrying...");
							SleepDuringWork(3000);
							continue;
						}

					} while (!m_bCancelUpdate);


				}
			}


		}
	} while (!m_bCancelUpdate && !noOnlineLauncherUpdates & !noOnlinePMUpdates);

	delete xmlBuffer;
	swprintf(sInfoText, 255, L"Complete.");
	if (!m_bCancelUpdate)
	{
		showNoUpdatePopup = true;
		while (showNoUpdatePopup && !m_bCancelUpdate)
			SleepDuringWork(1);
	}
	return false;


}


bool setDownloadProgress(void *obj, int size, int position)
{
	CUpdateScene *m = (CUpdateScene *)obj;

	swprintf(m->sInfoText, 255, L"Downloading update... (%d/%dKB)", position / 1024, size / 1024);
	m->fProgressPercentage = (f32)position / (f32)size;
	return !m->m_bCancelUpdate;
}

void CUpdateScene::drawProgressBar()
{
	f32 progressBarFrame = m_iDrawFrameNumber;
	if (progressBarFrame > 20.0f)
		progressBarFrame = 20.0f;

	f32 animationRatio = (f32)progressBarFrame / (f32)20;
	animationRatio = (1.0f - (1.0f - animationRatio) * (1.0f - animationRatio));

	f32 initialSizeRatio = 1.00f;
	f32 widthRatio = 0.75f;
	f32 heightRatio = 0.10f;

	f32 finalWidth = m_fScreenWidth * widthRatio;
	f32 finalHeight = m_fScreenHeight * heightRatio;

	//f32 xFinalPos = m_fScreenWidth * (1.0f - widthRatio) * 0.50f;
	//f32 yFinalPos = m_fScreenHeight * (1.0f - heightRatio) * 0.50f;

	f32 xPos = (m_fScreenWidth * 0.50f) * (1.0f - (widthRatio * initialSizeRatio) - (widthRatio * (1.0f - initialSizeRatio) * animationRatio));
	f32 yPos = (m_fScreenHeight * 0.50f) * (1.0f - (heightRatio * initialSizeRatio) - (heightRatio * (1.0f - initialSizeRatio) * animationRatio));

	f32 width = initialSizeRatio * finalWidth + ((1.0f - initialSizeRatio) * finalWidth  * animationRatio);
	f32 height = initialSizeRatio * finalHeight + ((1.0f - initialSizeRatio) * finalHeight * animationRatio);

	Menu_DrawRectangle(xPos, yPos, width, height, (GXColor){ 0, 0, 0, u8(178 * animationRatio) }, true);
	Menu_DrawRectangle(xPos, yPos, width, height, (GXColor){ 255, 255, 255, u8(255 * animationRatio) }, false);

	initialSizeRatio = 1.00f;
	widthRatio = 0.74f;
	heightRatio = 0.09f;

	finalWidth = m_fScreenWidth * widthRatio;
	finalHeight = m_fScreenHeight * heightRatio;

	//xFinalPos = m_fScreenWidth * (1.0f - widthRatio) * 0.50f;
	//yFinalPos = m_fScreenHeight * (1.0f - heightRatio) * 0.50f;

	xPos = (m_fScreenWidth * 0.50f) * (1.0f - (widthRatio * initialSizeRatio) - (widthRatio * (1.0f - initialSizeRatio) * animationRatio));
	yPos = (m_fScreenHeight * 0.50f) * (1.0f - (heightRatio * initialSizeRatio) - (heightRatio * (1.0f - initialSizeRatio) * animationRatio));

	width = initialSizeRatio * finalWidth + ((1.0f - initialSizeRatio) * finalWidth  * animationRatio);
	height = initialSizeRatio * finalHeight + ((1.0f - initialSizeRatio) * finalHeight * animationRatio);

	Menu_DrawRectangle(xPos, yPos, width, height, (GXColor){ 128, 128, 128, u8(64 * animationRatio) }, true);

	initialSizeRatio = 1.00f;
	widthRatio = 0.73f;
	heightRatio = 0.08f;

	finalWidth = m_fScreenWidth * widthRatio;
	finalHeight = m_fScreenHeight * heightRatio;

	//xFinalPos = m_fScreenWidth * (1.0f - widthRatio) * 0.50f;
	//yFinalPos = m_fScreenHeight * (1.0f - heightRatio) * 0.50f;

	xPos = (m_fScreenWidth * 0.50f) * (1.0f - (widthRatio * initialSizeRatio) - (widthRatio * (1.0f - initialSizeRatio) * animationRatio));
	yPos = (m_fScreenHeight * 0.50f) * (1.0f - (heightRatio * initialSizeRatio) - (heightRatio * (1.0f - initialSizeRatio) * animationRatio));

	width = initialSizeRatio * finalWidth + ((1.0f - initialSizeRatio) * finalWidth  * animationRatio);
	height = initialSizeRatio * finalHeight + ((1.0f - initialSizeRatio) * finalHeight * animationRatio);

	Menu_DrawRectangle(xPos, yPos, width, height, (GXColor){ 0, 0, 0, u8(178 * animationRatio) }, true);
	Menu_DrawRectangle(xPos, yPos, width * fProgressPercentage, height, (GXColor){ 163, 255, 215, u8(255 * animationRatio) }, true);

	//drawBox(0, 0, screenWidth, screenHeight, 0, 0, 0, 178);
}