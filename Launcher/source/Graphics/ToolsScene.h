#ifndef TOOLSSCENE_H
#define TOOLSSCENE_H
#include "GraphicsScene.h"
#include "video.h"
#include "stdlib.h"
#include "Popup.h"

class CToolsScene : public GraphicsScene
{
public:
	CToolsScene(f32 w, f32 h);
	virtual void Draw();
	virtual void Load();
	virtual void Unload();
	virtual void HandleInputs(u32 gcPressed, s8 dStickX, s8 dStickY, s8 cStickX, s8 cStickY, u32 wiiPressed, bool powerButtonPressed, bool resetButtonPressed);

	virtual void Focus() { }; //not used
	virtual bool Work()  { return false; }; //not used

private:
	CToolsScene() {}
	
	Popup * aboutPopup;
	bool showAboutPopup;
	wchar_t * aboutLine2Text;	

	
};

#endif