#pragma once
#include <windows.h>

// Display language is independent of vLanguage (the engine input mode).
namespace OpenKeyUILanguage
{
    enum class Language : DWORD { Vietnamese = 0, English = 1 };
    constexpr wchar_t RegistryPath[] = L"SOFTWARE\\TuyenMai\\OpenKey";
    constexpr wchar_t ValueName[] = L"uiDisplayLanguage";

    inline Language Read()
    {
        HKEY key = nullptr;
        if (RegOpenKeyExW(HKEY_CURRENT_USER, RegistryPath, 0, KEY_READ, &key) != ERROR_SUCCESS)
            return Language::Vietnamese;
        DWORD value = 0, type = 0, size = sizeof(value);
        const auto result = RegQueryValueExW(key, ValueName, nullptr, &type,
            reinterpret_cast<LPBYTE>(&value), &size);
        RegCloseKey(key);
        return result == ERROR_SUCCESS && type == REG_DWORD && size == sizeof(value) && value == 1
            ? Language::English : Language::Vietnamese;
    }

    inline bool Save(Language language)
    {
        if (language != Language::Vietnamese && language != Language::English) return false;
        HKEY key = nullptr;
        DWORD disposition = 0;
        if (RegCreateKeyExW(HKEY_CURRENT_USER, RegistryPath, 0, nullptr, REG_OPTION_NON_VOLATILE,
            KEY_SET_VALUE, nullptr, &key, &disposition) != ERROR_SUCCESS) return false;
        const DWORD value = static_cast<DWORD>(language);
        const auto result = RegSetValueExW(key, ValueName, 0, REG_DWORD,
            reinterpret_cast<const BYTE*>(&value), sizeof(value));
        RegCloseKey(key);
        return result == ERROR_SUCCESS;
    }

    inline const wchar_t* Tag(Language language)
    {
        return language == Language::English ? L"en-US" : L"vi-VN";
    }
}
