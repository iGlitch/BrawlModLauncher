#include <stdlib.h>
#include <math.h>
#include <gccore.h>
#include <unistd.h>
#include <fat.h>
#include <ogcsys.h>
#include <string.h>
#include <wiiuse/wpad.h>
#include <dirent.h>

#include "../Common.h"

#include "video.h"
#include "AddonsScene.h"
#include "stdlib.h"
#include "FreeTypeGX.h"
#include "../Audio/sfx.h"
#include "../Patching/tinyxml2.h"
#include "../Graphics/FreeTypeGX.h"
#include "textures.h"
#include "../Patching/7z/CreateSubfolder.h"

using namespace tinyxml2;

CAddonsScene::CAddonsScene(f32 w, f32 h)
{
        m_eNextScreen = SCENE_ADDONS;

        m_fScreenWidth = w;
        m_fScreenHeight = h;
        m_bIsLoaded = false;
        m_iMenuSelectionAnimationFrames = 15;
        m_fMaxNewsScrollFrames = f32(60 * 25);
        m_fCurrentNewsScrollFrame = f32(-1);
        enumeratedFiles = false;
        m_bShowScrollbar = false;
        m_fScrollbarHeight = 0.0f;
        m_fScrollbarY = 0.0f;

};

void CAddonsScene::Load()
{
        installIndex = -1;
        m_iMenuSelectedIndex = -2L;
        m_iMenuSelectionFrame = 0;
        m_iDrawFrameNumber = 0;
        m_sPrevDStickX = s8(0);
        m_sPrevDStickY = s8(0);
        m_eNextScreen = SCENE_ADDONS;




        sInfoText = malloc(sizeof(wchar_t)* 255);
        memset(sInfoText, 0, sizeof(wchar_t)* 255);
        //addonFiles.reserve(0);

};

void CAddonsScene::Unload()
{
        m_iMenuSelectedIndex = -1L;
        m_iMenuSelectionFrame = 0;
        m_iDrawFrameNumber = 0;
        m_sPrevDStickX = s8(0);
        m_sPrevDStickY = s8(0);

        delete sInfoText;
        int addonFilesLen = addonFiles.size();
        for (int i = 0; i < addonFilesLen; i++)
        {
                if (addonFiles[i] != NULL)
                {
                        delete addonFiles[i];
                        addonFiles[i] = NULL;
                }
                //openedFiles[i]->FFlush();
                //openedFiles[i]->FClose();
                //free(openedFiles[i]);
        }
        addonFiles.clear();
};

void CAddonsScene::HandleInputs(u32 gcPressed, s8 dStickX, s8 dStickY, s8 cStickX, s8 cStickY, u32 wiiPressed, bool powerButtonPressed, bool resetButtonPressed)
{
        if (m_iDrawFrameNumber < 30 || m_iMenuSelectedIndex == -2 || installIndex != -1)
                return;
        if (m_iMenuSelectedIndex == -1)
        {
                if (gcPressed & PAD_BUTTON_B || wiiPressed & WPAD_BUTTON_B || powerButtonPressed)
                {
                        m_eNextScreen = SCENE_MAIN_MENU;
                        playSFX(SFX_BACK);
                }
                return;
        }

        if (gcPressed & PAD_BUTTON_A || wiiPressed & WPAD_BUTTON_A || gcPressed & PAD_BUTTON_START || resetButtonPressed)
        {
                installIndex = m_iMenuSelectedIndex;
                playSFX(SFX_CONFIRM);
        }
        else
        {
                if (gcPressed & PAD_BUTTON_B || wiiPressed & WPAD_BUTTON_B || (powerButtonPressed && m_iMenuSelectedIndex == addonFiles.size() - 1))
                {
                        m_eNextScreen = SCENE_MAIN_MENU;
                        playSFX(SFX_BACK);
                }

        }

        if ((dStickY > STICK_DEADZONE && m_sPrevDStickY <= STICK_DEADZONE) || gcPressed & PAD_BUTTON_UP || wiiPressed & WPAD_BUTTON_UP)
        {
                if (m_iMenuSelectedIndex == 0L)
                        m_iMenuSelectedIndex = addonFiles.size() - 1;
                else
                        m_iMenuSelectedIndex--;
                m_iMenuSelectionFrame = 0L;
                playSFX(SFX_SELECT);
        }

        if ((dStickY < -STICK_DEADZONE && m_sPrevDStickY >= -STICK_DEADZONE) || (gcPressed & PAD_BUTTON_DOWN || wiiPressed & WPAD_BUTTON_DOWN || powerButtonPressed && !(m_iMenuSelectedIndex == addonFiles.size() - 1))){
                if (m_iMenuSelectedIndex == addonFiles.size() - 1)
                        m_iMenuSelectedIndex = 0L;
                else
                        m_iMenuSelectedIndex++;
                m_iMenuSelectionFrame = 0L;
                playSFX(SFX_SELECT);
        }

        m_sPrevDStickX = dStickX;
        m_sPrevDStickY = dStickY;

}

