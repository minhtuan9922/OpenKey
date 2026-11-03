#include "X11Hotkey.h"
#include <stdexcept>
#include <string>
int main() {
    using namespace openkey;
    X11Hotkey disabled(false, DesktopSession::X11);
    X11Hotkey wayland(true, DesktopSession::Wayland);
    X11Hotkey unknown(true, DesktopSession::Unknown);
#ifdef OPENKEY_TEST_NO_X11_HOTKEY
    X11Hotkey notBuilt(true, DesktopSession::X11);
    if (notBuilt.status() != HotkeyStatus::NotBuilt || notBuilt.consume())
        throw std::runtime_error("Disabled build must never connect to X11");
#endif
    if (disabled.status() != HotkeyStatus::Disabled || wayland.status() != HotkeyStatus::Unsupported ||
        unknown.status() != HotkeyStatus::Unsupported || disabled.consume() || wayland.consume() || unknown.consume())
        throw std::runtime_error("Hotkeys must be opt-in and reject Wayland/unknown without X I/O");
    if (std::string(hotkeyHint(DesktopSession::X11, HotkeyStatus::Active)).find("Ctrl+Alt+Shift+O") == std::string::npos ||
        std::string(hotkeyHint(DesktopSession::X11, HotkeyStatus::Conflict)).find("chiếm") == std::string::npos)
        throw std::runtime_error("Hotkey state must be visible in the tray");
}
