// Run only through RunX11HotkeyTests.sh, on a private Xvfb server, never desktop X.
#include "X11Hotkey.h"
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <X11/extensions/XTest.h>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <thread>
namespace {
using namespace std::chrono_literals;
void require(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
template<class Predicate> void await(Predicate ready) {
    const auto deadline = std::chrono::steady_clock::now() + 3s;
    while (!ready()) {
        require(std::chrono::steady_clock::now() < deadline, "X11 hotkey timeout");
        std::this_thread::sleep_for(5ms);
    }
}
}
int main() {
    using namespace openkey;
    require(std::getenv("OPENKEY_ISOLATED_X11") != nullptr, "Private Xvfb fixture required");
    Display* display = XOpenDisplay(nullptr);
    require(display != nullptr, "Cannot open private Xvfb");
    struct Close { Display* value; ~Close() { XCloseDisplay(value); } } close{display};
    const auto key = [&](KeySym symbol, bool down) {
        const auto code = XKeysymToKeycode(display, symbol);
        require(code != 0, "Key missing in test keymap");
        XTestFakeKeyEvent(display, code, down, 0);
        XFlush(display);
    };
    const auto modifiers = [&](bool down) {
        key(XK_Control_L, down); key(XK_Alt_L, down); key(XK_Shift_L, down);
    };
    // XTEST creates virtual keyboard devices lazily, which changes the keymap.
    // Complete that setup before installing the grab under test.
    key(XK_Shift_L, true); key(XK_Shift_L, false);
    XSync(display, False);
    {
        X11Hotkey hook(true, DesktopSession::X11);
        await([&] { return hook.status() == HotkeyStatus::Active; });
        {
            X11Hotkey conflict(true, DesktopSession::X11);
            await([&] { return conflict.status() == HotkeyStatus::Conflict; });
            require(hook.status() == HotkeyStatus::Active, "Conflicting client must not remove original grabs");
        }
        key(XK_o, true); key(XK_o, false);
        std::this_thread::sleep_for(60ms);
        require(!hook.consume(), "Plain typing must not trigger the shortcut");
        modifiers(true); key(XK_o, true);
        await([&] { return hook.consume(); });
        // Hold across the X server's repeat interval: only one activation.
        std::this_thread::sleep_for(1200ms);
        require(!hook.consume(), "Auto-repeat must not open repeated settings windows");
        key(XK_o, false); modifiers(false);
        std::this_thread::sleep_for(30ms);
        for (const auto lock : {XK_Caps_Lock, XK_Num_Lock}) {
            key(lock, true); key(lock, false);
            std::this_thread::sleep_for(30ms);
            modifiers(true); key(XK_o, true);
            await([&] { return hook.consume(); });
            key(XK_o, false); modifiers(false);
            std::this_thread::sleep_for(30ms);
        }
        require(hook.status() == HotkeyStatus::Active, "Lock key changes must preserve the registration");
        int count = 0;
        const auto code = XKeysymToKeycode(display, XK_o);
        auto* map = XGetKeyboardMapping(display, code, 1, &count);
        require(map != nullptr, "Cannot read test keymap");
        XChangeKeyboardMapping(display, code, count, map, 1);
        XFree(map);
        XSync(display, False);
        await([&] { return hook.status() == HotkeyStatus::Error; });
        require(!hook.consume(), "Keymap changes must release grabs, not trigger a command");
    }
    X11Hotkey reacquire(true, DesktopSession::X11);
    await([&] { return reacquire.status() == HotkeyStatus::Active; });
    std::cout << "Private Xvfb: passive grab, conflict, plain typing, repeat, lock variants and cleanup passed\n";
}
