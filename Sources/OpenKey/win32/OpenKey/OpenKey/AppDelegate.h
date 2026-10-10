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
#include "stdafx.h"
#include "MainControlDialog.h"
#include "AboutDialog.h"
#include "ConvertToolDialog.h"
#include "MacroDialog.h"
#include "AsyncUpdateCheck.h"

class BaseDialog;
class AppDelegate {
private:
	HINSTANCE hInstance = nullptr;
	BaseDialog* mainDialog = NULL, *aboutDialog = NULL, *macroDialog = NULL, *convertDialog = NULL;
private:
	bool isDialogMsg(MSG &msg) const;
	void checkUpdate();
	AsyncUpdateCheck updateCheck;
	UINT_PTR updateTimer = 0;
	HWND updateOwner = nullptr, updateButton = nullptr;
	void* updateToken = nullptr;
	bool manualUpdateCheck = false;
	bool exiting = false;
	void stopUpdateCheck();
public:
	AppDelegate();
	static AppDelegate* getInstance();
	int run(HINSTANCE hInstance);
	void createMainDialog();
	void closeDialog(BaseDialog* dialog);
	void beginUpdateCheck(HWND owner = nullptr, HWND button = nullptr);
	void pollUpdateCheck(UINT_PTR timer);
public: //event
	void onInputMethodChangedFromHotKey();
	void onSettingsChangedFromUI();
	void onDefaultConfig();

	void onToggleVietnamese();
	void onToggleCheckSpelling();
	void onToggleUseSmartSwitchKey();
	void onToggleUseMacro();

	void onMacroTable();
	void onConvertTool();
	void onQuickConvert();

	void onInputType(const int& type);
	void onTableCode(const int& code);

	void onControlPanel();
	void onOpenKeyAbout();
	void onOpenKeyExit();
};
