#pragma once
#include <windows.h>

namespace OpenKeyKeyboardCase
{
    constexpr bool IsCaseToggleKey(const UINT key) noexcept
    {
        return key == VK_CAPITAL;
    }

    // GetKeyState's low bit is the toggle state; the high bit is key-down.
    constexpr bool CapsLockEnabled(const SHORT state) noexcept
    {
        return (static_cast<unsigned short>(state) & 1u) != 0;
    }
}
