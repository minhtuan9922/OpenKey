#pragma once
#include <windows.h>
#include <string>

namespace OpenKeyUpdatePolicy {
    inline bool HasFluentBundle(const std::wstring& directory) {
        // The manifest also blocks engine-only updates if a bundle is incomplete.
        const wchar_t* markers[] = { L"\\package-manifest.json", L"\\OpenKey.UI.exe", L"\\OpenKey.UI" };
        for (const auto marker : markers) {
            if (GetFileAttributesW((directory + marker).c_str()) != INVALID_FILE_ATTRIBUTES) return true;
        }
        return false;
    }

    inline bool CanUseLegacyUpdater() {
        std::wstring module(32768, L'\0');
        const DWORD length = GetModuleFileNameW(nullptr, &module[0], static_cast<DWORD>(module.size()));
        if (length == 0 || length >= module.size()) return false;
        module.resize(length);
        const size_t separator = module.find_last_of(L"\\/");
        if (separator == std::wstring::npos) return false;
        return !HasFluentBundle(module.substr(0, separator));
    }

    inline void ShowManualUpdateNotice(HWND owner) {
        MessageBoxW(owner,
            L"Bản Fluent beta cần cập nhật đồng bộ engine, giao diện và runtime. "
            L"Updater stable chưa hỗ trợ gói này. Hãy xem hướng dẫn beta và cập nhật thủ công; "
            L"ứng dụng sẽ không tự thay tệp.", L"OpenKey Fluent beta", MB_OK | MB_ICONINFORMATION);
    }
}
