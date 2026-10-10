#pragma once
#include <windows.h>
#include <algorithm>
#include <cstdint>

namespace OpenKeyUIWindowSize
{
    struct Size { DWORD width = 1120; DWORD height = 800; };
    struct Record { DWORD version; DWORD width; DWORD height; };
    struct Position { LONG x; LONG y; };
    struct PositionRecord { DWORD version; LONG x; LONG y; };
    constexpr wchar_t RegistryPath[] = L"SOFTWARE\\TuyenMai\\OpenKey";
    constexpr wchar_t ValueName[] = L"uiWindowSize";
    constexpr wchar_t PositionValueName[] = L"uiWindowPosition";
    constexpr wchar_t MaximizedValueName[] = L"uiWindowMaximized";

    inline bool ReadMaximized()
    {
        HKEY key = nullptr;
        if (RegOpenKeyExW(HKEY_CURRENT_USER, RegistryPath, 0, KEY_READ, &key) != ERROR_SUCCESS) return false;
        DWORD value = 0, type = 0, bytes = sizeof(value);
        const auto result = RegQueryValueExW(key, MaximizedValueName, nullptr, &type,
            reinterpret_cast<LPBYTE>(&value), &bytes);
        RegCloseKey(key);
        return result == ERROR_SUCCESS && type == REG_DWORD && bytes == sizeof(value) && value == 1;
    }

    inline bool SaveMaximized(bool maximized)
    {
        HKEY key = nullptr;
        DWORD disposition = 0;
        if (RegCreateKeyExW(HKEY_CURRENT_USER, RegistryPath, 0, nullptr, REG_OPTION_NON_VOLATILE,
            KEY_SET_VALUE, nullptr, &key, &disposition) != ERROR_SUCCESS) return false;
        const DWORD value = maximized ? 1 : 0;
        const auto result = RegSetValueExW(key, MaximizedValueName, 0, REG_DWORD,
            reinterpret_cast<const BYTE*>(&value), sizeof(value));
        RegCloseKey(key);
        return result == ERROR_SUCCESS;
    }

    inline bool ValidPosition(Position position)
    {
        return position.x >= -1000000 && position.x <= 1000000 &&
            position.y >= -1000000 && position.y <= 1000000;
    }

    inline bool ReadPosition(Position& position)
    {
        HKEY key = nullptr;
        if (RegOpenKeyExW(HKEY_CURRENT_USER, RegistryPath, 0, KEY_READ, &key) != ERROR_SUCCESS) return false;
        PositionRecord record{};
        DWORD type = 0, bytes = sizeof(record);
        const auto result = RegQueryValueExW(key, PositionValueName, nullptr, &type,
            reinterpret_cast<LPBYTE>(&record), &bytes);
        RegCloseKey(key);
        const Position candidate{record.x, record.y};
        if (result != ERROR_SUCCESS || type != REG_BINARY || bytes != sizeof(record) ||
            record.version != 1 || !ValidPosition(candidate)) return false;
        position = candidate;
        return true;
    }

    inline bool SavePosition(Position position)
    {
        if (!ValidPosition(position)) return false;
        HKEY key = nullptr;
        DWORD disposition = 0;
        if (RegCreateKeyExW(HKEY_CURRENT_USER, RegistryPath, 0, nullptr, REG_OPTION_NON_VOLATILE,
            KEY_SET_VALUE, nullptr, &key, &disposition) != ERROR_SUCCESS) return false;
        const PositionRecord record{1, position.x, position.y};
        const auto result = RegSetValueExW(key, PositionValueName, 0, REG_BINARY,
            reinterpret_cast<const BYTE*>(&record), sizeof(record));
        RegCloseKey(key);
        return result == ERROR_SUCCESS;
    }

    inline int ClampPosition(int desired, int origin, int extent, int windowExtent)
    {
        const auto upper = (std::min)(static_cast<std::int64_t>(INT32_MAX),
            static_cast<std::int64_t>(origin) + (std::max)(std::int64_t{0},
                static_cast<std::int64_t>(extent) - (std::max)(1, windowExtent)));
        return static_cast<int>((std::max)(static_cast<std::int64_t>(origin),
            (std::min)(static_cast<std::int64_t>(desired), upper)));
    }

    inline bool Valid(Size size)
    {
        return size.width >= 640 && size.width <= 8192 && size.height >= 400 && size.height <= 8192;
    }

    inline UINT NormalizeDpi(UINT dpi) { return dpi >= 48 && dpi <= 768 ? dpi : 96; }
    inline double LogicalMetric(int pixels, UINT dpi)
    {
        return static_cast<double>((std::max)(0, pixels)) * 96 / NormalizeDpi(dpi);
    }

    inline Size Read()
    {
        HKEY key = nullptr;
        if (RegOpenKeyExW(HKEY_CURRENT_USER, RegistryPath, 0, KEY_READ, &key) != ERROR_SUCCESS) return {};
        Record record{};
        DWORD type = 0, bytes = sizeof(record);
        const auto result = RegQueryValueExW(key, ValueName, nullptr, &type,
            reinterpret_cast<LPBYTE>(&record), &bytes);
        RegCloseKey(key);
        const Size size{record.width, record.height};
        return result == ERROR_SUCCESS && type == REG_BINARY && bytes == sizeof(record) &&
            record.version == 1 && Valid(size) ? size : Size{};
    }

    inline bool Save(Size size)
    {
        if (!Valid(size)) return false;
        HKEY key = nullptr;
        DWORD disposition = 0;
        if (RegCreateKeyExW(HKEY_CURRENT_USER, RegistryPath, 0, nullptr, REG_OPTION_NON_VOLATILE,
            KEY_SET_VALUE, nullptr, &key, &disposition) != ERROR_SUCCESS) return false;
        const Record record{1, size.width, size.height};
        const auto result = RegSetValueExW(key, ValueName, 0, REG_BINARY,
            reinterpret_cast<const BYTE*>(&record), sizeof(record));
        RegCloseKey(key);
        return result == ERROR_SUCCESS;
    }

    inline int Pixels(DWORD logical, UINT dpi)
    {
        return static_cast<int>((static_cast<std::uint64_t>(logical) * NormalizeDpi(dpi) + 48) / 96);
    }

    inline Size LogicalSize(int width, int height, UINT dpi)
    {
        if (width <= 0 || height <= 0) return {0, 0};
        const auto normalized = NormalizeDpi(dpi);
        return {static_cast<DWORD>((static_cast<std::uint64_t>(width) * 96 + normalized / 2) / normalized),
            static_cast<DWORD>((static_cast<std::uint64_t>(height) * 96 + normalized / 2) / normalized)};
    }

    inline int Fit(DWORD logical, int available, UINT dpi)
    {
        const int margin = Pixels(24, dpi);
        const int bounded = static_cast<int>((std::max)(std::int64_t{1},
            static_cast<std::int64_t>(available) - 2 * margin));
        return (std::min)(Pixels(logical, dpi), bounded);
    }

    struct NormalGeometry { Size size; Position position{}; bool valid = false; };

    inline bool CaptureNormalGeometry(NormalGeometry& geometry, Size size, Position position,
        bool minimized, bool maximized)
    {
        if (minimized || maximized || !Valid(size) || !ValidPosition(position)) return false;
        geometry = {size, position, true};
        return true;
    }
}
