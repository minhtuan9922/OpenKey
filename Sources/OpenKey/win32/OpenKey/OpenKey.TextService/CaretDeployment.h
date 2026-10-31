#pragma once
#include <windows.h>
#include <string>
#include <vector>

namespace OpenKeyCaretDeployment {
    constexpr wchar_t ServerKey[] = L"SOFTWARE\\Classes\\CLSID\\{727F9E3B-0491-4D5C-B33D-64D59BE28A70}\\InprocServer32";
    inline bool Is64BitWindows() {
        SYSTEM_INFO info{}; GetNativeSystemInfo(&info);
        return info.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_AMD64 || info.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_ARM64;
    }
    inline std::wstring RegisteredPath(bool x64) {
        DWORD bytes = 0;
        const DWORD flags = RRF_RT_REG_SZ | (x64 ? RRF_SUBKEY_WOW6464KEY : RRF_SUBKEY_WOW6432KEY);
        if (RegGetValueW(HKEY_LOCAL_MACHINE, ServerKey, nullptr, flags, nullptr, nullptr, &bytes) != ERROR_SUCCESS ||
            bytes < sizeof(wchar_t) || bytes > 65536) return {};
        std::vector<wchar_t> value(bytes / sizeof(wchar_t) + 1);
        if (RegGetValueW(HKEY_LOCAL_MACHINE, ServerKey, nullptr, flags, nullptr, value.data(), &bytes) != ERROR_SUCCESS) return {};
        return value.data();
    }
    inline bool SamePath(const std::wstring& left, const std::wstring& right) {
        return !left.empty() && CompareStringOrdinal(left.c_str(), -1, right.c_str(), -1, TRUE) == CSTR_EQUAL;
    }
}
