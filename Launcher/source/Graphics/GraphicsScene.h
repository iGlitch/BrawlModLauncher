#ifndef GRAPHICSSCENE_H
#define GRAPHICSSCENE_H

#include <gccore.h>
#include <string.h>
#include <stdio.h>
#include "video.h"
#include "FreeTypeGX.h"

extern FreeTypeGX *fontSystem[];
extern u8 * configFileData;
extern int configFileSize;
extern u8 * infoFileData;
extern int infoFileSize;
extern wchar_t * newsText;
extern uint8_t ATTRIBUTE_ALIGN(32) wu_previousPadData[37];
extern uint8_t ATTRIBUTE_ALIGN(32) wu_currentPadData[37];
extern u16 wu_padDataSize;
extern bool networkLoaderNetBusy;

enum ESceneType
{
	SCENE_EXIT = -1,
	SCENE_MAIN_MENU = 0,
	SCENE_TOOLS = 1,
	SCENE_ADDONS = 2,
	SCENE_UPDATE = 3,
	SCENE_SETTINGS = 4,
	SCENE_LAUNCHTITLE = 5,
	SCENE_LAUNCHELF = 6,
	SCENE_TEST = 7,

};

const int STICK_DEADZONE = 25;

class GraphicsScene
{
public:
	virtual ~GraphicsScene() {}

	virtual void Load(){};
	virtual void Unload(){};
	virtual void Draw(){};
	virtual void HandleInputs(u32, s8, s8, s8, s8, u32, bool, bool){};

	virtual bool Work(){return true;};

	bool IsLoaded() const { return m_bIsLoaded; };
	virtual ESceneType GetNextSceneType() const { return m_eNextScreen; }

protected:
	void drawNewsBox(f32 & yPos, f32 offset, f32 height);
	void drawMenuItem(f32 x, f32 y, f32 itemWidth, f32 textureWidth, f32 textureHeight, int animateOnFrame, int animationFrames, GXTexObj *texture, bool selected);
	void drawInfoBox(f32 yPos, f32 height, const wchar_t * text);

	
	bool m_bIsLoaded;
	f32 m_fScreenWidth;
	f32 m_fScreenHeight;
	
	int m_iMenuSelectedIndex;
	int m_iMenuSelectionFrame;
	int m_iMenuSelectionAnimationFrames;

	f32 m_fMaxNewsScrollFrames;
	f32 m_fCurrentNewsScrollFrame;

	s8 m_sPrevDStickX;
	s8 m_sPrevDStickY;

	int m_iDrawFrameNumber;
	
	ESceneType m_eNextScreen;

private:
	void drawMenuItemInternal(f32 x, f32 y, f32 width, u8 red, u8 green, u8 blue);
};

#endif