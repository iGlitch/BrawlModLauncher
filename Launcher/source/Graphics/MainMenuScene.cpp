#include <stdlib.h>
#include <math.h>
#include <wchar.h>
#include <gccore.h>
#include <wiiuse/wpad.h>
#include "video.h"
#include "textures.h"
#include "MainMenuScene.h"
#include "FreeTypeGX.h"
#include "../Network/networkloader.h"
#include "../Audio/sfx.h"
#include "../Patching/tinyxml2.h"
#include "Popup.h"
#include "../IOSLoader/sys.h"
#include "../Common.h"

extern f32 g_LauncherVersion;

CMainMenuScene::CMainMenuScene(f32 w, f32 h)
{
	m_eNextScreen = SCENE_MAIN_MENU;

	m_fScreenWidth = w;
	m_fScreenHeight = h;
	m_bIsLoaded = false;
	m_iMenuSelectionAnimationFrames = 15;
	m_fMaxNewsScrollFrames = f32(60 * 25);
	m_fCurrentNewsScrollFrame = f32(-1);

};


void CMainMenuScene::Load()
{
	m_iMenuSelectedIndex = 0;
	m_iMenuSelectionFrame = 0;
	m_iDrawFrameNumber = 0;
	m_sPrevDStickX = s8(0);
	m_sPrevDStickY = s8(0);
	m_eNextScreen = SCENE_MAIN_MENU;

	showBadBootPopup = false;
	badBootPopup = new Popup(screenwidth, screenheight, 0.90f, 0.55f, 0.40f, 0.75f);
	badBootPopup->setAnimationFrames(15, true);
	badBootPopup->setSelectionTextItems(0, 1, L"Exit");
	badBootPopup->setLineTextItems(2, L"Bad boot.", L"Missing arguments.");

	showInstallPopup = false;
	installPopup = new Popup(screenwidth, screenheight, 0.90f, 0.55f, 0.40f, 0.75f);
	installPopup->setAnimationFrames(15, true);
	installPopup->setSelectionTextItems(0, 2, L"Yes", L"No");
	wchar_t installLine1[128], installLine2[128], installLine3[128];
	swprintf(installLine1, sizeof(installLine1)/sizeof(wchar_t), L"Install %hs to your SD card?", projectName);
	wcscpy(installLine2, L"No Wii system files will be modified."); 
	wcscpy(installLine3, L"Installation can take up to 15 minutes.");
	installPopup->setLineTextItems(3, installLine1, installLine2, installLine3);

	const char* projectMVersion = gameVersionStr[0] ? gameVersionStr : "Unknown";

	showAboutPopup = false;
	aboutPopup = new Popup(screenwidth, screenheight, 0.90f, 0.55f, 0.40f, 0.75f);
	aboutPopup->setAnimationFrames(15, true);
	aboutPopup->setSelectionTextItems(0, 1, L"OK");

	aboutLine1Text = (wchar_t*)malloc(sizeof(wchar_t)* 50);
	aboutLine2Text = (wchar_t*)malloc(sizeof(wchar_t)* 50);


	swprintf(aboutLine1Text, 50, L"Launcher Version: %4.2f", g_LauncherVersion);
	swprintf(aboutLine2Text, 50, L"%s Version: %s", projectName, projectMVersion);


	aboutPopup->setLineTextItems(2, aboutLine1Text, aboutLine2Text);
	m_bIsFromHBC = true;
};

void CMainMenuScene::Unload()
{
	m_iMenuSelectedIndex = -1;
	m_iMenuSelectionFrame = 0;
	m_iDrawFrameNumber = 0;
	m_sPrevDStickX = s8(0);
	m_sPrevDStickY = s8(0);

	showBadBootPopup = false;
	delete badBootPopup;
	
	showInstallPopup = false;
	delete installPopup;

	showAboutPopup = false;
	delete aboutPopup;
	free(aboutLine1Text);
	free(aboutLine2Text);
};

