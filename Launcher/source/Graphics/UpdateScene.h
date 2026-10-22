#ifndef CUPDATESCENE_H
#define CUPDATESCENE_H

#include <vector>
#include "GraphicsScene.h"
#include "video.h"
#include "stdlib.h"
#include "Popup.h"


class CUpdateScene : public GraphicsScene
{
public:
	CUpdateScene(f32 w, f32 h);
	~CUpdateScene();

	virtual void Draw();
	virtual void Load();
	virtual void Unload();
	virtual bool Work();
	virtual void HandleInputs(u32 gcPressed, s8 dStickX, s8 dStickY, s8 cStickX, s8 cStickY, u32 wiiPressed, bool powerButtonPressed, bool resetButtonPressed);

	virtual void Focus() { }; //not used
	f32 fProgressPercentage;
	bool m_bCancelUpdate;
	bool m_bRestartLauncher;
	bool m_bInstallUpdate;
	bool m_bForcePMVersion300;
	wchar_t * sInfoText;

private:
	CUpdateScene() {}

	void drawProgressBar();
	
	Popup * infoFileMissingPopup;
	bool showInfoFileMissingPopup;

	Popup * cancelPopup;
	bool showCancelPopup;
	
	Popup * noUpdatePopup;
	bool showNoUpdatePopup;
	
	Popup * confirmUpdatePopup;
	bool showConfirmUpdatePopup;
	wchar_t * confirmUpdateLine3Text;

	Popup * confirmLaunchPopup;
	bool showConfirmLaunchPopup;

	Popup * patchVerificationFailedPopup;
	bool showPatchVerificationFailedPopup;

	

	void SleepDuringWork(int milliseconds);
};

#endif