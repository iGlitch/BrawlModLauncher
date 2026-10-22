#ifndef PATCHSCENE_H
#define PATCHSCENE_H

#include "GraphicsScene.h"
#include "video.h"
#include "stdlib.h"
#include "..\Patching\Patcher.h"

class CPatchScene : public GraphicsScene
{
public:
	CPatchScene(f32 w, f32 h);
	~CPatchScene();

	virtual void Draw();
	virtual void Load();
	virtual void Unload();
	virtual bool Work();
	virtual void HandleInputs(u32 gcPressed, s8 dStickX, s8 dStickY, s8 cStickX, s8 cStickY, u32 wiiPressed, bool powerButtonPressed, bool resetButtonPressed);

	virtual void Focus() { }; //not used
private:
	CPatchScene() {}

	void drawProgressBar();
	void drawInfoBox(f32 yPos, f32 f32);
	void writeInfoText(f32 y, char* text);
	void drawCancelPopup();
	int selectedIndex;
	int selectionFrame;
	int selectionAnimationFrames;
	int cancelPopupFrame;
	int cancelPopupSelectedIndex;
	int cancelPopupAnimationFrames;
	bool cancelPopup;
	s8 prevDStickX;
	s8 prevDStickY;
	
	bool m_bCancelPatch;


	f32 fProgressPercentage;
	char sInfoText[255];
	bool bWorkRemaining;
	
};

#endif