void CMainMenuScene::HandleInputs(u32 gcPressed, s8 dStickX, s8 dStickY, s8 cStickX, s8 cStickY, u32 wiiPressed, bool powerButtonPressed, bool resetButtonPressed)
{
	if (showBadBootPopup)
	{
		if (gcPressed & PAD_BUTTON_A || wiiPressed & WPAD_BUTTON_A || gcPressed & PAD_BUTTON_START || resetButtonPressed)
		{
			if (badBootPopup->getSelectedIndex() == 0)
			{
				m_eNextScreen = SCENE_EXIT;
				playSFX(SFX_CONFIRM);
			}
		}
		else if (gcPressed & PAD_BUTTON_B || wiiPressed & WPAD_BUTTON_B)
		{
			m_eNextScreen = SCENE_EXIT;
			playSFX(SFX_BACK);
		}
	}
	else if (showAboutPopup)
	{
		if (gcPressed & PAD_BUTTON_A || wiiPressed & WPAD_BUTTON_A || gcPressed & PAD_BUTTON_START || resetButtonPressed)
		{
			if (aboutPopup->getSelectedIndex() == 0)
			{
				showAboutPopup = false;
				aboutPopup->reset();
				playSFX(SFX_BACK);
			}
		}
		else if (gcPressed & PAD_BUTTON_B || wiiPressed & WPAD_BUTTON_B)
		{
			showAboutPopup = false;
			aboutPopup->reset();
			playSFX(SFX_BACK);
		}
	}
	else if (showInstallPopup)
	{
		if ((dStickX > STICK_DEADZONE && m_sPrevDStickX <= STICK_DEADZONE) || gcPressed & PAD_BUTTON_RIGHT || wiiPressed & WPAD_BUTTON_RIGHT)
		{
			if (installPopup->incrementSelection())
				playSFX(SFX_SELECT);
		}

		if ((dStickX < -STICK_DEADZONE && m_sPrevDStickX >= -STICK_DEADZONE) || gcPressed & PAD_BUTTON_LEFT || wiiPressed & WPAD_BUTTON_RIGHT)
		{
			if (installPopup->decrementSelection())
				playSFX(SFX_SELECT);
		}

		if (gcPressed & PAD_BUTTON_A || wiiPressed & WPAD_BUTTON_A || gcPressed & PAD_BUTTON_START || resetButtonPressed)
		{
			if (installPopup->getSelectedIndex() == 1)
			{
				showInstallPopup = false;
				installPopup->reset();
				playSFX(SFX_BACK);
			}
			else
			{
				playSFX(SFX_CONFIRM);
			}
		}
		else if (gcPressed & PAD_BUTTON_B || wiiPressed & WPAD_BUTTON_B)
		{
			showInstallPopup = false;
			installPopup->reset();
			playSFX(SFX_BACK);
		}

	}
	else
	{
		if (gcPressed & PAD_BUTTON_Y || wiiPressed & WPAD_BUTTON_PLUS)
			showAboutPopup = true;


		if ((dStickY > STICK_DEADZONE && m_sPrevDStickY <= STICK_DEADZONE) || gcPressed & PAD_BUTTON_UP || wiiPressed & WPAD_BUTTON_UP)
		{
			if (m_iMenuSelectedIndex == 0)
				m_iMenuSelectedIndex = 4;
			else
				m_iMenuSelectedIndex--;
			m_iMenuSelectionFrame = 0L;
			playSFX(SFX_SELECT);
		}

		if ((dStickY < -STICK_DEADZONE && m_sPrevDStickY >= -STICK_DEADZONE) || (gcPressed & PAD_BUTTON_DOWN || wiiPressed & WPAD_BUTTON_DOWN || powerButtonPressed)){
			if (m_iMenuSelectedIndex == 4)
				m_iMenuSelectedIndex = 0L;
			else
				m_iMenuSelectedIndex++;
			m_iMenuSelectionFrame = 0L;
			playSFX(SFX_SELECT);
		}

		if (gcPressed & PAD_BUTTON_A || wiiPressed & WPAD_BUTTON_A || gcPressed & PAD_BUTTON_START || resetButtonPressed)
		{
			switch (m_iMenuSelectedIndex)
			{
			case 0:
				/*showInstallPopup= true;
				installPopupFrame = 0L;
				installPopupSelectedIndex = -1;*/

				m_eNextScreen = SCENE_LAUNCHTITLE;
				playSFX(SFX_CONFIRM);
				break;

			case 1:
				m_eNextScreen = SCENE_UPDATE;
				playSFX(SFX_CONFIRM);
				break;
			case 2:
				m_eNextScreen = SCENE_ADDONS;
				//showAboutPopup = true;

				playSFX(SFX_CONFIRM);
				break;
			case 3:
				m_eNextScreen = SCENE_SETTINGS;
				playSFX(SFX_CONFIRM);
				break;
			case 4:
				m_eNextScreen = SCENE_EXIT;
				playSFX(SFX_CONFIRM);
				break;
			}
		}
		else
		{
			if (gcPressed & PAD_BUTTON_B || wiiPressed & WPAD_BUTTON_B)
			{
				if (m_iMenuSelectedIndex != 4)
				{
					m_iMenuSelectedIndex = 4;
					m_iMenuSelectionFrame = 0L;
					playSFX(SFX_SELECT);
				}
			}
		}
	}
	m_sPrevDStickX = dStickX;
	m_sPrevDStickY = dStickY;

}

