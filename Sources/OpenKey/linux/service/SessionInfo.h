#pragma once
#include <cstdlib>
#include <string_view>

namespace openkey {
enum class DesktopSession { Wayland, X11, Unknown };
enum class HotkeyStatus { Disabled, Unsupported, NotBuilt, Active, Conflict, Unavailable, Error };
inline const char* hotkeyStatusName(HotkeyStatus status) {
    switch (status) {
    case HotkeyStatus::Unsupported: return "unsupported-session";
    case HotkeyStatus::NotBuilt: return "not-built";
    case HotkeyStatus::Active: return "active";
    case HotkeyStatus::Conflict: return "conflict";
    case HotkeyStatus::Unavailable: return "unavailable";
    case HotkeyStatus::Error: return "error";
    default: return "disabled";
    }
}
inline DesktopSession desktopSession(std::string_view type) {
    if (type == "wayland") return DesktopSession::Wayland;
    if (type == "x11") return DesktopSession::X11;
    return DesktopSession::Unknown;
}
inline DesktopSession currentDesktopSession() {
    const char* type = std::getenv("XDG_SESSION_TYPE");
    // DISPLAY alone can be XWayland, not evidence of an X11 desktop session.
    return desktopSession(type ? type : "");
}
inline const char* hotkeyHint(DesktopSession session, HotkeyStatus status = HotkeyStatus::Disabled) {
    switch (session) {
    case DesktopSession::Wayland: return "Wayland: hotkey riêng chưa hỗ trợ";
    case DesktopSession::X11:
        switch (status) {
        case HotkeyStatus::Active: return "X11: Ctrl+Alt+Shift+O mở cấu hình";
        case HotkeyStatus::NotBuilt: return "X11: bản build chưa có hotkey riêng";
        case HotkeyStatus::Conflict: return "X11: tổ hợp phím đã bị chiếm";
        case HotkeyStatus::Unavailable: return "X11: chưa kết nối được màn hình";
        case HotkeyStatus::Error: return "X11: lỗi đăng ký phím tắt; cần khởi động lại tray";
        default: return "X11: hotkey riêng đang tắt";
        }
    default: return "Chưa rõ phiên: hotkey riêng chưa hỗ trợ";
    }
}
}
