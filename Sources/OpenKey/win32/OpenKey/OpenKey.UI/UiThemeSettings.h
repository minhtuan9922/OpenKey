#pragma once
#include <windows.h>

// A UI-only preference: never writes engine options or sends reload messages.
namespace OpenKeyUITheme
{
    enum class Theme : DWORD { System = 0, Light = 1, Dark = 2 };
    constexpr wchar_t RegistryPath[] = L"SOFTWARE\\TuyenMai\\OpenKey";
    constexpr wchar_t ValueName[] = L"uiTheme";

    inline bool IsColorLight(BYTE red, BYTE green, BYTE blue)
    {
        return 5 * green + 2 * red + blue > 8 * 128;
    }

    inline Theme Read()
    {
        HKEY key = nullptr;
        if (RegOpenKeyExW(HKEY_CURRENT_USER, RegistryPath, 0, KEY_READ, &key) != ERROR_SUCCESS)
            return Theme::System;
        DWORD value = 0, type = 0, size = sizeof(value);
        const auto result = RegQueryValueExW(key, ValueName, nullptr, &type,
            reinterpret_cast<LPBYTE>(&value), &size);
        RegCloseKey(key);
        return result == ERROR_SUCCESS && type == REG_DWORD && size == sizeof(value) && value <= 2
            ? static_cast<Theme>(value) : Theme::System;
    }

    inline bool Save(Theme theme)
    {
        if (static_cast<DWORD>(theme) > 2) return false;
        HKEY key = nullptr;
        DWORD disposition = 0;
        if (RegCreateKeyExW(HKEY_CURRENT_USER, RegistryPath, 0, nullptr, REG_OPTION_NON_VOLATILE,
            KEY_SET_VALUE, nullptr, &key, &disposition) != ERROR_SUCCESS) return false;
        const DWORD value = static_cast<DWORD>(theme);
        const auto result = RegSetValueExW(key, ValueName, 0, REG_DWORD,
            reinterpret_cast<const BYTE*>(&value), sizeof(value));
        RegCloseKey(key);
        return result == ERROR_SUCCESS;
    }
}
