#include <gctypes.h>
#include <wchar.h>
#include <cstdarg>
#include <vector>
#include <memory>
#include <stdlib.h>
#include <string.h>

#include "Popup.h"
#include "video.h"
#include "FreeTypeGX.h"
#include "../Common.h"

Popup::Popup(f32 screenWidth, f32 screenHeight, f32 initialSizeRatio, f32 widthRatio, f32 heightRatio, f32 textAreaRatio)
{
	m_fScreenWidth = screenWidth;
	m_fScreenHeight = screenHeight;
	m_fInitialSizeRatio = initialSizeRatio;
	m_fWidthRatio = widthRatio;
	m_fHeightRatio = heightRatio;
	m_fTextAreaRatio = textAreaRatio;
	m_iFontSize = 16;
	m_iDefaultSelectedIndex = -1;
	m_bIncrementFrameOnDraw = true;
	this->reset();
}

Popup::~Popup()
{
	this->reset();
	m_vSelectionItems.clear();
	m_vLineItems.clear();
	
}

void Popup::setSelectionTextItems(int defaultSelectedIndex, int selectionCount, ...)
{
	m_iSelectionCount = selectionCount;
	m_iDefaultSelectedIndex = defaultSelectedIndex;
	va_list arguments;
	/* Initializing arguments to store all values after num */
	va_start(arguments, selectionCount);
	/* Sum all the inputs; we still rely on the function caller to tell us how
	* many there are */
	m_vSelectionItems.clear();
	m_vSelectionItems.reserve(selectionCount);
	for (int x = 0; x < selectionCount; x++)
	{
		const wchar_t * value = va_arg(arguments, const wchar_t *);
		m_vSelectionItems.push_back(value);
	}
	va_end(arguments);
}

void Popup::setLineTextItems(int lineCount, ...)
{
	m_iLineCount = lineCount;
	va_list arguments;
	/* Initializing arguments to store all values after num */
	va_start(arguments, lineCount);
	/* Sum all the inputs; we still rely on the function caller to tell us how
	* many there are */
	m_vLineItems.clear();
	m_vLineItems.reserve(lineCount);
	for (int x = 0; x < lineCount; x++)
	{
		const wchar_t * value = va_arg(arguments, const wchar_t *);
		m_vLineItems.push_back(value);
	}
	va_end(arguments);


}

/*void Popup::setTextData(const wchar_t Selection1Text, const wchar_t Selection2Text, const wchar_t Line1Text, const wchar_t Line2Text, const wchar_t Line3Text)
{
m_iSelectionCount = 1;
m_vSelectionItems

wprintf(m_wSelection1Text, Selection1Text);
wprintf(m_wSelection2Text, Selection2Text);
if (Selection2Text != NULL)
m_iSelectionCount = 2;

m_iLineCount = 1;
wprintf(m_wLine1Text, Line1Text);
wprintf(m_wLine2Text, Line2Text);
wprintf(m_wLine3Text, Line3Text);

if (Line3Text != NULL)
m_iLineCount = 3;
else if (Line2Text != NULL)
m_iLineCount = 2;
}*/

void Popup::setAnimationFrames(int totalFrames, bool decelerate)
{
	m_iTotalFrames = totalFrames;
	m_bDecelerate = decelerate;
}

bool Popup::incrementSelection()
{
	if (m_iSelectedIndex == -1)
		return false;
	if (m_iSelectedIndex == m_iSelectionCount - 1)
		return false;
	m_iSelectedIndex++;
	return true;
}
bool Popup::decrementSelection()
{
	if (m_iSelectedIndex == -1)
		return false;
	if (m_iSelectedIndex == 0)
		return false;
	m_iSelectedIndex--;
	return true;
}
bool Popup::toggleSelection()
{
	if (m_iSelectedIndex == -1)
		return false;
	if (m_iSelectedIndex == 0)
	{
		m_iSelectedIndex = 1;
		return true;
	}
	else if (m_iSelectedIndex == 1)
	{
		m_iSelectedIndex = 0;
		return true;
	}
	return false;
}

void Popup::reset()
{
	m_iSelectedIndex = -1;
	m_iFrameNumber = 0;
}

bool Popup::incrementFrameNumber()
{
	if (m_iFrameNumber >= m_iTotalFrames)
		return false;
	if (m_iFrameNumber < m_iTotalFrames)
		m_iFrameNumber++;
	if (m_iFrameNumber == m_iTotalFrames)
		m_iSelectedIndex = m_iDefaultSelectedIndex;
	return true;
}

