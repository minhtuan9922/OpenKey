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
#include "stdafx.h"
#include "SettingsService.h"
#include "../../../engine/ConvertTool.h"

namespace {
	template<typename T>
	void loadSetting(const wchar_t* key, T& destination, const int defaultValue) {
		destination = static_cast<T>(OpenKeyHelper::getRegInt(key, defaultValue));
	}

	template<typename T>
	void saveSetting(const wchar_t* key, T& destination, const int value) {
		destination = static_cast<T>(value);
		OpenKeyHelper::setRegInt(key, value);
	}
}

void SettingsService::load() {
	const Preferences preferences = readPreferences();
	vLanguage = preferences.language;
	vInputType = preferences.inputType;
	vFreeMark = 0;
	vCodeTable = preferences.codeTable;
	vCheckSpelling = preferences.checkSpelling;
	vUseModernOrthography = preferences.useModernOrthography;
	vQuickTelex = preferences.quickTelex;
	loadSetting(L"vSwitchKeyStatus", vSwitchKeyStatus, 0x7A000206);
	vRestoreIfWrongSpelling = preferences.restoreIfWrongSpelling;
	loadSetting(L"vFixRecommendBrowser", vFixRecommendBrowser, 1);
	loadSetting(L"vUseMacro", vUseMacro, 1);
	loadSetting(L"vUseMacroInEnglishMode", vUseMacroInEnglishMode, 0);
	vAutoCapsMacro = preferences.autoCapsMacro;
	loadSetting(L"vSendKeyStepByStep", vSendKeyStepByStep, 1);
	loadSetting(L"vUseGrayIcon", vUseGrayIcon, 0);
	loadSetting(L"vShowOnStartUp", vShowOnStartUp, 1);
	loadSetting(L"vRunWithWindows", vRunWithWindows, 1);
	loadSetting(L"vUseSmartSwitchKey", vUseSmartSwitchKey, 1);
	loadSetting(L"vUpperCaseFirstChar", vUpperCaseFirstChar, 0);
	loadSetting(L"vAllowConsonantZFWJ", vAllowConsonantZFWJ, 0);
	loadSetting(L"vTempOffSpelling", vTempOffSpelling, 0);
	vQuickStartConsonant = preferences.quickStartConsonant;
	vQuickEndConsonant = preferences.quickEndConsonant;
	loadSetting(L"vSupportMetroApp", vSupportMetroApp, 0);
	loadSetting(L"vRunAsAdmin", vRunAsAdmin, 0);
	loadSetting(L"vCreateDesktopShortcut", vCreateDesktopShortcut, 0);
	loadSetting(L"vCheckNewVersion", vCheckNewVersion, 0);
	vRememberCode = preferences.rememberCode;
	loadSetting(L"vOtherLanguage", vOtherLanguage, 1);
	loadSetting(L"vTempOffOpenKey", vTempOffOpenKey, 0);
	loadSetting(L"vFixChromiumBrowser", vFixChromiumBrowser, 0);

	convertToolDontAlertWhenCompleted = preferences.convertToolDontAlertWhenCompleted != 0;
	convertToolToAllCaps = preferences.convertToolToAllCaps != 0;
	convertToolToAllNonCaps = preferences.convertToolToAllNonCaps != 0;
	convertToolToCapsFirstLetter = preferences.convertToolToCapsFirstLetter != 0;
	convertToolToCapsEachWord = preferences.convertToolToCapsEachWord != 0;
	convertToolRemoveMark = preferences.convertToolRemoveMark != 0;
	convertToolFromCode = static_cast<Uint8>(preferences.convertToolFromCode);
	convertToolToCode = static_cast<Uint8>(preferences.convertToolToCode);
	convertToolHotKey = preferences.convertToolHotKey;

	if (convertToolHotKey == 0) {
		convertToolHotKey = EmptyHotKey;
	}
}

void SettingsService::restoreDefaults() {
	saveSetting(L"vLanguage", vLanguage, 1);
	saveSetting(L"vInputType", vInputType, 0);
	vFreeMark = 0;
	saveSetting(L"vCodeTable", vCodeTable, 0);
	saveSetting(L"vCheckSpelling", vCheckSpelling, 1);
	saveSetting(L"vUseModernOrthography", vUseModernOrthography, 0);
	saveSetting(L"vQuickTelex", vQuickTelex, 0);
	saveSetting(L"vSwitchKeyStatus", vSwitchKeyStatus, DefaultSwitchKeyStatus);
	saveSetting(L"vRestoreIfWrongSpelling", vRestoreIfWrongSpelling, 1);
	saveSetting(L"vFixRecommendBrowser", vFixRecommendBrowser, 1);
	saveSetting(L"vUseMacro", vUseMacro, 0);
	saveSetting(L"vUseMacroInEnglishMode", vUseMacroInEnglishMode, 0);
	saveSetting(L"vAutoCapsMacro", vAutoCapsMacro, 0);
	saveSetting(L"vSendKeyStepByStep", vSendKeyStepByStep, 1);
	saveSetting(L"vUseSmartSwitchKey", vUseSmartSwitchKey, 1);
	saveSetting(L"vUpperCaseFirstChar", vUpperCaseFirstChar, 0);
	saveSetting(L"vAllowConsonantZFWJ", vAllowConsonantZFWJ, 0);
	saveSetting(L"vTempOffSpelling", vTempOffSpelling, 0);
	saveSetting(L"vUseGrayIcon", vUseGrayIcon, 0);
	saveSetting(L"vShowOnStartUp", vShowOnStartUp, 1);
	saveSetting(L"vRunWithWindows", vRunWithWindows, 1);
	saveSetting(L"vSupportMetroApp", vSupportMetroApp, 1);
	saveSetting(L"vRememberCode", vRememberCode, 1);
	saveSetting(L"vOtherLanguage", vOtherLanguage, 1);
	saveSetting(L"vTempOffOpenKey", vTempOffOpenKey, 0);
	saveSetting(L"vFixChromiumBrowser", vFixChromiumBrowser, 0);
	saveSetting(L"convertToolDontAlertWhenCompleted", convertToolDontAlertWhenCompleted, 0);
	saveSetting(L"convertToolToAllCaps", convertToolToAllCaps, 0);
	saveSetting(L"convertToolToAllNonCaps", convertToolToAllNonCaps, 0);
	saveSetting(L"convertToolToCapsFirstLetter", convertToolToCapsFirstLetter, 0);
	saveSetting(L"convertToolToCapsEachWord", convertToolToCapsEachWord, 0);
	saveSetting(L"convertToolRemoveMark", convertToolRemoveMark, 0);
	saveSetting(L"convertToolFromCode", convertToolFromCode, 0);
	saveSetting(L"convertToolToCode", convertToolToCode, 0);
	saveSetting(L"convertToolHotKey", convertToolHotKey, EmptyHotKey);
}
