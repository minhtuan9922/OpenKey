#pragma once
#include "SessionInfo.h"
#include <memory>

namespace openkey {
// Fixed Ctrl+Alt+Shift+O, opt-in, X11 sessions only. No Fcitx calls on X events.
class X11Hotkey {
public:
    X11Hotkey(bool enabled, DesktopSession session);
    ~X11Hotkey();
    X11Hotkey(const X11Hotkey&) = delete;
    X11Hotkey& operator=(const X11Hotkey&) = delete;
    HotkeyStatus status() const;
    bool consume();
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
