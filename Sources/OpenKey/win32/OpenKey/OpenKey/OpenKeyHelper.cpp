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
#include "OpenKeyHelper.h"
#include "LegacyQuickConvert.h"
#include <stdarg.h>
#include <algorithm>
#include <memory>

#pragma comment(lib, "version.lib")

static BYTE* _regData = 0;

static LPCTSTR sk = TEXT("SOFTWARE\\TuyenMai\\OpenKey");
static HKEY hKey;
static LPCTSTR _runOnStartupKeyPath = _T("Software\\Microsoft\\Windows\\CurrentVersion\\Run");
static TCHAR _executePath[MAX_PATH];
static bool _hasGetPath = false;

static DWORD _cacheProcessId = 0, _tempProcessId = 0;
static HWND _tempWnd;
static TCHAR _exePath[1024] = { 0 };
static LPCTSTR _exeName = _exePath;
static HANDLE _proc;
static string _exeNameUtf8 = "TheOpenKeyProject";
static string _unknownProgram = "UnknownProgram";

namespace {
	struct ClipboardCloser {
		~ClipboardCloser() { CloseClipboard(); }
	};

	struct GlobalMemoryDeleter {
		void operator()(void* memory) const { GlobalFree(memory); }
	};
	using GlobalMemory = std::unique_ptr<void, GlobalMemoryDeleter>;

	GlobalMemory copyClipboardBytes(const void* source, const size_t size) {
		GlobalMemory memory(GlobalAlloc(GMEM_MOVEABLE, size));
		if (!memory) {
			return {};
		}
		void* destination = GlobalLock(memory.get());
		if (!destination) {
			return {};
		}
		memcpy(destination, source, size);
		GlobalUnlock(memory.get());
		return memory;
	}

	template<typename Character>
	std::basic_string<Character> readClipboardString(const UINT format) {
		const HANDLE memory = GetClipboardData(format);
		if (!memory) {
			return {};
		}
		const size_t count = GlobalSize(memory) / sizeof(Character);
		const Character* text = static_cast<const Character*>(GlobalLock(memory));
		if (!text) {
			return {};
		}
		struct Unlocker {
			HANDLE memory;
			~Unlocker() { GlobalUnlock(memory); }
		} unlocker{ memory };
		const Character* end = std::find(text, text + count, Character{});
		// Reject malformed text instead of reading beyond the clipboard allocation.
		return end == text + count ? std::basic_string<Character>{} : std::basic_string<Character>(text, end);
	}

		void removeRegistryStartupEntry() {
		HKEY runKey = nullptr;
		if (RegOpenKeyEx(HKEY_CURRENT_USER, _runOnStartupKeyPath, 0, KEY_SET_VALUE, &runKey) == ERROR_SUCCESS) {
			RegDeleteValue(runKey, _T("OpenKey"));
			RegCloseKey(runKey);
		}
	}

		bool launchScheduledTaskCommand(const std::wstring& arguments) {
			WCHAR systemDirectory[MAX_PATH]{};
			const UINT length = GetSystemDirectoryW(systemDirectory, ARRAYSIZE(systemDirectory));
			if (length == 0 || length >= ARRAYSIZE(systemDirectory)) {
				return false;
			}
			const std::wstring executable = std::wstring(systemDirectory, length) + L"\\schtasks.exe";
			std::wstring command = L"\"" + executable + L"\" " + arguments;
			STARTUPINFOW startup{};
			startup.cb = sizeof(startup);
			PROCESS_INFORMATION process{};
			if (!CreateProcessW(executable.c_str(), &command[0], nullptr, nullptr, FALSE,
				CREATE_NO_WINDOW, nullptr, nullptr, &startup, &process)) {
				return false;
			}
			CloseHandle(process.hThread);
			CloseHandle(process.hProcess);
			return true;
		}

		void removeScheduledStartupTask() {
			launchScheduledTaskCommand(L"/delete /tn OpenKey /f");
	}
}

int CF_RTF = RegisterClipboardFormat(_T("Rich Text Format"));
int CF_HTML = RegisterClipboardFormat(_T("HTML Format"));
int CF_OPENKEY = RegisterClipboardFormat(_T("OpenKey Format"));

