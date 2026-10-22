#ifndef SETTINGSSCENE_H
#define SETTINGSSCENE_H

#include "GraphicsScene.h"
#include <vector>
#include <string>
#include "Popup.h"

enum ESettingType
{
	SETTING_LOAD_METHOD = 0,
	SETTING_AUTO_BOOT,
	SETTING_VIDEO_MODE,
	SETTING_DEFLICKER,
	SETTING_GAME_ASPECT_RATIO,
	SETTING_LANGUAGE,
	SETTING_LOADERIOS_CIOS,
	SETTING_USBPORT,
	SETTING_MULTIPLE_PARTITIONS,
	SETTING_NANDEMUMODE,
	SETTING_NANDEMUPATH,
	SETTING_DEBUG_TO_FILE,
	SETTING_RESET_TO_DEFAULTS,
	SETTING_COUNT
};

enum ELanguage
{
	LANGUAGE_JAPANESE = 0,
	LANGUAGE_ENGLISH,
	LANGUAGE_GERMAN,
	LANGUAGE_FRENCH,
	LANGUAGE_SPANISH,
	LANGUAGE_ITALIAN,
	LANGUAGE_DUTCH,
	LANGUAGE_S_CHINESE,
	LANGUAGE_T_CHINESE,
	LANGUAGE_KOREAN,
	LANGUAGE_CONSOLE_DEFAULT,
	LANGUAGE_MAX
};

struct SSetting
{
	ESettingType type;
	std::string name;
	std::string description;
	int currentValue;
	std::vector<std::string> options;
	std::vector<int> optionValues;
};

class CSettingsScene : public GraphicsScene
{
public:
	CSettingsScene(f32 w, f32 h);
	virtual ~CSettingsScene();

	virtual void Load();
	virtual void Unload();
	virtual void Draw();
	virtual void HandleInputs(u32 gcPressed, s8 stickX, s8 stickY, s8 substickX, s8 substickY, u32 wiiPressed, bool powerButtonPressed, bool resetButtonPressed);

private:
	void InitializeSettings();
	void LoadLoaderCfg();
	void LoadConfigXml();
	void SaveSettings();
	void CycleSettingValue(int index);
	void ResetSettingsToDefaults();
	const wchar_t* GetCurrentValueText(int index);
	const char* GetDynamicDescription(int index);
	void drawSelectionMenu(float yPos);
	void UpdateScrollbarState(f32 menuY, f32 menuHeight);
	void drawScrollbar(f32 menuX, f32 menuY, f32 menuWidth, f32 menuHeight, f32 animationRatio);

	int GetStoredValue(int settingIndex) const;
	int FindDisplayIndex(int settingIndex, int storedValue) const;

	std::vector<SSetting> m_vSettings;

	wchar_t m_wszInfoText[512];
	wchar_t m_wszValueBuffer[64];
	char m_szDescriptionBuffer[512];

	bool m_bShowScrollbar;
	f32 m_fScrollbarHeight;
	f32 m_fScrollbarY;

	Popup * autoBootInfoPopup;
	bool showAutoBootInfoPopup;
	bool showResetMessage;
};

#endif