void Popup::draw()
{
	f32 animationRatio = (f32)m_iFrameNumber / (f32)m_iTotalFrames;
	if (m_bDecelerate)
		animationRatio = (1.0f - (1.0f - animationRatio) * (1.0f - animationRatio));

	drawBox(0, 0, m_fScreenWidth, m_fScreenHeight, 0, 0, 0, 178 * animationRatio);

	/*f32 initialSizeRatio = 0.90f;
	f32 widthRatio = 0.55f;
	f32 heightRatio = 0.40f;
	f32 textAreaRatio = 0.75f;*/

	f32 finalWidth = m_fScreenWidth * m_fWidthRatio;
	f32 finalHeight = m_fScreenHeight * m_fHeightRatio;

	//f32 xFinalPos = m_fScreenWidth * (1.0f - widthRatio) * 0.50f;
	//f32 yFinalPos = m_fScreenHeight * (1.0f - heightRatio) * 0.50f;

	f32 xPos = (m_fScreenWidth * 0.50f) * (1.0f - (m_fWidthRatio * m_fInitialSizeRatio) - (m_fWidthRatio * (1.0f - m_fInitialSizeRatio) * animationRatio));
	f32 yPos = (m_fScreenHeight * 0.50f) * (1.0f - (m_fHeightRatio * m_fInitialSizeRatio) - (m_fHeightRatio * (1.0f - m_fInitialSizeRatio) * animationRatio));

	f32 width = m_fInitialSizeRatio * finalWidth + ((1.0f - m_fInitialSizeRatio) * finalWidth  * animationRatio);
	f32 height = m_fInitialSizeRatio * finalHeight + ((1.0f - m_fInitialSizeRatio) * finalHeight * animationRatio);


	ChangeFontSize(m_iFontSize);
	if (!fontSystem[m_iFontSize])
		fontSystem[m_iFontSize] = new FreeTypeGX(m_iFontSize);


	Menu_DrawRectangle(xPos, yPos, width, height, (GXColor){ 0, 0, 0, u8(178 * animationRatio) }, true);


	for (int i = 0; i < m_iSelectionCount; i++)
	{
		GXColor itemBackgroundColor = (GXColor){ 255, 255, 255, u8(255 * animationRatio) };
		if (i == m_iSelectedIndex)
		{
			Menu_DrawRectangle(xPos + ((f32)i * width / (f32)m_iSelectionCount), yPos + (height * m_fTextAreaRatio), width / (f32)m_iSelectionCount, height - (height * m_fTextAreaRatio), (GXColor){ selectionColor.r, selectionColor.g, selectionColor.b, u8(255 * animationRatio) }, true);
			itemBackgroundColor = (GXColor){ 0, 0, 0, u8(255 * animationRatio) };
		}

		fontSystem[m_iFontSize]->drawText(xPos + (width / (2.0f * (f32)m_iSelectionCount) + ((f32)i * width / (f32)m_iSelectionCount)), yPos + (height * (m_fTextAreaRatio + ((1 - m_fTextAreaRatio) * 0.50f))), m_vSelectionItems[i], itemBackgroundColor, FTGX_JUSTIFY_CENTER | FTGX_ALIGN_MIDDLE);
	}

	Menu_DrawRectangle(xPos, yPos, width, height, (GXColor){ 125, 125, 125, u8(255 * animationRatio) }, false);

	Menu_DrawRectangle(xPos, yPos + (height * m_fTextAreaRatio), width, 1.0f, (GXColor){ 125, 125, 125, u8(255 * animationRatio) }, true);

	for (int i = 1; i < m_iSelectionCount; i++)
		Menu_DrawRectangle(xPos + ((f32)i *  (width / (f32)m_iSelectionCount)), yPos + (height * m_fTextAreaRatio), 1.0f, height - (height * m_fTextAreaRatio), (GXColor){ 125, 125, 125, u8(255 * animationRatio) }, true);

	for (int i = 0; i < m_iLineCount; i++)
	{
		if (true)
		fontSystem[m_iFontSize]->drawText(m_fScreenWidth / 2.0f, yPos + 35.0f + ((f32)i * 25.0f), m_vLineItems[i], (GXColor){ 255, 255, 255, u8(255 * animationRatio) }, FTGX_JUSTIFY_CENTER);
		else //equal height
			fontSystem[m_iFontSize]->drawText(m_fScreenWidth / 2.0f, yPos + ((height * m_fTextAreaRatio) / (2.0f * (f32)m_iLineCount) + ((f32)i * (height * m_fTextAreaRatio) / (f32)m_iLineCount)), m_vLineItems[i], (GXColor){ 255, 255, 255, u8(255 * animationRatio) }, FTGX_JUSTIFY_CENTER);
	}

	if (m_bIncrementFrameOnDraw)
		incrementFrameNumber();

}