#include <stdlib.h>
#include <math.h>
#include <gccore.h>
#include "video.h"
#include "textures.h"

#include "CodesetScene.h"
#include "stdlib.h"
#include "FreeTypeGX.h"
#include "textures.h"

#include <fat.h>

#include "..\Patching\Patcher.h"
#include "..\Common.h"

#include "..\Patching\DVDISO.h"
#include "..\Patching\DirectoryISO.h"
extern "C"
{
#include "..\Profiler\profiler.h"
#include "..\Profiler\timer.h"
}

#include "..\Launcher\wdvd.h"
#include "..\Launcher\disc.h"


#include <unistd.h>

CPatchScene::CPatchScene(f32 w, f32 h)
{
	m_fScreenWidth = w;
	m_fScreenHeight = h;
	m_bIsLoaded = false;

	m_eNextScreen = SCENE_TOOLS;

	cancelPopup = false;
	cancelPopupSelectedIndex = -1;
	cancelPopupFrame = 0L;
	cancelPopupAnimationFrames = 15;
	selectionAnimationFrames = 15;
	m_bCancelPatch = false;
	fProgressPercentage = 0.0f;
	/*FreeTypeGX *tmpFontSystem = new FreeTypeGX(14, GX_TF_IA8);
	fontSystem = tmpFontSystem;

	//fontSystem->loadFont(vera_bold_ttf, vera_bold_ttf_size, fontSize, true);	// Initialize the font system with the font parameters from rursus_compact_mono_ttf.h*/
};

CPatchScene::~CPatchScene()
{

}

void CPatchScene::Load()
{
	selectedIndex = -1;
	selectionFrame = 0L;
	m_iDrawFrameNumber = 0L;
	prevDStickX = 0L;
	prevDStickY = 0L;
	m_eNextScreen = SCENE_TOOLS;
	cancelPopup = 0L;
	m_bCancelPatch = false;
	fProgressPercentage = 0.0f;

	cancelPopup = false;
	cancelPopupSelectedIndex = -1;
	cancelPopupFrame = 0L;

};

void CPatchScene::Unload()
{
	selectedIndex = -1;
	selectionFrame = 0L;
	m_iDrawFrameNumber = 0L;
	prevDStickX = 0L;
	prevDStickY = 0L;
	m_eNextScreen = SCENE_TOOLS;
	cancelPopup = false;

};

void CPatchScene::HandleInputs(u32 gcPressed, s8 dStickX, s8 dStickY, s8 cStickX, s8 cStickY, u32 wiiPressed, bool powerButtonPressed, bool resetButtonPressed)
{
	if (m_bCancelPatch)
		return;
	if (m_iDrawFrameNumber < 45 || selectedIndex == -1)
		return;

	if (cancelPopup)
	{
		if (dStickX > STICK_DEADZONE && prevDStickX <= STICK_DEADZONE)
		{
			cancelPopupSelectedIndex = 1;
		}

		if (dStickX < -STICK_DEADZONE && prevDStickX >= -STICK_DEADZONE)
		{
			cancelPopupSelectedIndex = 0;
		}

		if (gcPressed & PAD_BUTTON_A)
		{
			if (cancelPopupSelectedIndex == 1)
				cancelPopup = false;
			else
			{
				m_eNextScreen = SCENE_MAIN_MENU;
				m_bCancelPatch = true;
			}
		}
		else if (gcPressed & PAD_BUTTON_B)
		{
			cancelPopup = false;
		}
	}
	else
	{

		if (gcPressed & PAD_BUTTON_B)
		{
			cancelPopup = true;
			cancelPopupFrame = 0L;
			cancelPopupSelectedIndex = -1;
		}

	}
	prevDStickY = dStickY;

}

void CPatchScene::Draw()
{
	if (m_iDrawFrameNumber == 45)
		selectedIndex = 0L;
	if (cancelPopup && cancelPopupFrame < cancelPopupAnimationFrames)
		cancelPopupFrame++;
	if (cancelPopup &&  cancelPopupSelectedIndex == -1 && cancelPopupFrame == cancelPopupAnimationFrames)
		cancelPopupSelectedIndex = 1;

	f32 yPos = 46.0f;
	f32 offset = 0.0F;
	if (m_iDrawFrameNumber < 15)
		offset = 11.5f * (m_iDrawFrameNumber - 15);

	//Top White Line
	drawBox(0.0f, yPos + offset, m_fScreenWidth, 2, 255, 255, 255, 255);
	yPos += 2.0f;

	//Transparent New Box
	drawBox(0.0f, yPos + offset, m_fScreenWidth, 65, 0, 0, 0, 95);
	yPos += 65.0f;

	//Bottom White Line
	drawBox(0.0f, yPos + offset, m_fScreenWidth, 2, 255, 255, 255, 255);
	yPos += 2.0f;

	//Top Left Logo
	if (isTexturesLoaded())
		drawTexturedBox(&logoTexture, 25, 12 + offset, 180, 60, 255, 255, 255, 255);
	yPos += 20.0f;


	drawProgressBar();

	//MenuItem0
	//drawMenuItem(0, yPos, 300, 144, 40, 21, 15, &scene2InstallTexture, (selectedIndex == 0));
	yPos += 70.0f;

	//MenuItem1
	//drawMenuItem(0, yPos, 275, 89, 33, 29, 15, &scene0ToolsTexture, (selectedIndex == 1));
	yPos += 70.0f;

	//MenuItem2
	//drawMenuItem(0, yPos, 250, 100, 33, 37, 15, &scene0AboutTexture, (selectedIndex == 2));
	yPos += 70.0f;

	//MenuItem3
	//drawMenuItem(0, yPos, 225, 68, 33, 45, 15, &scene0ExitTexture, (selectedIndex == 3));
	yPos += 78.0f;

	if (m_iDrawFrameNumber < 15)
		offset = ((yPos - m_fScreenHeight) / 10.0f) * (m_iDrawFrameNumber - 15);
	else
		offset = 0.0f;
	drawInfoBox(yPos + offset, 36.0F);

	writeInfoText(yPos + offset, sInfoText);

	if (cancelPopup)
		drawCancelPopup();

	m_iDrawFrameNumber++;

}

