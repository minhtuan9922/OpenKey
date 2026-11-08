#include "TrayItem.h"
#include <array>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

namespace openkey {
namespace {
constexpr const char* itemPath = "/StatusNotifierItem";
constexpr const char* itemInterface = "org.kde.StatusNotifierItem";
constexpr const char* watcher = "org.kde.StatusNotifierWatcher";
constexpr const char* menuInterface = "com.canonical.dbusmenu";
constexpr int quitMenuId = 2000; // POPUP_OPENKEY_EXIT in Tests/TrayContract.json.
constexpr int englishMenuId = 920; // Linux-only explicit selector, not Windows' toggle command 900.
constexpr int configurationMenuId = 1001; // Opens Fcitx settings, not Windows' OpenKey control panel.
constexpr int settingsMenuId = 1000; // OpenKey's own settings window.
bool settingsMenuIdForPage(int id) { return id == settingsMenuId || id == 990 || id == 980 || id == 1010; }
bool inputMenuId(int id) { return id == 910 || id == 911 || id == 912 || id == 913 || id == englishMenuId; }
bool validMenuId(int id) { return (id >= 0 && id <= 4) || inputMenuId(id) || id == configurationMenuId || settingsMenuIdForPage(id) || id == quitMenuId; }
int emptyPixmaps(sd_bus_message* reply) {
    int result = sd_bus_message_open_container(reply, 'a', "(iiay)");
    return result < 0 ? result : sd_bus_message_close_container(reply);
}
struct Stroke { double x1, y1, x2, y2; };
std::vector<uint8_t> renderIcon(InputStatus status, int size) {
    // Vector strokes in a 24-unit view box; rounded caps, no background tile.
    std::vector<Stroke> strokes;
    strokes.reserve(32);
    if (status == InputStatus::Telex) strokes = {{6, 5, 18, 5}, {12, 5, 12, 19}};
    else if (status == InputStatus::SimpleTelex1 || status == InputStatus::SimpleTelex2) {
        strokes = {{17, 5, 7, 5}, {7, 5, 7, 12}, {7, 12, 17, 12}, {17, 12, 17, 19}, {17, 19, 7, 19}};
        if (status == InputStatus::SimpleTelex1) strokes.push_back({21, 15, 21, 20});
        else { strokes.push_back({20, 15, 23, 15}); strokes.push_back({23, 15, 20, 20}); strokes.push_back({20, 20, 23, 20}); }
    }
    else if (status == InputStatus::Vni)
        strokes = {{6, 19, 6, 5}, {6, 5, 18, 19}, {18, 19, 18, 5}};
    else if (status == InputStatus::English)
        strokes = {{7, 5, 7, 19}, {7, 5, 17, 5}, {7, 12, 15, 12}, {7, 19, 17, 19}};
    else if (status == InputStatus::Inactive) strokes = {{7, 12, 17, 12}};
    else if (status == InputStatus::Other) {
        constexpr double pi = 3.14159265358979323846;
        for (int i = 0; i < 32; ++i) {
            const double a = i * pi / 16, b = (i + 1) * pi / 16;
            strokes.push_back({12 + 6 * std::cos(a), 12 + 7 * std::sin(a),
                               12 + 6 * std::cos(b), 12 + 7 * std::sin(b)});
        }
    } else strokes = {{7, 7, 9, 5}, {9, 5, 15, 5}, {15, 5, 17, 7},
                      {17, 7, 17, 10}, {17, 10, 12, 13}, {12, 13, 12, 14},
                      {12, 18.5, 12, 18.5}};
    std::vector<uint8_t> pixels(size * size * 4, 0);
    constexpr int samples = 4;
    for (int y = 0; y < size; ++y) for (int x = 0; x < size; ++x) {
        double alpha = 0, color = 0;
        for (int sy = 0; sy < samples; ++sy) for (int sx = 0; sx < samples; ++sx) {
            const double px = (x + (sx + 0.5) / samples) * 24 / size;
            const double py = (y + (sy + 0.5) / samples) * 24 / size;
            double distance = 100;
            for (const auto& line : strokes) {
                const double dx = line.x2 - line.x1, dy = line.y2 - line.y1;
                const double length2 = dx * dx + dy * dy;
                const double t = length2 == 0 ? 0 : std::clamp(
                    ((px - line.x1) * dx + (py - line.y1) * dy) / length2, 0.0, 1.0);
                distance = std::min(distance, std::hypot(px - line.x1 - t * dx, py - line.y1 - t * dy));
            }
            // Near-white ink plus a subtle dark keyline for light panels.
            if (distance <= 1.1) { alpha += 1; color += 245; }
            else if (distance <= 1.55) { alpha += 0.65; color += 28 * 0.65; }
        }
        const auto offset = (y * size + x) * 4;
        // SNI uses straight-alpha, network-order A,R,G,B (not host-endian words).
        pixels[offset] = static_cast<uint8_t>(std::lround(255 * alpha / (samples * samples)));
        if (alpha > 0) {
            const auto ink = static_cast<uint8_t>(std::lround(color / alpha));
            pixels[offset + 1] = pixels[offset + 2] = pixels[offset + 3] = ink;
        }
    }
    return pixels;
}
int pixmap(sd_bus_message* reply, InputStatus status) {
    int result = sd_bus_message_open_container(reply, 'a', "(iiay)");
    for (const int size : {24, 48}) {
        if (result < 0) break;
        const auto pixels = renderIcon(status, size);
        result = sd_bus_message_open_container(reply, 'r', "iiay");
        if (result >= 0) result = sd_bus_message_append(reply, "ii", size, size);
        if (result >= 0) result = sd_bus_message_append_array(reply, 'y', pixels.data(), pixels.size());
        if (result >= 0) result = sd_bus_message_close_container(reply);
    }
    if (result >= 0) result = sd_bus_message_close_container(reply);
    return result;
}
int menuFields(sd_bus_message* reply, int id, const std::string& title, ActionResult action,
               InputStatus status, DesktopSession session, HotkeyStatus hotkey) {
    int result = sd_bus_message_open_container(reply, 'a', "{sv}");
    if (id != 0 && validMenuId(id) && result >= 0) {
        const char* text = title.c_str();
        if (id == quitMenuId) text = "Thoát tray OpenKey (giữ bộ gõ)";
        else if (id == 910) text = "Chuyển sang Telex";
        else if (id == 911) text = "Chuyển sang VNI";
        else if (id == 912) text = "Chuyển sang Simple Telex 1";
        else if (id == 913) text = "Chuyển sang Simple Telex 2";
        else if (id == englishMenuId) text = "Chuyển sang tiếng Anh (US)";
        else if (id == configurationMenuId) text = "Cấu hình bộ gõ Fcitx5…";
        else if (id == settingsMenuId) text = "Cài đặt OpenKey…";
        else if (id == 990) text = "Cấu hình gõ tắt…";
        else if (id == 980) text = "Công cụ chuyển mã…";
        else if (id == 1010) text = "Giới thiệu OpenKey";
        else if (id == 2) text = actionLabel(action);
        else if (id == 3) text = hotkeyHint(session, hotkey);
        else if (id == 4) text = "Đổi phím tắt trong cấu hình Fcitx5";
        result = sd_bus_message_open_container(reply, 'e', "sv");
        if (result >= 0) result = sd_bus_message_append(reply, "s", "label");
        if (result >= 0) result = sd_bus_message_open_container(reply, 'v', "s");
        if (result >= 0) result = sd_bus_message_append(reply, "s", text);
        if (result >= 0) result = sd_bus_message_close_container(reply);
        if (result >= 0) result = sd_bus_message_close_container(reply);
        if (inputMenuId(id)) {
            if (result >= 0) result = sd_bus_message_open_container(reply, 'e', "sv");
            if (result >= 0) result = sd_bus_message_append(reply, "s", "toggle-type");
            if (result >= 0) result = sd_bus_message_open_container(reply, 'v', "s");
            if (result >= 0) result = sd_bus_message_append(reply, "s", "radio");
            if (result >= 0) result = sd_bus_message_close_container(reply);
            if (result >= 0) result = sd_bus_message_close_container(reply);
            if (result >= 0) result = sd_bus_message_open_container(reply, 'e', "sv");
            if (result >= 0) result = sd_bus_message_append(reply, "s", "toggle-state");
            if (result >= 0) result = sd_bus_message_open_container(reply, 'v', "i");
            const bool selected = (id == 910 && status == InputStatus::Telex) ||
                                  (id == 911 && status == InputStatus::Vni) ||
                                  (id == 912 && status == InputStatus::SimpleTelex1) ||
                                  (id == 913 && status == InputStatus::SimpleTelex2) ||
                                  (id == englishMenuId && status == InputStatus::English);
            if (result >= 0) result = sd_bus_message_append(reply, "i", selected ? 1 : 0);
            if (result >= 0) result = sd_bus_message_close_container(reply);
            if (result >= 0) result = sd_bus_message_close_container(reply);
        }
        if (result >= 0) result = sd_bus_message_open_container(reply, 'e', "sv");
        if (result >= 0) result = sd_bus_message_append(reply, "s", "enabled");
        if (result >= 0) result = sd_bus_message_open_container(reply, 'v', "b");
        if (result >= 0) result = sd_bus_message_append(reply, "b", id == quitMenuId ||
            ((inputMenuId(id) || id == configurationMenuId || settingsMenuIdForPage(id)) && action != ActionResult::Busy));
        if (result >= 0) result = sd_bus_message_close_container(reply);
        if (result >= 0) result = sd_bus_message_close_container(reply);
    }
    return result < 0 ? result : sd_bus_message_close_container(reply);
}
int menuNode(sd_bus_message* reply, int id, int depth, const std::string& title, ActionResult action,
             InputStatus status, DesktopSession session, HotkeyStatus hotkey) {
    int result = sd_bus_message_open_container(reply, 'r', "ia{sv}av");
    if (result >= 0) result = sd_bus_message_append(reply, "i", id);
    if (result >= 0) result = menuFields(reply, id, title, action, status, session, hotkey);
    if (result >= 0) result = sd_bus_message_open_container(reply, 'a', "v");
    if (result >= 0 && id == 0 && depth != 0) {
        for (const int child : {1, 910, 911, 912, 913, englishMenuId, 2, settingsMenuId, 990, 980, 1010, configurationMenuId, 3, 4, quitMenuId}) {
            if (result >= 0) result = sd_bus_message_open_container(reply, 'v', "(ia{sv}av)");
            if (result >= 0) result = menuNode(reply, child, 0, title, action, status, session, hotkey);
            if (result >= 0) result = sd_bus_message_close_container(reply);
        }
    }
    if (result >= 0) result = sd_bus_message_close_container(reply);
    if (result >= 0) result = sd_bus_message_close_container(reply);
    return result;
}
}
const char* TrayItem::label(InputStatus status) {
    if (status == InputStatus::Telex) return "T";
    if (status == InputStatus::Vni) return "N";
    if (status == InputStatus::SimpleTelex1) return "S1";
    if (status == InputStatus::SimpleTelex2) return "S2";
    if (status == InputStatus::English) return "E";
    if (status == InputStatus::Inactive) return "-";
    if (status == InputStatus::Other) return "O";
    return "?";
}
std::string TrayItem::title() const {
    if (status_ == InputStatus::Telex) return "OpenKey — Telex (sampled)";
    if (status_ == InputStatus::Vni) return "OpenKey — VNI (sampled)";
    if (status_ == InputStatus::SimpleTelex1) return "OpenKey — Simple Telex 1 (sampled)";
    if (status_ == InputStatus::SimpleTelex2) return "OpenKey — Simple Telex 2 (sampled)";
    return std::string("OpenKey: ") + statusName(status_) + " (sampled)";
}
TrayItem::~TrayItem() { disconnect(); }
void TrayItem::disconnect() {
    registration_ = sd_bus_slot_unref(registration_);
    watch_ = sd_bus_slot_unref(watch_);
    menu_ = sd_bus_slot_unref(menu_);
    item_ = sd_bus_slot_unref(item_);
    bus_ = sd_bus_unref(bus_);
    registered_ = false;
}
const char* TrayItem::registrationState() const {
    return !enabled_ ? "disabled" : !bus_ ? "unavailable" : registered_ ? "registered" : "waiting-host";
}
int TrayItem::property(sd_bus*, const char*, const char*, const char* property,
                       sd_bus_message* reply, void* data, sd_bus_error*) {
    auto& self = *static_cast<TrayItem*>(data);
    if (!std::strcmp(property, "IconPixmap")) return pixmap(reply, self.status_);
    if (!std::strcmp(property, "OverlayIconPixmap") || !std::strcmp(property, "AttentionIconPixmap"))
        return emptyPixmaps(reply);
    if (!std::strcmp(property, "WindowId")) return sd_bus_message_append(reply, "i", 0);
    if (!std::strcmp(property, "ItemIsMenu")) return sd_bus_message_append(reply, "b", 1);
    if (!std::strcmp(property, "Menu")) return sd_bus_message_append(reply, "o", "/Menu");
    if (!std::strcmp(property, "ToolTip")) {
        int result = sd_bus_message_open_container(reply, 'r', "sa(iiay)ss");
        if (result >= 0) result = sd_bus_message_append(reply, "s", "");
        if (result >= 0) result = emptyPixmaps(reply);
        if (result >= 0) result = sd_bus_message_append(reply, "ss", self.title().c_str(),
            "Input status indicator. Quit stops only the tray, not Fcitx5.");
        return result < 0 ? result : sd_bus_message_close_container(reply);
    }
    const char* value = "";
    const auto title = self.title();
    if (!std::strcmp(property, "Category")) value = "SystemServices";
    else if (!std::strcmp(property, "Id")) value = "openkey-session";
    else if (!std::strcmp(property, "Title")) value = title.c_str();
    else if (!std::strcmp(property, "Status")) value = "Active";
    // Letter is drawn into IconPixmap; don't add a second visible V/E label.
    return sd_bus_message_append(reply, "s", value);
}
int TrayItem::noAction(sd_bus_message* message, void*, sd_bus_error*) {
    return sd_bus_reply_method_return(message, "");
}
int TrayItem::menuEvent(sd_bus_message* message, void* data, sd_bus_error*) {
    int id = 0;
    const char* event = nullptr;
    uint32_t timestamp = 0;
    int result = sd_bus_message_read(message, "is", &id, &event);
    if (result >= 0) result = sd_bus_message_skip(message, "v");
    if (result >= 0) result = sd_bus_message_read(message, "u", &timestamp);
    if (result < 0) return result;
    if (!validMenuId(id))
        return sd_bus_reply_method_errorf(message, SD_BUS_ERROR_INVALID_ARGS, "Unknown menu item");
    // Acknowledge before asking the owning main loop to shut down cleanly.
    result = sd_bus_reply_method_return(message, "");
    if (result >= 0 && !std::strcmp(event, "clicked")) {
        auto& self = *static_cast<TrayItem*>(data);
        if (id == quitMenuId) {
            self.quitRequested_ = true;
            sd_bus_flush(sd_bus_message_get_bus(message));
        } else if (!self.quitRequested_ && inputMenuId(id)) {
            self.actions_.request(id == 910 ? InputMethod::Telex :
                                  id == 911 ? InputMethod::Vni :
                                  id == 912 ? InputMethod::SimpleTelex1 :
                                  id == 913 ? InputMethod::SimpleTelex2 : InputMethod::English);
        } else if (!self.quitRequested_ && id == configurationMenuId) {
            self.actions_.requestConfiguration();
        } else if (!self.quitRequested_ && settingsMenuIdForPage(id)) {
            self.actions_.requestSettings(id == 990 ? SettingsPage::Macro :
                id == 980 ? SettingsPage::Convert : id == 1010 ? SettingsPage::About : SettingsPage::General);
        }
    }
    return result;
}
int TrayItem::menuProperty(sd_bus*, const char*, const char*, const char* name,
                           sd_bus_message* reply, void*, sd_bus_error*) {
    if (!std::strcmp(name, "Version")) return sd_bus_message_append(reply, "u", 3u);
    return sd_bus_message_append(reply, "s", !std::strcmp(name, "Status") ? "normal" : "ltr");
}
int TrayItem::getLayout(sd_bus_message* message, void* data, sd_bus_error*) {
    auto& self = *static_cast<TrayItem*>(data);
    int id = 0, depth = 0;
    int result = sd_bus_message_read(message, "ii", &id, &depth);
    if (result < 0) return result;
    if (!validMenuId(id)) return sd_bus_reply_method_errorf(message, SD_BUS_ERROR_INVALID_ARGS, "Unknown menu item");
    sd_bus_message* reply = nullptr;
    result = sd_bus_message_new_method_return(message, &reply);
    if (result >= 0) result = sd_bus_message_append(reply, "u", self.revision_);
    if (result >= 0) result = menuNode(reply, id, depth, self.title(), self.actions_.result(), self.status_, self.session_, self.hotkey_);
    if (result >= 0) result = sd_bus_send(nullptr, reply, nullptr);
    sd_bus_message_unref(reply);
    return result < 0 ? result : 1;
}
int TrayItem::groupProperties(sd_bus_message* message, void* data, sd_bus_error*) {
    auto& self = *static_cast<TrayItem*>(data);
    sd_bus_message* reply = nullptr;
    int result = sd_bus_message_new_method_return(message, &reply);
    if (result >= 0) result = sd_bus_message_open_container(reply, 'a', "(ia{sv})");
    if (result >= 0) result = sd_bus_message_enter_container(message, 'a', "i");
    int id = 0;
    std::vector<int> ids;
    while (result >= 0 && sd_bus_message_read(message, "i", &id) > 0) {
        if (ids.size() >= 32) { sd_bus_message_unref(reply);
            return sd_bus_reply_method_errorf(message, SD_BUS_ERROR_INVALID_ARGS, "Too many menu IDs"); }
        ids.push_back(id);
    }
    if (ids.empty()) ids = {0, 1, 910, 911, 912, 913, englishMenuId, 2, settingsMenuId, configurationMenuId, 3, 4, quitMenuId};
    for (const auto requested : ids) {
        id = requested;
        if (!validMenuId(id)) continue;
        result = sd_bus_message_open_container(reply, 'r', "ia{sv}");
        if (result >= 0) result = sd_bus_message_append(reply, "i", id);
        if (result >= 0) result = menuFields(reply, id, self.title(), self.actions_.result(), self.status_, self.session_, self.hotkey_);
        if (result >= 0) result = sd_bus_message_close_container(reply);
    }
    if (result >= 0) result = sd_bus_message_close_container(reply);
    if (result >= 0) result = sd_bus_send(nullptr, reply, nullptr);
    sd_bus_message_unref(reply);
    return result < 0 ? result : 1;
}
int TrayItem::aboutToShow(sd_bus_message* message, void*, sd_bus_error*) {
    return sd_bus_reply_method_return(message, "b", 0);
}
int TrayItem::registered(sd_bus_message* message, void* data, sd_bus_error*) {
    auto& self = *static_cast<TrayItem*>(data);
    self.registered_ = !sd_bus_message_is_method_error(message, nullptr);
    self.registration_ = sd_bus_slot_unref(self.registration_);
    self.retry_ = Clock::now() + std::chrono::seconds(5);
    return 0;
}
int TrayItem::ownerChanged(sd_bus_message* message, void* data, sd_bus_error*) {
    const char *name = nullptr, *oldOwner = nullptr, *newOwner = nullptr;
    if (sd_bus_message_read(message, "sss", &name, &oldOwner, &newOwner) < 0) return 0;
    if (std::strcmp(name, watcher)) return 0;
    auto& self = *static_cast<TrayItem*>(data);
    self.registration_ = sd_bus_slot_unref(self.registration_);
    self.registered_ = false;
    self.retry_ = Clock::now();
    return 0;
}
bool TrayItem::connect() {
    disconnect();
    if (sd_bus_open_user(&bus_) < 0) { disconnect(); return false; }
    static const sd_bus_vtable item[] = {
        SD_BUS_VTABLE_START(0),
#define PROP(name, type) SD_BUS_PROPERTY(name, type, property, 0, SD_BUS_VTABLE_PROPERTY_EMITS_CHANGE)
        PROP("Category", "s"), PROP("Id", "s"), PROP("Title", "s"), PROP("Status", "s"),
        PROP("WindowId", "i"), PROP("IconName", "s"), PROP("IconThemePath", "s"),
        PROP("IconPixmap", "a(iiay)"), PROP("OverlayIconName", "s"), PROP("OverlayIconPixmap", "a(iiay)"),
        PROP("AttentionIconName", "s"), PROP("AttentionIconPixmap", "a(iiay)"), PROP("AttentionMovieName", "s"),
        PROP("ToolTip", "(sa(iiay)ss)"), PROP("Menu", "o"), PROP("ItemIsMenu", "b"),
        PROP("XAyatanaLabel", "s"), PROP("XAyatanaLabelGuide", "s"),
#undef PROP
        SD_BUS_METHOD("Activate", "ii", "", noAction, SD_BUS_VTABLE_UNPRIVILEGED),
        SD_BUS_METHOD("SecondaryActivate", "ii", "", noAction, SD_BUS_VTABLE_UNPRIVILEGED),
        SD_BUS_METHOD("ContextMenu", "ii", "", noAction, SD_BUS_VTABLE_UNPRIVILEGED),
        SD_BUS_METHOD("Scroll", "is", "", noAction, SD_BUS_VTABLE_UNPRIVILEGED),
        SD_BUS_SIGNAL("NewIcon", "", 0), SD_BUS_SIGNAL("NewTitle", "", 0),
        SD_BUS_SIGNAL("NewToolTip", "", 0), SD_BUS_SIGNAL("NewStatus", "s", 0),
        SD_BUS_SIGNAL("XAyatanaNewLabel", "ss", 0), SD_BUS_VTABLE_END
    };
    static const sd_bus_vtable menu[] = {
        SD_BUS_VTABLE_START(0),
        SD_BUS_PROPERTY("Version", "u", menuProperty, 0, SD_BUS_VTABLE_PROPERTY_CONST),
        SD_BUS_PROPERTY("TextDirection", "s", menuProperty, 0, SD_BUS_VTABLE_PROPERTY_CONST),
        SD_BUS_PROPERTY("Status", "s", menuProperty, 0, SD_BUS_VTABLE_PROPERTY_CONST),
        SD_BUS_METHOD("GetLayout", "iias", "u(ia{sv}av)", getLayout, SD_BUS_VTABLE_UNPRIVILEGED),
        SD_BUS_METHOD("GetGroupProperties", "aias", "a(ia{sv})", groupProperties, SD_BUS_VTABLE_UNPRIVILEGED),
        SD_BUS_METHOD("Event", "isvu", "", menuEvent, SD_BUS_VTABLE_UNPRIVILEGED),
        SD_BUS_METHOD("AboutToShow", "i", "b", aboutToShow, SD_BUS_VTABLE_UNPRIVILEGED),
        SD_BUS_SIGNAL("LayoutUpdated", "ui", 0), SD_BUS_VTABLE_END
    };
    if (sd_bus_add_object_vtable(bus_, &item_, itemPath, itemInterface, item, this) < 0 ||
        sd_bus_add_object_vtable(bus_, &menu_, "/Menu", menuInterface, menu, this) < 0 ||
        sd_bus_match_signal(bus_, &watch_, "org.freedesktop.DBus", "/org/freedesktop/DBus",
            "org.freedesktop.DBus", "NameOwnerChanged", ownerChanged, this) < 0) {
        disconnect(); return false;
    }
    return true;
}
void TrayItem::registerItem() {
    sd_bus_message* message = nullptr;
    int result = sd_bus_message_new_method_call(bus_, &message, watcher, "/StatusNotifierWatcher",
        watcher, "RegisterStatusNotifierItem");
    if (result >= 0) result = sd_bus_message_set_auto_start(message, 0);
    if (result >= 0) result = sd_bus_message_append(message, "s", itemPath);
    if (result >= 0) result = sd_bus_call_async(bus_, &registration_, message, registered, this, 150000);
    sd_bus_message_unref(message);
    if (result < 0) { registration_ = sd_bus_slot_unref(registration_); retry_ = Clock::now() + std::chrono::seconds(5); }
}
void TrayItem::tick(InputStatus status, HotkeyStatus hotkey) {
    if (!enabled_) return;
    actions_.tick();
    const auto now = Clock::now();
    if (!bus_) {
        if (now < retry_) return;
        retry_ = now + std::chrono::seconds(5);
        if (!connect()) return;
        retry_ = now;
    }
    if (hotkey_ != hotkey) {
        hotkey_ = hotkey;
        ++revision_;
        sd_bus_emit_signal(bus_, "/Menu", menuInterface, "LayoutUpdated", "ui", revision_, 0);
    }
    for (unsigned i = 0; i < 32; ++i) {
        const int result = sd_bus_process(bus_, nullptr);
        if (result < 0) { disconnect(); retry_ = now + std::chrono::seconds(5); return; }
        if (!result) break;
    }
    if (status != status_) {
        status_ = status;
        ++revision_;
        sd_bus_emit_properties_changed(bus_, itemPath, itemInterface, "IconPixmap", "Title", "ToolTip", "XAyatanaLabel", nullptr);
        sd_bus_emit_signal(bus_, itemPath, itemInterface, "NewIcon", "");
        sd_bus_emit_signal(bus_, itemPath, itemInterface, "NewTitle", "");
        sd_bus_emit_signal(bus_, itemPath, itemInterface, "NewToolTip", "");
        sd_bus_emit_signal(bus_, itemPath, itemInterface, "XAyatanaNewLabel", "ss", "", "");
        sd_bus_emit_signal(bus_, "/Menu", menuInterface, "LayoutUpdated", "ui", revision_, 0);
    }
    if (actions_.result() != actionResult_) {
        actionResult_ = actions_.result();
        ++revision_;
        sd_bus_emit_signal(bus_, "/Menu", menuInterface, "LayoutUpdated", "ui", revision_, 0);
    }
    if (!registered_ && !registration_ && now >= retry_) registerItem();
}
}