void CAddonsScene::Draw()
{


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

        drawInfoBox(yPos + offset, 36.0F, sInfoText);

        //if (frameNumber <= 45)



        if (m_iDrawFrameNumber <= 30)
                m_iDrawFrameNumber++;

        //swprintf(sInfoText, 255, L"menu %d", m_iMenuSelectedIndex);

}

void CAddonsScene::drawSelectionMenu(float yPos)
{
        f32 animationFrame = m_iDrawFrameNumber;
        f32 animationFrames = 20.0f;
        if (animationFrame > animationFrames)
                animationFrame = animationFrames;


        f32 animationRatio = (f32)animationFrame / (f32)animationFrames;
        animationRatio = (1.0f - (1.0f - animationRatio) * (1.0f - animationRatio));

        f32 initialSizeRatio = 0.10f;
        f32 widthRatio = 0.85f;
        f32 heightRatio = 1.00f;


        f32 baseWidth = m_fScreenWidth * widthRatio;
        f32 finalHeight = 280 * heightRatio;
        
        UpdateScrollbarState(yPos, finalHeight);
        
        f32 scrollbarWidth = m_bShowScrollbar ? 12.0f : 0.0f;
        f32 finalWidth = baseWidth - scrollbarWidth;

        //f32 xFinalPos = m_fScreenWidth * (1.0f - widthRatio) * 0.50f;
        //f32 yFinalPos = m_fScreenHeight * (1.0f - heightRatio) * 0.50f;

        f32 xPos = (m_fScreenWidth * 0.50f) * (1.0f - (widthRatio * initialSizeRatio) - (widthRatio * (1.0f - initialSizeRatio) * animationRatio));
        //yPos+= (finalHeight * 0.50f) * (1.0f - (initialSizeRatio) -  ;
        yPos += (finalHeight * 0.50f) * (1.0f - (heightRatio * initialSizeRatio) - (heightRatio * (1.0f - initialSizeRatio) * animationRatio));

        f32 width = initialSizeRatio * finalWidth + ((1.0f - initialSizeRatio) * finalWidth  * animationRatio);
        f32 height = initialSizeRatio * finalHeight + ((1.0f - initialSizeRatio) * finalHeight * animationRatio);

        //Box
        Menu_DrawRectangle(xPos, yPos, width, height, (GXColor){ 0, 0, 0, u8(178 * animationRatio) }, true);


        //Image Box
        /*
        f32 ibFinalWidth = 210.0f;
        f32 ibWidthRatio = ibFinalWidth / finalWidth;
        f32 ibWidth = (width * ibWidthRatio);
        f32 ibX = xPos + width - ibWidth;

        Menu_DrawRectangle(ibX, yPos, ibWidth, height, (GXColor){ 255, 255, 255, u8(255 * animationRatio) }, false);
        */



        int separatorCount = 5;

        f32 rowHeight = height / (f32)separatorCount;
        //Populate

        if (enumeratedFiles && m_iMenuSelectedIndex >= 0)
        {
                int initialPosition = m_iMenuSelectedIndex / separatorCount; //floor
                initialPosition *= 5;
                int modPosition = m_iMenuSelectedIndex % separatorCount;
                int endPosition = initialPosition + 5;
                if (endPosition > addonFiles.size())
                        endPosition = addonFiles.size();
                for (int i = initialPosition; i < endPosition; i++)
                {
                        if (m_iMenuSelectedIndex == i && m_iDrawFrameNumber >= 20)
                                Menu_DrawRectangle(xPos, yPos + ((f32)(i % separatorCount) * height / (f32)separatorCount), width, rowHeight, (GXColor){ selectionColor.r, selectionColor.g, selectionColor.b, u8(255 * animationRatio) }, true);

                        AddonFile * f = addonFiles[i];
                        ChangeFontSize(18);
                        if (!fontSystem[18])
                                fontSystem[18] = new FreeTypeGX(18);

                        auto text = charToWideChar(f->title);
                        if (m_iMenuSelectedIndex == i && m_iDrawFrameNumber >= 20)
                                fontSystem[18]->drawText(xPos + (width * 0.01f), yPos + ((f32)(i % separatorCount) * height / (f32)separatorCount) + (rowHeight * .5f), text, (GXColor){ 0x00, 0x00, 0x00, 0xff }, FTGX_JUSTIFY_LEFT | FTGX_ALIGN_MIDDLE);
                        else
                                fontSystem[18]->drawText(xPos + (width * 0.01f), yPos + ((f32)(i % separatorCount) * height / (f32)separatorCount) + (rowHeight * .5f), text, (GXColor){ 0xff, 0xff, 0xff, 0xff }, FTGX_JUSTIFY_LEFT | FTGX_ALIGN_MIDDLE);
                        free(text);

                        f32 texHeight = rowHeight * 0.75f;
                        f32 texWidth = texHeight;

                        f32 texX = (xPos + width) - (width * 0.01f) - (texWidth);
                        f32 texY = yPos + ((f32)(i % separatorCount) * height / (f32)separatorCount) + (rowHeight * 0.10f);
                        switch (f->state)
                        {

                        case ADDON_FILE_STATE_INCOMPLETE:
                        case ADDON_FILE_STATE_CONFLICTING:
                                if (isTexturesLoaded())
                                        drawTexturedBox(&addonConflict, texX, texY, texWidth, texHeight, 255, 255, 255, 255);
                                if (installIndex == -1)
                                        swprintf(sInfoText, 255, L"Addon is not properly installed. Select to reinstall.");
                                break;
                        case ADDON_FILE_STATE_UPGRADE:
                        case ADDON_FILE_STATE_NOT_INSTALLED:
                                if (isTexturesLoaded())
                                        drawTexturedBox(&addonNotInstalled, texX, texY, texWidth, texHeight, 255, 255, 255, 255);
                                if (installIndex == -1)
                                        swprintf(sInfoText, 255, L"Install this addon.");
                                break;
                        case ADDON_FILE_STATE_INSTALLED:
                                if (isTexturesLoaded())
                                        drawTexturedBox(&addonInstalled, texX, texY, texWidth, texHeight, 255, 255, 255, 255);
                                if (installIndex == -1)
                                        swprintf(sInfoText, 255, L"Uninstall this addon.");
                                break;
                        case ADDON_FILE_STATE_ONLINE:
                                break;
                        }
                }
        }

        //Separators
        for (int i = 1; i < separatorCount; i++)
        {
                Menu_DrawRectangle(xPos, yPos + ((f32)i * height / (f32)separatorCount), width, 1, (GXColor){ 255, 255, 255, u8(255 * animationRatio) }, true);
        }

        Menu_DrawRectangle(xPos, yPos, width, height, (GXColor){ 255, 255, 255, u8(255 * animationRatio) }, false);
        UpdateScrollbarState(yPos, height);
        drawScrollbar(xPos, yPos, width, height, animationRatio);

        //drawBox(0, 0, screenWidth, screenHeight, 0, 0, 0, 178);
}