bool CPatchScene::Work()
{

	//threadIsBusy = true;
	/*LWP_CreateThread(&workthread, workThreadFunction, NULL, NULL, 0, 96);
	LWP_ResumeThread(workthread);
	while (threadIsBusy)
	usleep(500);*/
	/*DirectoryISO d("/basefolder/");
	p.set_Source(&d);
	p.Patch("/patch.7z", "/test/");*/



	//if (!m_cPatcher.Patch("sd:/clean.7z", sInfoText, m_bCancelPatch, fProgressPercentage))
	{
		//error kept in sInfotext
		return false;
	}

	if (m_bCancelPatch)
	{
		sprintf(sInfoText, "Cancelling...");
	}
	else
	{
		sprintf(sInfoText, "Finished");
		m_eNextScreen = SCENE_MAIN_MENU; //TODO Install should be come Play
	}

	return false;
}

void* workThreadFunction()
{
	return nullptr;
}

void CPatchScene::drawProgressBar()
{
	f32 progressBarFrame = m_iDrawFrameNumber;
	if (progressBarFrame > 20.0f)
		progressBarFrame = 20.0f;

	f32 animationRatio = progressBarFrame / 20.0f;
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
	Menu_DrawRectangle(xPos, yPos, width * fProgressPercentage, height, (GXColor){ selectionColor.r, selectionColor.g, selectionColor.b, u8(255 * animationRatio) }, true);

	//drawBox(0, 0, screenWidth, screenHeight, 0, 0, 0, 178);
}

