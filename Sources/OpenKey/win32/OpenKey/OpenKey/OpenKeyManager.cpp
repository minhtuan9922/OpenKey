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
#include "OpenKeyManager.h"
#include <shlobj.h>
#include <wrl/client.h>
#include "UpdateService.h"

static vector<LPCTSTR> _inputType = {
	_T("Telex"),
	_T("VNI"),
	_T("Simple Telex"),
};

static vector<LPCTSTR> _tableCode = {
	_T("Unicode"),
	_T("TCVN3 (ABC)"),
	_T("VNI Windows"),
	_T("Unicode Tổ hợp"),
	_T("Vietnamese Locale CP 1258")
};

/*-----------------------------------------------------------------------*/

extern void OpenKeyInit();
extern void OpenKeyFree();

unsigned short  OpenKeyManager::_lastKeyCode = 0;

vector<LPCTSTR>& OpenKeyManager::getInputType() {
	return _inputType;
}

vector<LPCTSTR>& OpenKeyManager::getTableCode() {
	return _tableCode;
}

void OpenKeyManager::initEngine() {
	OpenKeyInit();
}

void OpenKeyManager::freeEngine() {
	OpenKeyFree();
}

UpdateCheckService::Result OpenKeyManager::checkUpdate(string& newVersion) {
	const auto outcome = UpdateService::Check(OpenKeyHelper::getVersionNumber());
	newVersion = outcome.version;
	return outcome.result;
}

void OpenKeyManager::createDesktopShortcut() {
	const HRESULT initialized = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
	if (FAILED(initialized)) {
		return;
	}
	struct ComUninitializer {
		~ComUninitializer() { CoUninitialize(); }
	} uninitializer;
	// Release interfaces before balancing this function's COM initialization.
	Microsoft::WRL::ComPtr<IShellLink> shellLink;
	if (FAILED(CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER,
		IID_PPV_ARGS(shellLink.GetAddressOf())))) {
		return;
	}
	const wstring path = OpenKeyHelper::getFullPath();
	if (FAILED(shellLink->SetPath(path.c_str())) ||
		FAILED(shellLink->SetDescription(_T("OpenKey - Bộ gõ Tiếng Việt"))) ||
		FAILED(shellLink->SetIconLocation(path.c_str(), 0))) {
		return;
	}
	Microsoft::WRL::ComPtr<IPersistFile> persistFile;
	if (FAILED(shellLink.As(&persistFile))) {
		return;
	}
	wchar_t desktopPath[MAX_PATH + 1]{};
	if (FAILED(SHGetFolderPathW(nullptr, CSIDL_DESKTOP, nullptr, SHGFP_TYPE_CURRENT, desktopPath))) {
		return;
	}
	const wstring savePath = wstring(desktopPath) + L"\\OpenKey.lnk";
	const HRESULT saved = persistFile->Save(savePath.c_str(), TRUE);
	if (FAILED(saved)) {
		OutputDebugStringW(L"OpenKey: failed to save desktop shortcut.\n");
	}
}
