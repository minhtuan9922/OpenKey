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
#include <windows.h>

#include "SettingsService.h"

namespace {
	constexpr wchar_t RegistryPath[] = L"SOFTWARE\\TuyenMai\\OpenKey";
	constexpr wchar_t LegacyWindowClass[] = L"OpenKeyVietnameseInputMethod";
	constexpr UINT PreferencesChangedMessage = WM_USER + 2020;

	int readDword(HKEY key, const wchar_t* name, const int defaultValue) {
		DWORD value = 0;
		DWORD size = sizeof(value);
		DWORD type = 0;
		if (RegQueryValueExW(key, name, nullptr, &type,
			reinterpret_cast<LPBYTE>(&value), &size) != ERROR_SUCCESS ||
			type != REG_DWORD || size != sizeof(value)) {
			return defaultValue;
		}

		return static_cast<int>(value);
	}

	bool writeDword(HKEY key, const wchar_t* name, const int value) {
		const DWORD storedValue = static_cast<DWORD>(value);
		return RegSetValueExW(key, name, 0, REG_DWORD,
			reinterpret_cast<const BYTE*>(&storedValue), sizeof(storedValue)) == ERROR_SUCCESS;
	}
}

SettingsService::Preferences SettingsService::readPreferences() {
	Preferences preferences;
	HKEY key = nullptr;
	if (RegOpenKeyExW(HKEY_CURRENT_USER, RegistryPath, 0, KEY_READ, &key) != ERROR_SUCCESS) {
		return preferences;
	}

	preferences.language = readDword(key, L"vLanguage", preferences.language);
	preferences.inputType = readDword(key, L"vInputType", preferences.inputType);
	preferences.codeTable = readDword(key, L"vCodeTable", preferences.codeTable);
	preferences.checkSpelling = readDword(key, L"vCheckSpelling", preferences.checkSpelling);
	preferences.restoreIfWrongSpelling = readDword(key, L"vRestoreIfWrongSpelling", preferences.restoreIfWrongSpelling);
	preferences.rememberCode = readDword(key, L"vRememberCode", preferences.rememberCode);
	preferences.useModernOrthography = readDword(key, L"vUseModernOrthography", preferences.useModernOrthography);
	preferences.quickTelex = readDword(key, L"vQuickTelex", preferences.quickTelex);
	preferences.quickStartConsonant = readDword(key, L"vQuickStartConsonant", preferences.quickStartConsonant);
	preferences.quickEndConsonant = readDword(key, L"vQuickEndConsonant", preferences.quickEndConsonant);
	preferences.autoCapsMacro = readDword(key, L"vAutoCapsMacro", preferences.autoCapsMacro);
		preferences.useMacro = readDword(key, L"vUseMacro", preferences.useMacro);
		preferences.useMacroInEnglishMode = readDword(key, L"vUseMacroInEnglishMode", preferences.useMacroInEnglishMode);
	preferences.switchKeyStatus = readDword(key, L"vSwitchKeyStatus", preferences.switchKeyStatus);
	preferences.fixRecommendBrowser = readDword(key, L"vFixRecommendBrowser", preferences.fixRecommendBrowser);
	preferences.sendKeyStepByStep = readDword(key, L"vSendKeyStepByStep", preferences.sendKeyStepByStep);
	preferences.useSmartSwitchKey = readDword(key, L"vUseSmartSwitchKey", preferences.useSmartSwitchKey);
	preferences.supportMetroApp = readDword(key, L"vSupportMetroApp", preferences.supportMetroApp);
	preferences.otherLanguage = readDword(key, L"vOtherLanguage", preferences.otherLanguage);
	preferences.tempOffOpenKey = readDword(key, L"vTempOffOpenKey", preferences.tempOffOpenKey);
	preferences.fixChromiumBrowser = readDword(key, L"vFixChromiumBrowser", preferences.fixChromiumBrowser);
	preferences.upperCaseFirstChar = readDword(key, L"vUpperCaseFirstChar", preferences.upperCaseFirstChar);
	preferences.allowConsonantZFWJ = readDword(key, L"vAllowConsonantZFWJ", preferences.allowConsonantZFWJ);
	preferences.tempOffSpelling = readDword(key, L"vTempOffSpelling", preferences.tempOffSpelling);
	preferences.useGrayIcon = readDword(key, L"vUseGrayIcon", preferences.useGrayIcon);
	preferences.runWithWindows = readDword(key, L"vRunWithWindows", preferences.runWithWindows);
	preferences.showOnStartUp = readDword(key, L"vShowOnStartUp", preferences.showOnStartUp);
	preferences.checkNewVersion = readDword(key, L"vCheckNewVersion", preferences.checkNewVersion);
	preferences.convertToolDontAlertWhenCompleted = readDword(key, L"convertToolDontAlertWhenCompleted", preferences.convertToolDontAlertWhenCompleted);
	preferences.convertToolToAllCaps = readDword(key, L"convertToolToAllCaps", preferences.convertToolToAllCaps);
	preferences.convertToolToAllNonCaps = readDword(key, L"convertToolToAllNonCaps", preferences.convertToolToAllNonCaps);
	preferences.convertToolToCapsFirstLetter = readDword(key, L"convertToolToCapsFirstLetter", preferences.convertToolToCapsFirstLetter);
	preferences.convertToolToCapsEachWord = readDword(key, L"convertToolToCapsEachWord", preferences.convertToolToCapsEachWord);
	preferences.convertToolRemoveMark = readDword(key, L"convertToolRemoveMark", preferences.convertToolRemoveMark);
	preferences.convertToolFromCode = readDword(key, L"convertToolFromCode", preferences.convertToolFromCode);
	preferences.convertToolToCode = readDword(key, L"convertToolToCode", preferences.convertToolToCode);
	preferences.convertToolHotKey = readDword(key, L"convertToolHotKey", preferences.convertToolHotKey);

	RegCloseKey(key);
	return preferences;
}