void OpenKeyHelper::openKey() {
	LONG nError = RegOpenKeyEx(HKEY_CURRENT_USER, sk, NULL, KEY_ALL_ACCESS, &hKey);
	if (nError == ERROR_FILE_NOT_FOUND) 	{
		nError = RegCreateKeyEx(HKEY_CURRENT_USER, sk, NULL, NULL, REG_OPTION_NON_VOLATILE, KEY_CREATE_SUB_KEY, NULL, &hKey, NULL);
	}
	if (nError) {
		LOG(L"result %d\n", nError);
	}
}

void OpenKeyHelper::setRegInt(LPCTSTR key, const int & val) {
	openKey();
	RegSetValueEx(hKey, key, 0, REG_DWORD, (LPBYTE)&val, sizeof(val));
	RegCloseKey(hKey);
}

int OpenKeyHelper::getRegInt(LPCTSTR key, const int & defaultValue) {
	openKey();
	int val = defaultValue;
	DWORD size = sizeof(val);
	if (ERROR_SUCCESS != RegQueryValueEx(hKey, key, 0, 0, (LPBYTE)&val, &size)) {
		val = defaultValue;
	}
	RegCloseKey(hKey);
	return val;
}

void OpenKeyHelper::setRegBinary(LPCTSTR key, const BYTE * pData, const int & size) {
	openKey();
	RegSetValueEx(hKey, key, 0, REG_BINARY, pData, size);
	RegCloseKey(hKey);
}

BYTE * OpenKeyHelper::getRegBinary(LPCTSTR key, DWORD& outSize) {
	openKey();
	if (_regData) {
		delete[] _regData;
		_regData = NULL;
	}
	DWORD size = 0;
	RegQueryValueEx(hKey, key, 0, 0, 0, &size);
	_regData = new BYTE[size];
	if (ERROR_SUCCESS != RegQueryValueEx(hKey, key, 0, 0, _regData, &size)) {
		delete[] _regData;
		_regData = NULL;
	}
	outSize = size;
	RegCloseKey(hKey);
	return _regData;
}

void OpenKeyHelper::registerRunOnStartup(const int& val) {
	if (val) {
		if (vRunAsAdmin) {
			removeRegistryStartupEntry();
				const std::wstring path = getFullPath();
				// Quote the executable inside the /tr argument so paths with spaces survive task launch.
				launchScheduledTaskCommand(L"/create /sc onlogon /tn OpenKey /rl highest /tr \"\\\"" +
					path + L"\\\"\" /f");
		} else {
			removeScheduledStartupTask();
			HKEY runKey = nullptr;
			DWORD disposition = 0;
			if (RegCreateKeyEx(HKEY_CURRENT_USER, _runOnStartupKeyPath, 0, nullptr,
				REG_OPTION_NON_VOLATILE, KEY_SET_VALUE, nullptr, &runKey, &disposition) == ERROR_SUCCESS) {
				const wstring path = L"\"" + getFullPath() + L"\"";
				RegSetValueEx(runKey, _T("OpenKey"), 0, REG_SZ,
					reinterpret_cast<const BYTE*>(path.c_str()),
					(static_cast<DWORD>(path.size()) + 1) * sizeof(TCHAR));
				RegCloseKey(runKey);
			}
		}
	} else {
		removeRegistryStartupEntry();
		removeScheduledStartupTask();
	}
}

LPTSTR OpenKeyHelper::getExecutePath() {
	if (!_hasGetPath) {
		HMODULE hModule = GetModuleHandleW(NULL);
		GetModuleFileNameW(hModule, _executePath, MAX_PATH);
		_hasGetPath = true;
	}
	return _executePath;
}

