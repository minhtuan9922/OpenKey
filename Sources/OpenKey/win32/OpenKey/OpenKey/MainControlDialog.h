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
#include "BaseDialog.h"

class MainControlDialog : public BaseDialog {
private:
	HWND hTab = nullptr;
	HWND hTabPage1 = nullptr;
	HWND hTabPage2 = nullptr;
	HWND hTabPage3 = nullptr;
	HWND hTabPage4 = nullptr;
	HWND comboBoxInputType = nullptr;
	HWND comboBoxTableCode = nullptr;
	HWND checkCtrl = nullptr;
	HWND checkAlt = nullptr;
	HWND checkWin = nullptr;
	HWND checkShift = nullptr;
	HWND textSwitchKey = nullptr;
	HWND checkBeep = nullptr;
	HWND checkVietnamese = nullptr;
	HWND checkEnglish = nullptr;
	HWND checkModernOrthorgraphy = nullptr;
	HWND checkFixRecommendBrowser = nullptr;
	HWND checkShowOnStartup = nullptr;
	HWND checkRunWithWindows = nullptr;
	HWND checkSpelling = nullptr;
	HWND checkRestoreIfWrongSpelling = nullptr;
	HWND checkUseClipboard = nullptr;
	HWND checkModernIcon = nullptr;
	HWND checkAllowZWJF = nullptr;
	HWND checkTempOffSpelling = nullptr;
	HWND checkQuickStartConsonant = nullptr;
	HWND checkQuickEndConsonant = nullptr;
	HWND checkSmartSwitchKey = nullptr;
	HWND checkCapsFirstChar = nullptr;
	HWND checkQuickTelex = nullptr;
	HWND checkUseMacro = nullptr;
	HWND checkUseMacroInEnglish = nullptr;
	HWND checkCreateDesktopShortcut = nullptr;
	HWND checkCheckNewVersion = nullptr;
	HWND checkRunAsAdmin = nullptr;
	HWND checkSupportMetroApp = nullptr;
	HWND checkMacroAutoCaps = nullptr;
	HWND checkFixChromium = nullptr;
	HWND checkRememberTableCode = nullptr;
	HWND checkTempOffOpenKey = nullptr;
	HWND checkAllowOtherLanguages = nullptr;
	HWND hUpdateButton = nullptr;
private:
	void initDialog();
	void onComboBoxSelected(const HWND& hCombobox, const int& comboboxId);
	void onCheckboxClicked(const HWND& hWnd);
	void onCharacter(const HWND& hWnd, const UINT16& keyCode);
	void setSwitchKeyText(const HWND& hWnd, const UINT16 & keyCode);
	void onTabIndexChanged();
	void onUpdateButton();
	void requestRestartAsAdmin();
protected:
	INT_PTR eventProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);
	static INT_PTR CALLBACK tabPageEventProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);
public:
	MainControlDialog(const HINSTANCE& hInstance, const int& resourceId);
	virtual ~MainControlDialog();
	virtual void fillData() override;
	void setSwitchKey(const unsigned short& code);

	friend INT_PTR CALLBACK tabPageEventProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);
};

