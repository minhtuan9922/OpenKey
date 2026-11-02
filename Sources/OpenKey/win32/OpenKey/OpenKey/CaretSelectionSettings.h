#pragma once
#include <windows.h>
#pragma comment(lib, "advapi32.lib")

namespace OpenKeyCaretSettings {
    enum class Choice : int { Unknown = -1, Inactive = 0, Selected = 1 };
    constexpr wchar_t RegistryPath[] = L"SOFTWARE\\TuyenMai\\OpenKey";
    constexpr wchar_t ValueName[] = L"caretProfileSelected";
    inline Choice Read() {
        HKEY key = nullptr;
        if (RegOpenKeyExW(HKEY_CURRENT_USER, RegistryPath, 0, KEY_QUERY_VALUE, &key) != ERROR_SUCCESS) return Choice::Unknown;
        DWORD value = 0, type = 0, size = sizeof(value);
        const auto result = RegQueryValueExW(key, ValueName, nullptr, &type, reinterpret_cast<BYTE*>(&value), &size);
        RegCloseKey(key);
        return result == ERROR_SUCCESS && type == REG_DWORD && size == sizeof(value) && value <= 1
            ? static_cast<Choice>(value) : Choice::Unknown;
    }
    inline bool Save(bool selected) {
        HKEY key = nullptr; DWORD disposition = 0;
        if (RegCreateKeyExW(HKEY_CURRENT_USER, RegistryPath, 0, nullptr, REG_OPTION_NON_VOLATILE,
            KEY_SET_VALUE, nullptr, &key, &disposition) != ERROR_SUCCESS) return false;
        const DWORD value = selected ? 1 : 0;
        const auto result = RegSetValueExW(key, ValueName, 0, REG_DWORD, reinterpret_cast<const BYTE*>(&value), sizeof(value));
        RegCloseKey(key); return result == ERROR_SUCCESS;
    }
    struct Store { Choice Read() const { return OpenKeyCaretSettings::Read(); } bool Save(bool selected) const { return OpenKeyCaretSettings::Save(selected); } };
    class RememberedChoice {
        Choice saved = Choice::Unknown;
    public:
        template<class Storage> bool Load(Storage& store) { saved = store.Read(); return saved == Choice::Selected; }
        template<class Storage> bool Observe(bool selected, Storage& store) {
            const auto desired = selected ? Choice::Selected : Choice::Inactive;
            // The UI can also save this value from another process.
            if (saved == desired && store.Read() == desired) return true;
            if (!store.Save(selected)) return false;
            saved = desired; return true;
        }
    };
}