void CAddonsScene::UpdateScrollbarState(f32 menuY, f32 menuHeight)
{
	m_bShowScrollbar = (addonFiles.size() > 5);

	if (!m_bShowScrollbar)
	{
		m_fScrollbarHeight = 0.0f;
		m_fScrollbarY = 0.0f;
		return;
	}

	f32 visibleRatio = 5.0f / (f32)addonFiles.size();
	m_fScrollbarHeight = menuHeight * visibleRatio;
	
	if (m_fScrollbarHeight < 10.0f)
		m_fScrollbarHeight = 10.0f;

	if (m_iMenuSelectedIndex >= 0)
	{
		int currentPage = m_iMenuSelectedIndex / 5;
		int totalPages = (addonFiles.size() + 4) / 5;
		
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

void CAddonsScene::drawScrollbar(f32 menuX, f32 menuY, f32 menuWidth, f32 menuHeight, f32 animationRatio)
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

bool CAddonsScene::Work()
{

        if (installIndex != -1 && installIndex < addonFiles.size())
        {


                AddonFile * af = addonFiles[installIndex];

                int ret = 0;

                switch (af->state)
                {

                case ADDON_FILE_STATE_INCOMPLETE:
                case ADDON_FILE_STATE_CONFLICTING:
                case ADDON_FILE_STATE_NOT_INSTALLED:
                        swprintf(sInfoText, 255, L"Installing %s...", af->title);
                        ret = af->Install();
                        if (ret != 0)
                                swprintf(sInfoText, 255, L"Error #%d", ret);
                        else
                                swprintf(sInfoText, 255, L"Installed!", ret);
                        break;
                case ADDON_FILE_STATE_INSTALLED:
                        swprintf(sInfoText, 255, L"Uninstalling %s...", af->title);
                        ret = af->Uninstall();
                        if (ret != 0)
                                swprintf(sInfoText, 255, L"Error #%d", ret);
                        else
                                swprintf(sInfoText, 255, L"Uninstalled!", ret);
                        break;

                case ADDON_FILE_STATE_UPGRADE:
                        swprintf(sInfoText, 255, L"Upgrading %s...", af->title);

                        ret = af->Uninstall();
                        if (ret != 0)
                                swprintf(sInfoText, 255, L"Error #%d", ret);
                        else
                        {
                                ret = af->Install();
                                if (ret != 0)
                                        swprintf(sInfoText, 255, L"Error #%d", ret);
                                else
                                        swprintf(sInfoText, 255, L"Upgraded!", ret);
                        }
                        break;
                case ADDON_FILE_STATE_UNKNOWN:
                case ADDON_FILE_STATE_ONLINE:
                        break;
                }
                sleep(1);
                enumeratedFiles = false;
                installIndex = -1;
        }

        if (!enumeratedFiles)
        {
                //enumerate local files
                swprintf(sInfoText, 255, L"Searching for local addons...");
                struct dirent *pent;
                struct stat statbuf;
                char addonsPath[ISFS_MAXPATH];
                snprintf(addonsPath, sizeof(addonsPath), "%s/launcher/addons", codesBasePath);
                CreateSubfolder(addonsPath);
                DIR * addonsFolder = opendir(addonsPath);

                if (!addonsFolder) {
                        swprintf(sInfoText, 255, L"Cannot open addons folder!");
                        enumeratedFiles = true;
                        m_iMenuSelectedIndex = -1;
                        return false;
                }

                bool foundItems = false;
                addonFiles.clear();
                int filesChecked = 0;

                while ((pent = readdir(addonsFolder)) != NULL) {
                        if (strcmp(".", pent->d_name) == 0 ||
                                strcmp("..", pent->d_name) == 0)
                                continue;

                        int len = strlen(pent->d_name);
                        if (len < 3 || strcasecmp(pent->d_name + len - 3, ".7z") != 0) //check extension
                                continue;

                        filesChecked++;
                        char fullFilePath[ISFS_MAXPATH];
                        snprintf(fullFilePath, sizeof(fullFilePath), "%s/launcher/addons/%s", codesBasePath, pent->d_name);

                        if (stat(fullFilePath, &statbuf) != 0) {
                                swprintf(sInfoText, 255, L"Cannot stat %s", pent->d_name);
                                sleep(2);
                                continue;
                        }

                        if (S_ISDIR(statbuf.st_mode) != 0) {
                                swprintf(sInfoText, 255, L"%s is a directory, skipping", pent->d_name);
                                sleep(2);
                                continue;
                        }

                        AddonFile* f = new AddonFile(fullFilePath);

                        int ret = f->open();
                        if (ret != 0)
                        {
                                int szErr = (f->sZip) ? f->sZip->SzResult : -999;
                                u32 itemCount = (f->sZip) ? f->sZip->GetItemCount() : 0;
                                delete f;
                                swprintf(sInfoText, 255, L"%s: Err %d (7z=%d, items=%d)", pent->d_name, ret, szErr, itemCount);
                                sleep(5);
                                continue;
                        }

                        bool foundMatch = false;
                        for (int i = 0; i < addonFiles.size(); i++)
                        {
                                AddonFile * af = addonFiles[i];
                                if (strcasecmp(f->code, af->code) == 0)
                                {
                                        foundMatch = true;
                                        if (f->version > af->version)
                                        {
                                                delete af;
                                                addonFiles[i] = f;
                                                break;
                                        }
                                }
                        }
                        if (!foundMatch)
                        {
                                foundItems = true;
                                addonFiles.push_back(f);
                        }


                }
                closedir(addonsFolder);

                //swprintf(sInfoText, 255, L"Searching for online addons...");
                //enumerate online files
                if (foundItems)
                {
                        m_iMenuSelectedIndex = 0L;
                        swprintf(sInfoText, 255, L"");
                }
                else
                {
                        if (filesChecked == 0)
                                swprintf(sInfoText, 255, L"No .7z files found in addons folder.");
                        else
                                swprintf(sInfoText, 255, L"Found %d .7z files, but all were invalid.", filesChecked);
                        m_iMenuSelectedIndex = -1;
                }

                enumeratedFiles = true;



        }

        //check for install request
        //install

        //check for uninstall request
        //uninstall

        return false;
}