void CPatchScene::drawCancelPopup()
{
	ChangeFontSize(16);
	if (!fontSystem[16])
		fontSystem[16] = new FreeTypeGX(16);

	f32 popupAanimationRatio = (f32)cancelPopupFrame / (f32)cancelPopupAnimationFrames;
	popupAanimationRatio = (1.0f - (1.0f - popupAanimationRatio) * (1.0f - popupAanimationRatio));

	drawBox(0, 0, m_fScreenWidth, m_fScreenHeight, 0, 0, 0, u8(178 * popupAanimationRatio));

	f32 initialSizeRatio = 0.90f;
	f32 widthRatio = 0.55f;
	f32 heightRatio = 0.40f;
	f32 textAreaRatio = 0.75f;

	f32 finalWidth = m_fScreenWidth * widthRatio;
	f32 finalHeight = m_fScreenHeight * heightRatio;

	//f32 xFinalPos = m_fScreenWidth * (1.0f - widthRatio) * 0.50f;
	//f32 yFinalPos = m_fScreenHeight * (1.0f - heightRatio) * 0.50f;

	f32 xPos = (m_fScreenWidth * 0.50f) * (1.0f - (widthRatio * initialSizeRatio) - (widthRatio * (1.0f - initialSizeRatio) * popupAanimationRatio));
	f32 yPos = (m_fScreenHeight * 0.50f) * (1.0f - (heightRatio * initialSizeRatio) - (heightRatio * (1.0f - initialSizeRatio) * popupAanimationRatio));

	f32 width = initialSizeRatio * finalWidth + ((1.0f - initialSizeRatio) * finalWidth  * popupAanimationRatio);
	f32 height = initialSizeRatio * finalHeight + ((1.0f - initialSizeRatio) * finalHeight * popupAanimationRatio);

	Menu_DrawRectangle(xPos, yPos, width, height, (GXColor){ 0, 0, 0, u8(178 * popupAanimationRatio) }, true);

	if (cancelPopupSelectedIndex == 0)
	{
		Menu_DrawRectangle(xPos, yPos + (height * textAreaRatio), width / 2.0f, height - (height * textAreaRatio), (GXColor){ selectionColor.r, selectionColor.g, selectionColor.b, u8(255 * popupAanimationRatio) }, true);
		fontSystem[16]->drawText(xPos + (width / 4.0f), yPos + (height * (textAreaRatio + ((1 - textAreaRatio) * 0.50f))), L"Yes", (GXColor){ 0, 0, 0, u8(255 * popupAanimationRatio) }, FTGX_JUSTIFY_CENTER | FTGX_ALIGN_MIDDLE);
	}
	else
		fontSystem[16]->drawText(xPos + (width / 4.0f), yPos + (height * (textAreaRatio + ((1 - textAreaRatio) * 0.50f))), L"Yes", (GXColor){ 255, 255, 255, u8(255 * popupAanimationRatio) }, FTGX_JUSTIFY_CENTER | FTGX_ALIGN_MIDDLE);

	if (cancelPopupSelectedIndex == 1)
	{
		Menu_DrawRectangle(xPos + (width / 2.0f), yPos + (height * textAreaRatio), width / 2.0f, height - (height * textAreaRatio), (GXColor){ selectionColor.r, selectionColor.g, selectionColor.b, u8(255 * popupAanimationRatio) }, true);
		fontSystem[16]->drawText(xPos + (width / 4.0f) + (width / 2.0f), yPos + (height * (textAreaRatio + ((1 - textAreaRatio) * 0.50f))), L"No", (GXColor){ 0, 0, 0, u8(255 * popupAanimationRatio) }, FTGX_JUSTIFY_CENTER | FTGX_ALIGN_MIDDLE);
	}
	else
		fontSystem[16]->drawText(xPos + (width / 4.0f) + (width / 2.0f), yPos + (height * (textAreaRatio + ((1 - textAreaRatio) * 0.50f))), L"No", (GXColor){ 255, 255, 255, u8(255 * popupAanimationRatio) }, FTGX_JUSTIFY_CENTER | FTGX_ALIGN_MIDDLE);

	Menu_DrawRectangle(xPos, yPos, width, height, (GXColor){ 125, 125, 125, u8(255 * popupAanimationRatio) }, false);

	Menu_DrawRectangle(xPos, yPos + (height * textAreaRatio), width, 1.0f, (GXColor){ 125, 125, 125, u8(255 * popupAanimationRatio) }, true);
	Menu_DrawRectangle(m_fScreenWidth / 2.0f, yPos + (height * textAreaRatio), 1.0f, height - (height * textAreaRatio), (GXColor){ 125, 125, 125, u8(255 * popupAanimationRatio) }, true);

	//wchar_t installText[128];
	//swprintf(installText, 128, L"Install %hs to your SD card?", projectName);
	//fontSystem[16]->drawText(m_fScreenWidth / 2.0f, yPos + 35.0f, installText, (GXColor){ 255, 255, 255, 255 * popupAanimationRatio }, FTGX_JUSTIFY_CENTER);
	fontSystem[16]->drawText(m_fScreenWidth / 2.0f, yPos + 60.0f, L"Are you sure you want to cancel the installation?", (GXColor){ 255, 255, 255, u8(255 * popupAanimationRatio) }, FTGX_JUSTIFY_CENTER);
	//fontSystem[16]->drawText(m_fScreenWidth / 2.0f, yPos + 85.0f, L"Installation can take up to 15 minutes.", (GXColor){ 255, 255, 255, 255 * popupAanimationRatio }, FTGX_JUSTIFY_CENTER);



	//drawBox(0, 0, m_fScreenWidth, m_fScreenHeight, 0, 0, 0, 178);	
}


void CPatchScene::writeInfoText(f32 y, char* text)
{
	ChangeFontSize(15);
	if (!fontSystem[15])
		fontSystem[15] = new FreeTypeGX(15);

	auto p = charToWideChar(text);
	fontSystem[15]->drawText(m_fScreenWidth / 2.0f, y + 24, p, (GXColor){ 0xff, 0xff, 0xff, 0xff }, FTGX_JUSTIFY_CENTER);
	delete p;
}


void CPatchScene::drawInfoBox(f32 yPos, f32 height)
{
	//Top White Line
	drawBox(m_fScreenWidth / 16.0f, yPos, m_fScreenWidth / 16.0f * 14.0f, 2, 255, 255, 255, 255);
	//Left Line
	drawBox(m_fScreenWidth / 16.0f, yPos, 2, height + 4, 255, 255, 255, 255);
	//Right Line
	drawBox(m_fScreenWidth / 16.0f * 15.0f - 2.0f, yPos, 2, height + 4, 255, 255, 255, 255);

	yPos += 2.0f;

	//Transparent New Box
	drawBox(m_fScreenWidth / 16.0f + 2, yPos, m_fScreenWidth / 16.0f * 14.0f - 4.0f, height, 0, 0, 0, 95);
	yPos += height;

	//Top White Line
	drawBox(m_fScreenWidth / 16.0f, yPos, m_fScreenWidth / 16.0f * 14.0f, 2, 255, 255, 255, 255);
}

