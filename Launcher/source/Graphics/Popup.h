#ifndef _POPUP_H_
#define _POPUP_H_

#include <gctypes.h>
#include <wchar.h>
#include <vector>
#include <memory>
#include <stdlib.h>
#include <string.h>
#include <cstdarg>
#include "FreeTypeGX.h"


extern FreeTypeGX *fontSystem[];

class Popup
{
public:
	Popup(f32 screenWidth, f32 screenHeight, f32 initialSizeRatio, f32 widthRatio, f32 heightRatio, f32 textAreaRatio);
	~Popup();
	void setAnimationFrames(int totalFrames, bool decelerate);
	void draw();
	void setSelectionTextItems(int defaultSelectedIndex, int selectionCount, ...);
	void setLineTextItems(int selectionCount, ...);

	void setIncrementOnDraw(bool incrementOnDraw) { m_bIncrementFrameOnDraw = incrementOnDraw; };
	bool getIncrementOnDraw(){ return m_bIncrementFrameOnDraw; };

	void reset();
	bool incrementSelection();
	bool decrementSelection();
	bool toggleSelection();
	bool incrementFrameNumber();
	int getSelectedIndex(){ return m_iSelectedIndex; };

private:

	f32 m_fScreenWidth;
	f32 m_fScreenHeight;
	f32 m_fInitialSizeRatio;
	f32 m_fWidthRatio;
	f32 m_fHeightRatio;
	f32 m_fTextAreaRatio;
	f32 m_iTotalFrames;
	f32 m_bDecelerate;
	int m_bIncrementFrameOnDraw;
	int m_iFontSize;
	int m_iFrameNumber;
	int m_iSelectedIndex;
	int m_iDefaultSelectedIndex;
	int m_iSelectionCount;
	std::vector<const wchar_t* > m_vSelectionItems;
	int m_iLineCount;
	std::vector<const wchar_t* > m_vLineItems;



};
#endif