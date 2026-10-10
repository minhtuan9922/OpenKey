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
#include "AppDelegate.h"
#include "SettingsService.h"
#include "UpdatePolicy.h"
#include "../../../engine/Macro.h"

static AppDelegate* _instance;

namespace {
	bool launchModernControlPanel() {
		std::vector<std::wstring> candidates;
			constexpr DWORD pathCapacity = 32768;
			std::wstring configuredPath(pathCapacity, L'\0');
			const DWORD configuredLength = GetEnvironmentVariableW(
				L"OPENKEY_UI_PATH", &configuredPath[0], pathCapacity);
			if (configuredLength > 0 && configuredLength < pathCapacity) {
				configuredPath.resize(configuredLength);
			candidates.emplace_back(configuredPath);
		}

			std::wstring modulePath(pathCapacity, L'\0');
			const DWORD moduleLength = GetModuleFileNameW(nullptr, &modulePath[0], pathCapacity);
			if (moduleLength == 0 || moduleLength >= pathCapacity) {
			return false;
		}

			modulePath.resize(moduleLength);
			std::wstring directory = std::move(modulePath);
		const size_t separator = directory.find_last_of(L"\\/");
		if (separator == std::wstring::npos) {
			return false;
		}
		directory.resize(separator);
		candidates.push_back(directory + L"\\OpenKey.UI.exe");
		candidates.push_back(directory + L"\\OpenKey.UI\\OpenKey.UI.exe");

#ifdef _DEBUG
		constexpr wchar_t configuration[] = L"Debug";
#else
		constexpr wchar_t configuration[] = L"Release";
#endif
#ifdef _WIN64
		candidates.push_back(directory + L"\\..\\..\\OpenKey.UI\\x64\\" + configuration +
			L"\\OpenKey.UI\\OpenKey.UI.exe");
#else
		candidates.push_back(directory + L"\\..\\OpenKey.UI\\" + configuration +
			L"\\OpenKey.UI\\OpenKey.UI.exe");
#endif

		for (const auto& candidate : candidates) {
				std::wstring fullPath(pathCapacity, L'\0');
				const DWORD length = GetFullPathNameW(candidate.c_str(), pathCapacity, &fullPath[0], nullptr);
				if (length == 0 || length >= pathCapacity) {
				continue;
			}
				fullPath.resize(length);
				const DWORD attributes = GetFileAttributesW(fullPath.c_str());
			if (attributes == INVALID_FILE_ATTRIBUTES || (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
				continue;
			}

			const auto result = reinterpret_cast<INT_PTR>(
					ShellExecuteW(nullptr, L"open", fullPath.c_str(), nullptr, nullptr, SW_SHOWNORMAL));
			if (result > 32) {
				return true;
			}
		}
		return false;
	}
}

//see document in Engine.h
int vLanguage = 1;
int vInputType = 0;
int vFreeMark = 0;
int vCodeTable = 0;
int vCheckSpelling = 1;
int vUseModernOrthography = 1;
int vQuickTelex = 0;
int vSwitchKeyStatus = SettingsService::DefaultSwitchKeyStatus;
int vRestoreIfWrongSpelling = 1;
int vFixRecommendBrowser = 0;
int vUseMacro = 1;
int vUseMacroInEnglishMode = 1;
int vAutoCapsMacro = 0;
int vSendKeyStepByStep = 1;
int vUseSmartSwitchKey = 1;
int vUpperCaseFirstChar = 0;
int vTempOffSpelling = 0;
int vAllowConsonantZFWJ = 0;
int vQuickStartConsonant = 0;
int vQuickEndConsonant = 0;
int vOtherLanguage = 1;
int vRememberCode = 1;
int vTempOffOpenKey = 0;

int vUseGrayIcon = 0;
int vShowOnStartUp = 0;
int vRunWithWindows = 1;

int vSupportMetroApp = 1;
int vCreateDesktopShortcut = 0;
int vRunAsAdmin = 0;
int vCheckNewVersion = 0;
//beta feature
int vFixChromiumBrowser = 0; //new on version 2.0

bool AppDelegate::isDialogMsg(MSG & msg) const {
	return (mainDialog != NULL && IsDialogMessage(mainDialog->getHwnd(), &msg)) ||
		(macroDialog != NULL && IsDialogMessage(macroDialog->getHwnd(), &msg)) || 
		(convertDialog != NULL && IsDialogMessage(convertDialog->getHwnd(), &msg)) || 
		(aboutDialog != NULL && IsDialogMessage(aboutDialog->getHwnd(), &msg));
}

void AppDelegate::checkUpdate() {
	string newVersion;
	if (OpenKeyManager::checkUpdate(newVersion)) {
		WCHAR msg[256];
		wsprintf(msg,
			TEXT("OpenKey Có phiên bản mới (%s), bạn có muốn cập nhật không?"),
			utf8ToWideString(newVersion).c_str());

		int msgboxID = MessageBox(
			0,
			msg,
			_T("OpenKey Update"),
			MB_ICONEXCLAMATION | MB_YESNO
		);
			if (msgboxID == IDYES) {
				if (!OpenKeyUpdatePolicy::CanUseLegacyUpdater()) {
					OpenKeyUpdatePolicy::ShowManualUpdateNotice(nullptr);
					return;
				}
			//Call OpenKeyUpdate
			WCHAR path[MAX_PATH];
			GetCurrentDirectory(MAX_PATH, path);
			wsprintf(path, TEXT("%s\\OpenKeyUpdate.exe"), path);
			ShellExecute(0, L"", path, 0, 0, SW_SHOWNORMAL);
			AppDelegate::getInstance()->onOpenKeyExit();
		}

	}
}

AppDelegate::AppDelegate() {
	_instance = this;
}

AppDelegate * AppDelegate::getInstance() {
	return _instance;
}

int AppDelegate::run(HINSTANCE hInstance) {
	this->hInstance = hInstance;

	//check app has already run or not
	HWND previousInstance = FindWindow(APP_CLASS, NULL);
	if (previousInstance) {
		MessageBeep(MB_OK);
		SendMessage(previousInstance, WM_USER + 2019, 0, 0);
		PostQuitMessage(0);
		return 0;
	}

	//init OpenKey Engine
	OpenKeyManager::initEngine();

	//create system tray
	SystemTrayHelper::createSystemTrayIcon(hInstance);
	SystemTrayHelper::updateData();

	//create main control
	if (vShowOnStartUp)
		onControlPanel();
	MessageBeep(MB_OK);

	//check update
	if (vCheckNewVersion)
		checkUpdate();

	MSG msg;
	// Main message loop:
	while (GetMessage(&msg, nullptr, 0, 0))	{
		if (msg.message == WM_KEYDOWN) {
			OpenKeyManager::_lastKeyCode = (UINT16)msg.wParam;
		}
		if (!isDialogMsg(msg)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}
	return 0;
}

void AppDelegate::createMainDialog() {
	if (mainDialog == NULL) {
		mainDialog = new MainControlDialog(hInstance, IDD_DIALOG_MAIN);
		mainDialog->show();
	} else {
		mainDialog->bringOnTop();
	}
}

void AppDelegate::closeDialog(BaseDialog * dialog) {
	dialog->closeDialog();
	if (mainDialog == dialog) {
		delete mainDialog;
		mainDialog = NULL;
	} else if (aboutDialog == dialog) {
		delete aboutDialog;
		aboutDialog = NULL;
	} else if (macroDialog == dialog) {
		delete macroDialog;
		macroDialog = NULL;
	} else if (convertDialog == dialog) {
		delete convertDialog;
		convertDialog = NULL;
	}
}

void AppDelegate::onInputMethodChangedFromHotKey() {
	APP_SET_DATA(vLanguage, vLanguage);
	if (mainDialog) {
		mainDialog->fillData();
	}
	SystemTrayHelper::updateData();
}

void AppDelegate::onSettingsChangedFromUI() {
	const int previousCodeTable = vCodeTable;
	const int previousSpelling = vCheckSpelling;
	const int previousRunWithWindows = vRunWithWindows;
	SettingsService::load();

	if (previousCodeTable != vCodeTable) {
		onTableCodeChange();
	}
	if (previousSpelling != vCheckSpelling) {
		vSetCheckSpelling();
	}
	if (previousRunWithWindows != vRunWithWindows) {
		OpenKeyHelper::registerRunOnStartup(vRunWithWindows);
	}
	if (mainDialog) {
		mainDialog->fillData();
	}
	SystemTrayHelper::updateData();
}

void AppDelegate::onDefaultConfig() {
	SettingsService::restoreDefaults();

	if (mainDialog) {
		mainDialog->fillData();
	}
	SystemTrayHelper::updateData();
}

void AppDelegate::onToggleVietnamese() {
	APP_SET_DATA(vLanguage, vLanguage ? 0 : 1);
	if (mainDialog) {
		mainDialog->fillData();
	}
	
	if (vUseSmartSwitchKey) {
		string& exe = OpenKeyHelper::getLastAppExecuteName();
		setAppInputMethodStatus(exe, vLanguage | (vCodeTable << 1));
		saveSmartSwitchKeyData();
	}
}

void AppDelegate::onToggleCheckSpelling() {
	APP_SET_DATA(vCheckSpelling, vCheckSpelling ? 0 : 1);
	if (mainDialog) {
		mainDialog->fillData();
	}
	vSetCheckSpelling();
}

void AppDelegate::onToggleUseSmartSwitchKey() {
	APP_SET_DATA(vUseSmartSwitchKey, vUseSmartSwitchKey ? 0 : 1);
	if (mainDialog) {
		mainDialog->fillData();
	}
}

void AppDelegate::onToggleUseMacro() {
	APP_SET_DATA(vUseMacro, vUseMacro ? 0 : 1);
	if (mainDialog) {
		mainDialog->fillData();
	}
}

void AppDelegate::onMacroTable() {
	if (macroDialog == NULL) {
		macroDialog = new MacroDialog(hInstance, IDD_DIALOG_MACRO);
		macroDialog->show();
	} else {
		macroDialog->bringOnTop();
	}
}

void AppDelegate::onConvertTool() {
	if (convertDialog == NULL) {
		convertDialog = new ConvertToolDialog(hInstance, IDD_DIALOG_CONVERT_TOOL);
		convertDialog->show();
	} else {
		convertDialog->bringOnTop();
	}
}

void AppDelegate::onQuickConvert() {
	if (OpenKeyHelper::quickConvert()) {
		// Keep quick conversion non-modal so the global hotkey never blocks typing.
		if (!convertToolDontAlertWhenCompleted) {
			TCHAR msg[256];
			LoadString(hInstance, IDS_STRING_CONVERT_COMPLETED, msg, 256);
			SystemTrayHelper::showNotification(_T("OpenKey"), msg);
		}
	}
}

void AppDelegate::onInputType(const int & type) {
	APP_SET_DATA(vInputType, type);
	if (mainDialog) {
		mainDialog->fillData();
	}
}

void AppDelegate::onTableCode(const int & code) {
	APP_SET_DATA(vCodeTable, code);
	if (mainDialog) {
		mainDialog->fillData();
	}
	if (vRememberCode) {
		setAppInputMethodStatus(OpenKeyHelper::getFrontMostAppExecuteName(), vLanguage | (vCodeTable << 1));
		saveSmartSwitchKeyData();
	}
}

void AppDelegate::onControlPanel() {
	if (!launchModernControlPanel()) {
		createMainDialog();
	}
}

void AppDelegate::onOpenKeyAbout() {
	if (aboutDialog == NULL) {
		aboutDialog = new AboutDialog(hInstance, IDD_ABOUTBOX);
		aboutDialog->show();
	} else {
		aboutDialog->bringOnTop();
	}
}

void AppDelegate::onOpenKeyExit() {
	OpenKeyManager::freeEngine();
	SystemTrayHelper::removeSystemTray();
	PostQuitMessage(0);
}