namespace {
	struct PreferenceField {
		const wchar_t* name;
		int SettingsService::Preferences::* member;
	};
	constexpr PreferenceField PreferenceFields[] = {
		{ L"vLanguage", &SettingsService::Preferences::language },
			{ L"vUseMacro", &SettingsService::Preferences::useMacro },
			{ L"vUseMacroInEnglishMode", &SettingsService::Preferences::useMacroInEnglishMode },
		{ L"vInputType", &SettingsService::Preferences::inputType },
		{ L"vCodeTable", &SettingsService::Preferences::codeTable },
		{ L"vCheckSpelling", &SettingsService::Preferences::checkSpelling },
		{ L"vRestoreIfWrongSpelling", &SettingsService::Preferences::restoreIfWrongSpelling },
		{ L"vRememberCode", &SettingsService::Preferences::rememberCode },
		{ L"vUseModernOrthography", &SettingsService::Preferences::useModernOrthography },
		{ L"vQuickTelex", &SettingsService::Preferences::quickTelex },
		{ L"vQuickStartConsonant", &SettingsService::Preferences::quickStartConsonant },
		{ L"vQuickEndConsonant", &SettingsService::Preferences::quickEndConsonant },
		{ L"vAutoCapsMacro", &SettingsService::Preferences::autoCapsMacro },
		{ L"vSwitchKeyStatus", &SettingsService::Preferences::switchKeyStatus },
		{ L"vFixRecommendBrowser", &SettingsService::Preferences::fixRecommendBrowser },
		{ L"vSendKeyStepByStep", &SettingsService::Preferences::sendKeyStepByStep },
		{ L"vUseSmartSwitchKey", &SettingsService::Preferences::useSmartSwitchKey },
		{ L"vSupportMetroApp", &SettingsService::Preferences::supportMetroApp },
		{ L"vOtherLanguage", &SettingsService::Preferences::otherLanguage },
		{ L"vTempOffOpenKey", &SettingsService::Preferences::tempOffOpenKey },
		{ L"vFixChromiumBrowser", &SettingsService::Preferences::fixChromiumBrowser },
		{ L"vUpperCaseFirstChar", &SettingsService::Preferences::upperCaseFirstChar },
		{ L"vAllowConsonantZFWJ", &SettingsService::Preferences::allowConsonantZFWJ },
		{ L"vTempOffSpelling", &SettingsService::Preferences::tempOffSpelling },
		{ L"vUseGrayIcon", &SettingsService::Preferences::useGrayIcon },
		{ L"vRunWithWindows", &SettingsService::Preferences::runWithWindows },
		{ L"vShowOnStartUp", &SettingsService::Preferences::showOnStartUp },
		{ L"vCheckNewVersion", &SettingsService::Preferences::checkNewVersion },
		{ L"convertToolDontAlertWhenCompleted", &SettingsService::Preferences::convertToolDontAlertWhenCompleted },
		{ L"convertToolToAllCaps", &SettingsService::Preferences::convertToolToAllCaps },
		{ L"convertToolToAllNonCaps", &SettingsService::Preferences::convertToolToAllNonCaps },
		{ L"convertToolToCapsFirstLetter", &SettingsService::Preferences::convertToolToCapsFirstLetter },
		{ L"convertToolToCapsEachWord", &SettingsService::Preferences::convertToolToCapsEachWord },
		{ L"convertToolRemoveMark", &SettingsService::Preferences::convertToolRemoveMark },
		{ L"convertToolFromCode", &SettingsService::Preferences::convertToolFromCode },
		{ L"convertToolToCode", &SettingsService::Preferences::convertToolToCode },
		{ L"convertToolHotKey", &SettingsService::Preferences::convertToolHotKey },
	};

	bool savePreferenceFields(const SettingsService::Preferences& preferences,
		const SettingsService::Preferences* baseline) {
		bool changed = baseline == nullptr;
		if (baseline) {
			for (const auto& field : PreferenceFields) {
				if (preferences.*field.member != (*baseline).*field.member) {
					changed = true;
					break;
				}
			}
		}
		// A no-op must not create a key or trigger an engine reload.
		if (!changed) return true;

		HKEY key = nullptr;
		DWORD disposition = 0;
		if (RegCreateKeyExW(HKEY_CURRENT_USER, RegistryPath, 0, nullptr, REG_OPTION_NON_VOLATILE,
			KEY_SET_VALUE, nullptr, &key, &disposition) != ERROR_SUCCESS) return false;

		bool saved = true;
		for (const auto& field : PreferenceFields) {
			if (!baseline || preferences.*field.member != (*baseline).*field.member) {
				saved = writeDword(key, field.name, preferences.*field.member) && saved;
			}
		}
		RegCloseKey(key);
		if (saved) {
			if (const HWND legacyWindow = FindWindowW(LegacyWindowClass, nullptr)) {
				PostMessageW(legacyWindow, PreferencesChangedMessage, 0, 0);
			}
		}
		return saved;
	}
}

bool SettingsService::savePreferences(const Preferences& preferences) {
	return savePreferenceFields(preferences, nullptr);
}

bool SettingsService::saveChangedPreferences(Preferences& baseline, const Preferences& preferences) {
	if (!savePreferenceFields(preferences, &baseline)) return false;
	baseline = preferences;
	return true;
}