string& OpenKeyHelper::getFrontMostAppExecuteName() {
	_tempWnd = GetForegroundWindow();
	GetWindowThreadProcessId(_tempWnd, &_tempProcessId);
	if (_tempProcessId == _cacheProcessId) {
		return _exeNameUtf8;
	}
	_cacheProcessId = _tempProcessId;
	_proc = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, _tempProcessId);
	GetProcessImageFileName((HMODULE)_proc, _exePath, 1024);
	CloseHandle(_proc);
	
	if (wcscmp(_exePath, _T("")) == 0) {
		return _unknownProgram;
	}
	_exeName = _tcsrchr(_exePath, '\\') + 1;
	if (wcscmp(_exeName, _T("OpenKey64.exe")) == 0 ||
		wcscmp(_exeName, _T("OpenKey32.exe")) == 0 || 
		wcscmp(_exeName, _T("explorer.exe")) == 0) {
		return _exeNameUtf8;
	}
	int size_needed = WideCharToMultiByte(CP_UTF8, 0, _exeName, (int)lstrlen(_exeName), NULL, 0, NULL, NULL);
	std::string strTo(size_needed, 0);
	WideCharToMultiByte(CP_UTF8, 0, _exeName, (int)lstrlen(_exeName), &strTo[0], size_needed, NULL, NULL);
	_exeNameUtf8 = strTo;
	//LOG(L"%s\n", utf8ToWideString(_exeNameUtf8).c_str());
	return _exeNameUtf8;
}

string & OpenKeyHelper::getLastAppExecuteName() {
	if (!vUseSmartSwitchKey)
		return getFrontMostAppExecuteName();
	return _exeNameUtf8;
}

wstring OpenKeyHelper::getFullPath() {
	HMODULE hModule = GetModuleHandle(NULL);
	TCHAR path[MAX_PATH];
	GetModuleFileName(hModule, path, MAX_PATH);
	wstring rs(path);
	return rs;
}

wstring OpenKeyHelper::getClipboardText(const int& type) {
	if (!OpenClipboard(nullptr)) {
		return {};
	}
	const ClipboardCloser closer;
	return readClipboardString<wchar_t>(type);
}

void OpenKeyHelper::setClipboardText(LPCTSTR data, const int& len, const int& type) {
	if (!data || len <= 0) {
		return;
	}
	auto memory = copyClipboardBytes(data, static_cast<size_t>(len) * sizeof(WCHAR));
	if (!memory) {
		return;
	}
	const HWND owner = FindWindow(APP_CLASS, nullptr);
	if (!owner || !OpenClipboard(owner)) {
		return;
	}
	const ClipboardCloser closer;
	if (EmptyClipboard() && SetClipboardData(type, memory.get())) {
		memory.release(); // Windows owns the allocation only after a successful write.
	}
}

bool OpenKeyHelper::quickConvert() {
	const HWND owner = FindWindow(APP_CLASS, nullptr);
	return LegacyQuickConvert::convertClipboard(owner) == ConvertService::ClipboardResult::Success;
}

DWORD OpenKeyHelper::getVersionNumber() {
	// get the filename of the executable containing the version resource
	TCHAR szFilename[MAX_PATH + 1] = { 0 };
	if (GetModuleFileName(NULL, szFilename, MAX_PATH) == 0) {
		return 0;
	}

	// allocate a block of memory for the version info
	DWORD dummy;
	UINT dwSize = GetFileVersionInfoSize(szFilename, &dummy);
	if (dwSize == 0) {
		return 0;
	}
	std::vector<BYTE> data(dwSize);

	// load the version info
	if (!GetFileVersionInfo(szFilename, NULL, dwSize, &data[0])) {
		return 0;
	}

	LPBYTE lpBuffer = NULL;

	if (VerQueryValue(&data[0], _T("\\"), (VOID FAR * FAR*) & lpBuffer, &dwSize)) {
		if (dwSize) {
			VS_FIXEDFILEINFO* verInfo = (VS_FIXEDFILEINFO*)lpBuffer;
			if (verInfo->dwSignature == 0xfeef04bd) {
				return ((verInfo->dwFileVersionMS >> 16) & 0xffff) |
					(((verInfo->dwFileVersionMS >> 0) & 0xffff) << 8) |
					(((verInfo->dwFileVersionLS >> 16) & 0xffff) << 16);
			}
		}
	}

	return 0;
}

wstring OpenKeyHelper::getVersionString() {
	TCHAR versionBuffer[MAX_PATH];
	DWORD ver = getVersionNumber();
	wsprintfW(versionBuffer, _T("%d.%d.%d"), ver & 0xFF, (ver>>8) & 0xFF, (ver >> 16) & 0xFF);
	return wstring(versionBuffer);

	// get the filename of the executable containing the version resource
	TCHAR szFilename[MAX_PATH + 1] = { 0 };
	if (GetModuleFileName(NULL, szFilename, MAX_PATH) == 0) { 
		return _T("");
	}
}
