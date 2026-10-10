/*----------------------------------------------------------
OpenKey - The Cross platform Open source Vietnamese Keyboard application.

Copyright (C) 2019 Mai Vu Tuyen
Contact: maivutuyen.91@gmail.com
Github: https://github.com/tuyenvm/OpenKey
Fanpage: https://www.facebook.com/OpenKeyVN

This file is belong to the OpenKey project, Win32 version
which is released under GPL license.
You can fork, modify, improve this program. If you
redistribute your new version, it MUST be open source.
-----------------------------------------------------------*/
#pragma once

// Central settings bridge shared by the legacy Win32 UI and the future WinUI UI.
// Registry key names intentionally remain identical to the existing global
// variable names so upgrades keep every user preference.
class SettingsService {
public:
	struct Preferences {
		int language = 1;
		int inputType = 0;
		int codeTable = 0;
		int checkSpelling = 1;
		int restoreIfWrongSpelling = 1;
		int rememberCode = 1;
		int useModernOrthography = 0;
		int quickTelex = 0;
		int quickStartConsonant = 0;
		int quickEndConsonant = 0;
		int autoCapsMacro = 0;
		int switchKeyStatus = 0x5A00025A; // Alt + Z
		int fixRecommendBrowser = 1;
		int sendKeyStepByStep = 1;
		int useSmartSwitchKey = 1;
		int supportMetroApp = 0;
		int otherLanguage = 1;
		int tempOffOpenKey = 0;
		int fixChromiumBrowser = 0;
		int upperCaseFirstChar = 0;
		int allowConsonantZFWJ = 0;
		int tempOffSpelling = 0;
			int useGrayIcon = 0;
			int runWithWindows = 1;
			int showOnStartUp = 1;
		int checkNewVersion = 0;
		int convertToolDontAlertWhenCompleted = 0;
		int convertToolToAllCaps = 0;
		int convertToolToAllNonCaps = 0;
		int convertToolToCapsFirstLetter = 0;
		int convertToolToCapsEachWord = 0;
		int convertToolRemoveMark = 0;
		int convertToolFromCode = 0;
		int convertToolToCode = 0;
		int convertToolHotKey = 0xFE0000FE;
	};

	static constexpr int DefaultSwitchKeyStatus = 0x5A00025A; // Alt + Z
	static constexpr int EmptyHotKey = 0xFE0000FE;

	static Preferences readPreferences();
	static bool savePreferences(const Preferences& preferences);
	// Write only fields differing from the page snapshot. Advance it on success;
	// on failure keep it for retry. Multiple changed fields are not transactional.
	static bool saveChangedPreferences(Preferences& baseline, const Preferences& preferences);
	static void load();
	static void restoreDefaults();
};
