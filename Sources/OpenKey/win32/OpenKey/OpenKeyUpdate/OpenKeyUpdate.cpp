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

#include "framework.h"
#include "OpenKeyUpdate.h"
#include "../OpenKey/UpdatePolicy.h"
#include "../OpenKey/UpdateService.h"
#include <Urlmon.h>
#include <fstream>
#include <string>
#pragma comment(lib, "Urlmon.lib")

using namespace std;

INT_PTR CALLBACK MainDialogProcess(HWND, UINT, WPARAM, LPARAM);
void StartUpdate(HWND window);
HWND hDlg;
int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPWSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

	hDlg = CreateDialogParam(hInstance, MAKEINTRESOURCE(IDD_DIALOG_UPDATER), 0, MainDialogProcess, 0);
	ShowWindow(hDlg, SW_SHOWNORMAL);
 
	MSG msg;
	// Main message loop:
	while (GetMessage(&msg, nullptr, 0, 0)) {
		if (!IsDialogMessage(hDlg, &msg)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}
	return 0;
}

// Message handler for about box.
INT_PTR CALLBACK MainDialogProcess(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
	switch (message) {
	case WM_INITDIALOG:{
		HICON hIcon = LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_OPENKEYUPDATE));
		if (hIcon) {
			SendMessage(hDlg, WM_SETICON, ICON_BIG, (LPARAM)hIcon);
		}
		StartUpdate(hDlg);
		return (INT_PTR)TRUE;
	}
    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
        {
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        }
        break;
    }
    return (INT_PTR)FALSE;
}

// Run extraction in the download directory and wait for its actual result.
// Never remove the installed executable before a replacement is ready.
bool ExtractUpdate(const WCHAR* currentDir, const wstring& stagingName) {
	WCHAR systemDir[MAX_PATH];
	const UINT length = GetSystemDirectoryW(systemDir, ARRAYSIZE(systemDir));
	if (length == 0 || length >= ARRAYSIZE(systemDir)) return false;
	const wstring executable = wstring(systemDir) + L"\\WindowsPowerShell\\v1.0\\powershell.exe";
	// A caller using PowerShell 7 can supply an incompatible PSModulePath.
	// Load the archive module bundled with the explicitly selected Windows PowerShell.
	wstring command = L"\"" + executable + L"\" -NoProfile -NonInteractive -Command \"$ErrorActionPreference='Stop'; Import-Module (Join-Path $PSHOME 'Modules\\Microsoft.PowerShell.Archive\\Microsoft.PowerShell.Archive.psd1'); Expand-Archive -LiteralPath '.\\_OpenKeyUpdate.zip' -DestinationPath '.\\" + stagingName + L"'\"";
	STARTUPINFOW startup = {};
	startup.cb = sizeof(startup);
	PROCESS_INFORMATION process = {};
	if (!CreateProcessW(executable.c_str(), &command[0], nullptr, nullptr, FALSE,
		CREATE_NO_WINDOW, nullptr, currentDir, &startup, &process)) return false;
	CloseHandle(process.hThread);
	const DWORD waitResult = WaitForSingleObject(process.hProcess, INFINITE);
	DWORD exitCode = 1;
	const bool succeeded = waitResult == WAIT_OBJECT_0 &&
		GetExitCodeProcess(process.hProcess, &exitCode) && exitCode == 0;
	CloseHandle(process.hProcess);
	return succeeded;
}

namespace {
#ifdef _WIN64
	constexpr wchar_t UpdateExecutableName[] = L"OpenKey64.exe";
	constexpr WORD UpdateExecutableMachine = IMAGE_FILE_MACHINE_AMD64;
#else
	constexpr wchar_t UpdateExecutableName[] = L"OpenKey32.exe";
	constexpr WORD UpdateExecutableMachine = IMAGE_FILE_MACHINE_I386;
#endif
}

// Header validation is an architecture sanity check, not signature verification.
bool IsMatchingUpdateExecutable(const wstring& path) {
	ifstream file(path, ios::binary);
	IMAGE_DOS_HEADER dos = {};
	if (!file.read(reinterpret_cast<char*>(&dos), sizeof(dos)) ||
		dos.e_magic != IMAGE_DOS_SIGNATURE || dos.e_lfanew < static_cast<LONG>(sizeof(dos)) || dos.e_lfanew > 1024 * 1024) return false;
	file.seekg(dos.e_lfanew, ios::beg);
	DWORD signature = 0;
	WORD machine = 0;
	return file.read(reinterpret_cast<char*>(&signature), sizeof(signature)) &&
		file.read(reinterpret_cast<char*>(&machine), sizeof(machine)) &&
		signature == IMAGE_NT_SIGNATURE && machine == UpdateExecutableMachine;
}

bool InstallDownloadedUpdate(const WCHAR* currentDir, const wstring& stagingName) {
	const wstring stagingPath = wstring(currentDir) + L"\\" + stagingName;
	const wstring stagedExecutable = stagingPath + L"\\" + UpdateExecutableName;
	const wstring installedExecutable = wstring(currentDir) + L"\\" + UpdateExecutableName;
	return GetFileAttributesW(stagingPath.c_str()) == INVALID_FILE_ATTRIBUTES &&
		ExtractUpdate(currentDir, stagingName) &&
		IsMatchingUpdateExecutable(stagedExecutable) &&
		MoveFileExW(stagedExecutable.c_str(), installedExecutable.c_str(),
			MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH);
}

