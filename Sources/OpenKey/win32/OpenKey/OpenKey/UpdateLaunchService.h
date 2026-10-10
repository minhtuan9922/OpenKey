#pragma once

#include <windows.h>
#include <shellapi.h>
#include <string>
#include "UpdatePolicy.h"

#pragma comment(lib, "Shell32.lib")

namespace UpdateLaunchService {
enum class Result { Started, ManualUpdateRequired, Failed };

// Injectable launch boundary: tests never start an updater or display a dialog.
template<typename Allowed, typename ModulePath, typename Execute>
Result Start(HWND owner, Allowed allowed, ModulePath modulePath, Execute execute) {
    if (!allowed()) return Result::ManualUpdateRequired;
    constexpr DWORD capacity = 32768;
    std::wstring path(capacity, L'\0');
    const DWORD length = modulePath(&path[0], capacity);
    if (!length || length >= capacity) return Result::Failed;
    path.resize(length);
    if (path.find(L'\0') != std::wstring::npos) return Result::Failed;
    const bool absolute = (path.size() >= 3 && path[1] == L':' &&
        (path[2] == L'\\' || path[2] == L'/')) ||
        (path.size() >= 3 && path[0] == L'\\' && path[1] == L'\\');
    const auto separator = path.find_last_of(L"\\/");
    if (!absolute || separator == std::wstring::npos || separator + 1 == path.size()) return Result::Failed;
    const std::wstring directory = path.substr(0, separator + 1);
    const std::wstring updater = directory + L"OpenKeyUpdate.exe";
    return execute(owner, updater.c_str(), directory.c_str()) > 32 ? Result::Started : Result::Failed;
}

inline Result Start(HWND owner) {
    return Start(owner, [] { return OpenKeyUpdatePolicy::CanUseLegacyUpdater(); },
        [](LPWSTR buffer, DWORD size) { return GetModuleFileNameW(nullptr, buffer, size); },
        [](HWND parent, LPCWSTR file, LPCWSTR directory) {
            return reinterpret_cast<INT_PTR>(ShellExecuteW(parent, L"open", file, nullptr, directory, SW_SHOWNORMAL));
        });
}

// Controllers own application lifetime: failure must never stop the engine.
inline bool StartOrNotify(HWND owner) {
    const auto result = Start(owner);
    if (result == Result::ManualUpdateRequired) OpenKeyUpdatePolicy::ShowManualUpdateNotice(owner);
    else if (result == Result::Failed) MessageBoxW(owner,
        L"Kh\u00f4ng th\u1ec3 m\u1edf OpenKeyUpdate.exe. H\u00e3y ki\u1ec3m tra b\u1ed9 c\u00e0i v\u00e0 th\u1eed l\u1ea1i.",
        L"OpenKey Update", MB_OK | MB_ICONERROR);
    return result == Result::Started;
}
}
