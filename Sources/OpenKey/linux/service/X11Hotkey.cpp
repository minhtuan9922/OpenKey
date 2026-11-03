#include "X11Hotkey.h"
#include <atomic>
#include <thread>
#ifdef OPENKEY_HAVE_X11_HOTKEY
#include <xcb/xcb.h>
#include <algorithm>
#include <cerrno>
#include <cstdlib>
#include <poll.h>
#include <string_view>
#include <vector>
#endif

namespace openkey {
struct X11Hotkey::Impl {
    std::atomic<HotkeyStatus> state{HotkeyStatus::Disabled};
    std::atomic<bool> stopping{false}, triggered{false};
    std::thread worker;
    Impl(bool enabled, DesktopSession session) {
        if (!enabled) return;
        if (session != DesktopSession::X11) { state = HotkeyStatus::Unsupported; return; }
#ifdef OPENKEY_HAVE_X11_HOTKEY
        state = HotkeyStatus::Unavailable;
        worker = std::thread([this] { try { run(); } catch (...) { state = HotkeyStatus::Error; } });
#else
        state = HotkeyStatus::NotBuilt;
#endif
    }
    ~Impl() { stopping = true; if (worker.joinable()) worker.join(); }
#ifdef OPENKEY_HAVE_X11_HOTKEY
    void run() {
        // Do not connect to remote displays or infer X11 from XWayland's DISPLAY.
        const char* display = std::getenv("DISPLAY");
        if (!display || std::string_view(display).substr(0, 1) != ":") return;
        int screenNumber = 0;
        auto* connection = xcb_connect(display, &screenNumber);
        struct Connection { xcb_connection_t* value; ~Connection() { xcb_disconnect(value); } } cleanup{connection};
        if (xcb_connection_has_error(connection)) return;
        const auto* setup = xcb_get_setup(connection);
        const auto min = setup->min_keycode;
        auto* mapping = xcb_get_keyboard_mapping_reply(connection,
            xcb_get_keyboard_mapping(connection, min, setup->max_keycode - min + 1), nullptr);
        auto* modifiers = xcb_get_modifier_mapping_reply(connection, xcb_get_modifier_mapping(connection), nullptr);
        struct Replies {
            void *keys, *mods;
            ~Replies() { std::free(keys); std::free(mods); }
        } replies{mapping, modifiers};
        if (!mapping || !modifiers || !mapping->keysyms_per_keycode) { state = HotkeyStatus::Error; return; }
        const auto* symbols = xcb_get_keyboard_mapping_keysyms(mapping);
        const auto width = mapping->keysyms_per_keycode;
        unsigned key = 0;
        for (unsigned code = min; code <= setup->max_keycode && !key; ++code)
            for (unsigned column = 0; column < std::min<unsigned>(width, 2); ++column)
                if (symbols[(code - min) * width + column] == 'o' ||
                    symbols[(code - min) * width + column] == 'O') { key = code; break; }
        if (!key) { state = HotkeyStatus::Error; return; }
        uint16_t numLock = 0;
        const auto* codes = xcb_get_modifier_mapping_keycodes(modifiers);
        for (unsigned mod = 0; mod < 8; ++mod) for (unsigned i = 0; i < modifiers->keycodes_per_modifier; ++i) {
            const auto code = codes[mod * modifiers->keycodes_per_modifier + i];
            if (code < min || code > setup->max_keycode) continue;
            for (unsigned col = 0; col < width; ++col)
                if (symbols[(code - min) * width + col] == 0xff7f) numLock |= 1u << mod;
        }
        constexpr uint16_t base = XCB_MOD_MASK_CONTROL | XCB_MOD_MASK_1 | XCB_MOD_MASK_SHIFT;
        if (numLock & base) { state = HotkeyStatus::Error; return; }
        std::vector<uint16_t> variants{0, XCB_MOD_MASK_LOCK, numLock,
                                       static_cast<uint16_t>(XCB_MOD_MASK_LOCK | numLock)};
        std::sort(variants.begin(), variants.end());
        variants.erase(std::unique(variants.begin(), variants.end()), variants.end());
        for (auto roots = xcb_setup_roots_iterator(setup); roots.rem; xcb_screen_next(&roots))
            for (const auto locks : variants) {
                auto* error = xcb_request_check(connection, xcb_grab_key_checked(connection, 0,
                    roots.data->root, base | locks, key, XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC));
                if (error) {
                    state = error->error_code == XCB_ACCESS ? HotkeyStatus::Conflict : HotkeyStatus::Error;
                    std::free(error);
                    return; // Closing this connection releases every partial grab.
                }
            }
        state = HotkeyStatus::Active;
        bool held = false, releasedHeld = false;
        uint32_t releasedAt = 0;
        while (!stopping) {
            if (xcb_connection_has_error(connection)) { state = HotkeyStatus::Unavailable; return; }
            for (unsigned i = 0; i < 64; ++i) {
                auto* event = xcb_poll_for_event(connection);
                if (!event) break;
                const auto type = event->response_type & 0x7f;
                if (!(event->response_type & 0x80) && (type == XCB_KEY_PRESS || type == XCB_KEY_RELEASE)) {
                    const auto& e = *reinterpret_cast<xcb_key_press_event_t*>(event);
                    if (e.detail == key) {
                        if (type == XCB_KEY_RELEASE) { releasedHeld = held; held = false; releasedAt = e.time; }
                        else {
                            // Legacy X auto-repeat emits a release/press pair with the same timestamp.
                            if (!held && !(releasedHeld && releasedAt == e.time)) triggered = true;
                            held = true;
                            releasedHeld = false;
                        }
                    }
                }
                const bool invalidated = type == XCB_MAPPING_NOTIFY || type == 0;
                std::free(event);
                if (invalidated) { state = HotkeyStatus::Error; return; }
            }
            pollfd fd{xcb_get_file_descriptor(connection), POLLIN, 0};
            const int ready = poll(&fd, 1, 50);
            if ((ready < 0 && errno != EINTR) || (fd.revents & (POLLERR | POLLHUP | POLLNVAL))) {
                state = HotkeyStatus::Unavailable;
                return;
            }
        }
    }
#endif
};
X11Hotkey::X11Hotkey(bool enabled, DesktopSession session) : impl_(std::make_unique<Impl>(enabled, session)) {}
X11Hotkey::~X11Hotkey() = default;
HotkeyStatus X11Hotkey::status() const { return impl_->state.load(); }
bool X11Hotkey::consume() { return impl_->triggered.exchange(false); }
}