// Compatibility adapter for regression fixtures; production uses UpdateService.
bool ReadUpdateVersion(const wstring& data, wstring& versionName) {
    versionName.clear();
    string tag;
    const auto result = UpdateCheckService::Check(data, 0, tag);
    if (result != UpdateCheckService::Result::Available &&
        result != UpdateCheckService::Result::UpToDate) return false;
    versionName.assign(tag.begin(), tag.end());
    return true;
}

DWORD WINAPI UpdateThreadFunction(LPVOID lpParam) {
	const HWND hDlg = reinterpret_cast<HWND>(lpParam);
	if (!OpenKeyUpdatePolicy::CanUseLegacyUpdater()) {
		OpenKeyUpdatePolicy::ShowManualUpdateNotice(hDlg);
		ExitProcess(1);
		return 1;
	}
	const DWORD directoryLength = GetCurrentDirectoryW(0, nullptr);
	wstring currentDir(directoryLength, L'\0');
	const DWORD copiedLength = directoryLength ? GetCurrentDirectoryW(directoryLength, &currentDir[0]) : 0;
	if (copiedLength == 0 || copiedLength >= directoryLength) {
		MessageBox(hDlg, L"Không đọc được thư mục cập nhật.", L"OpenKey Update", MB_OK | MB_ICONERROR);
		ExitProcess(1);
	}
	currentDir.resize(copiedLength);
	if (OpenKeyUpdatePolicy::HasFluentBundle(currentDir)) {
		OpenKeyUpdatePolicy::ShowManualUpdateNotice(hDlg);
		ExitProcess(1);
		return 1;
	}
    UpdateService::Outcome metadata;
    try { metadata = UpdateService::Check(0); }
    catch (...) { /* Fail closed; never continue to archive installation. */ }
    if (metadata.result != UpdateCheckService::Result::Available &&
        metadata.result != UpdateCheckService::Result::UpToDate) {
        const wchar_t* message = metadata.result == UpdateCheckService::Result::InvalidMetadata
            ? L"Thông tin phiên bản cập nhật không hợp lệ."
            : L"Có lỗi trong quá trình cập nhật, vui lòng thử lại sau!";
        MessageBox(hDlg, message, L"OpenKey Update", MB_OK | MB_ICONERROR);
        ExitProcess(1);
        return 1;
    }
    const wstring versionName(metadata.version.begin(), metadata.version.end());
	
	//download zip file
	const wstring updateUrl = wstring(OpenKeyUpdateSource::ReleaseBase) +
		versionName + L"/OpenKey" + versionName + L"-Windows.zip";
	const wstring path = currentDir + L"\\_OpenKeyUpdate.zip";
	const HRESULT res = URLDownloadToFileW(NULL, updateUrl.c_str(), path.c_str(), 0, NULL);

	if (res == S_OK) {
		// A new directory prevents a leftover executable from an earlier failed update
		// from being mistaken for the executable in the freshly downloaded archive.
		const wstring stagingName = L"_OpenKeyUpdate-" + to_wstring(GetCurrentProcessId()) +
			L"-" + to_wstring(GetTickCount64());
		const wstring stagingPath = wstring(currentDir) + L"\\" + stagingName;
		if (!InstallDownloadedUpdate(currentDir.c_str(), stagingName)) {
			MessageBox(hDlg, L"Không thể giải nén hoặc thay tệp ứng dụng. Hãy thoát OpenKey và thử lại.",
				L"OpenKey Update", MB_OK | MB_ICONERROR);
			ExitProcess(1);
			return 1;
		}
		DeleteFileW(path.c_str());
		DeleteFileW((stagingPath + L"\\OpenKeyUpdate.exe").c_str());
		DeleteFileW((stagingPath + L"\\OpenKey32.exe").c_str());
		DeleteFileW((stagingPath + L"\\OpenKey64.exe").c_str());
		RemoveDirectoryW(stagingPath.c_str());
		MessageBox(hDlg, _T("Bạn đã cập nhật OpenKey bản mới nhất thành công!"), _T("OpenKey Update"), MB_OK);
		ExitProcess(0);
	} else {
		MessageBox(hDlg, _T("Có lỗi trong quá trình cập nhật, vui lòng thử lại sau!"), _T("OpenKey Update"), MB_OK);
		ExitProcess(0);
	}
	return 0;
}

void StartUpdate(HWND window) {
	DWORD hThread;
	HANDLE t = CreateThread(
							NULL,                   // default security attributes
							0,                      // use default stack size  
							UpdateThreadFunction,       // thread function name
							window,     // dialog is available before CreateDialogParam returns
							0,                      // use default creation flags 
							&hThread);   // returns the thread identifier 
	if (t) {
		CloseHandle(t);
	} else {
		MessageBox(window, L"Không thể khởi động cập nhật.", L"OpenKey Update", MB_OK | MB_ICONERROR);
		ExitProcess(1);
	}
}
