#ifndef MAINMENUSCENE_H
#define MAINMENUSCENE_H

#include "GraphicsScene.h"
#include "video.h"
#include "stdlib.h"
#include "textures.h"
#include "Popup.h"

class CMainMenuScene : public GraphicsScene
{
public:
	CMainMenuScene(f32 w, f32 h);
	virtual void Draw();
	virtual void Load();
	virtual void Unload();
	virtual void HandleInputs(u32 gcPressed, s8 dStickX, s8 dStickY, s8 cStickX, s8 cStickY, u32 wiiPressed, bool powerButtonPressed, bool resetButtonPressed);

	virtual void Focus() { }; //not used
	virtual bool Work()  { return false; }; //not used

private:
	CMainMenuScene() { }

	Popup * badBootPopup;
	bool showBadBootPopup;	

	Popup * installPopup;
	bool showInstallPopup;
	
	Popup * aboutPopup;
	bool showAboutPopup;
	wchar_t * aboutLine1Text;
	wchar_t * aboutLine2Text;	

	bool m_bIsFromHBC;

};

#endif