void CMainMenuScene::Draw()
{
	if (m_iMenuSelectedIndex != -1 && m_iMenuSelectionFrame < m_iMenuSelectionAnimationFrames)
		m_iMenuSelectionFrame++;

	f32 yPos = 46.0f;
	f32 offset = 0.0F;
	if (m_iDrawFrameNumber < 15)
		offset = 11.5f * (m_iDrawFrameNumber - 15);

	if (isBadBoot) {
		showBadBootPopup = true;
		badBootPopup->draw();
		return;
	}

	drawNewsBox(yPos, offset, 65);

	yPos += 13.0f;

	//MenuItem0
	drawMenuItem(0.0f, yPos + offset, 325, 78, 33, 21, 15, &menuPlayTexture, (m_iMenuSelectedIndex == 0L));
	yPos += 59.0f;

	//MenuItem1
	drawMenuItem(0.0f, yPos + offset, 300, 113, 33, 27, 15, &menuUpdateTexture, (m_iMenuSelectedIndex == 1));
	yPos += 59.0f;

	//MenuItem2
	drawMenuItem(0.0f, yPos + offset, 275, 113, 33, 33, 15, &menuAddonsTexture, (m_iMenuSelectedIndex == 2));
	yPos += 59.0f;

	//MenuItem3
	drawMenuItem(0.0f, yPos + offset, 250, 140, 33, 39, 15, &menuSettingsTexture, (m_iMenuSelectedIndex == 3));
	yPos += 59.0f;

	//MenuItem4
	drawMenuItem(0.0f, yPos + offset, 225, 68, 33, 45, 15, &menuExitTexture, (m_iMenuSelectedIndex == 4));
	yPos += 59.0f;

	if (m_iDrawFrameNumber < 15)
		offset = ((yPos - m_fScreenHeight) / 10.0f) * (m_iDrawFrameNumber - 15);
	else
		offset = 0.0f;

	switch (m_iMenuSelectedIndex)
	{
	case -1:
		drawInfoBox(yPos + offset, 36.0F, L"");
		break;
	case 0:
		drawInfoBox(yPos + offset, 36.0F, L"A copy of Super Smash Bros. Brawl is required to play mods.");
		break;
	case 1:
		drawInfoBox(yPos + offset, 36.0F, L"Check for updates.");
		break;
	case 2:
		wchar_t addonInfo[128];
		swprintf(addonInfo, 128, L"Configure %hs Addons.", projectName);
		drawInfoBox(yPos + offset, 36.0F, addonInfo);
		
		//wchar_t commentInfo[128];
		//swprintf(commentInfo, 128, L"Access a wide array of tools to enhance your %hs experience.", projectName);
		//drawInfoBox(yPos + offset, 36.0F, commentInfo);
		break;
	case 3:
		drawInfoBox(yPos + offset, 36.0F, L"Configure launcher settings.");
		break;	
	case 4:
		if (IsDolphin())
			drawInfoBox(yPos + offset, 36.0F, L"End Dolphin emulation.");
		else if (m_bIsFromHBC)
			drawInfoBox(yPos + offset, 36.0F, L"Return to the Homebrew Channel.");
		else
			drawInfoBox(yPos + offset, 36.0F, L"Return to the Wii System Menu.");
		break;
	default:
		break;
	}

	if (showAboutPopup)
		aboutPopup->draw();

	if (showInstallPopup)
		installPopup->draw();

	if (m_iDrawFrameNumber <= 60)
		m_iDrawFrameNumber++;

}








