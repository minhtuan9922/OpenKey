// SNI integration on a private bus, with no desktop host or real Fcitx changes.
#include "TrayItem.h"
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <vector>
#include <spawn.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

extern char** environ;

namespace {
using namespace std::chrono_literals;
void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
template<class Predicate> void await(Predicate predicate) {
    const auto deadline = std::chrono::steady_clock::now() + 3s;
    while (!predicate()) {
        require(std::chrono::steady_clock::now() < deadline, "Tray transition timed out");
        std::this_thread::sleep_for(2ms);
    }
}
struct TrayLoop {
    openkey::TrayItem tray;
    std::atomic<openkey::InputStatus> status{openkey::InputStatus::Unknown};
    std::atomic<openkey::HotkeyStatus> hotkey{openkey::HotkeyStatus::Disabled};
    std::atomic<bool> stop{false}, connected{false}, registered{false}, quitRequested{false};
    std::thread worker;
    explicit TrayLoop(openkey::DesktopSession session = openkey::DesktopSession::Wayland)
        : tray(true, session), worker([this] {
        while (!stop) {
            tray.tick(status.load(), hotkey.load());
            quitRequested = tray.quitRequested();
            const std::string state = tray.registrationState();
            connected = state == "waiting-host" || state == "registered";
            registered = state == "registered";
            std::this_thread::sleep_for(1ms);
        }
    }) {}
    ~TrayLoop() { stop = true; worker.join(); }
};
struct Watcher {
    sd_bus* bus = nullptr;
    sd_bus_slot* slot = nullptr;
    std::atomic<bool> stop{false}, wanted{true}, owned{true}, noAutoStart{true};
    std::atomic<unsigned> registrations{0};
    std::mutex mutex;
    std::string sender;
    std::thread worker;
    static int registration(sd_bus_message* message, void* data, sd_bus_error*) {
        auto& self = *static_cast<Watcher*>(data);
        const char* path = nullptr;
        if (sd_bus_message_read(message, "s", &path) <= 0 || std::string(path) != "/StatusNotifierItem")
            return sd_bus_reply_method_errorf(message, SD_BUS_ERROR_INVALID_ARGS, "Wrong item path");
        self.noAutoStart = self.noAutoStart && !sd_bus_message_get_auto_start(message);
        {
            std::lock_guard<std::mutex> lock(self.mutex);
            self.sender = sd_bus_message_get_sender(message);
        }
        ++self.registrations;
        return sd_bus_reply_method_return(message, "");
    }
    Watcher() {
        static const sd_bus_vtable table[] = {
            SD_BUS_VTABLE_START(0),
            SD_BUS_METHOD("RegisterStatusNotifierItem", "s", "", registration, SD_BUS_VTABLE_UNPRIVILEGED),
            SD_BUS_VTABLE_END
        };
        require(sd_bus_open_user(&bus) >= 0, "Cannot open private watcher bus");
        require(sd_bus_add_object_vtable(bus, &slot, "/StatusNotifierWatcher", "org.kde.StatusNotifierWatcher",
                                        table, this) >= 0, "Cannot export watcher");
        require(sd_bus_request_name(bus, "org.kde.StatusNotifierWatcher", 0) >= 0, "Cannot own private watcher name");
        worker = std::thread([this] {
            while (!stop) {
                if (wanted != owned) {
                    const int result = wanted ? sd_bus_request_name(bus, "org.kde.StatusNotifierWatcher", 0) :
                        sd_bus_release_name(bus, "org.kde.StatusNotifierWatcher");
                    if (result >= 0) owned = wanted.load();
                }
                if (sd_bus_process(bus, nullptr) <= 0) sd_bus_wait(bus, 1000);
            }
        });
    }
    ~Watcher() { stop = true; worker.join(); sd_bus_slot_unref(slot); sd_bus_unref(bus); }
    std::string owner() { std::lock_guard<std::mutex> lock(mutex); return sender; }
};
struct Client {
    sd_bus* bus = nullptr;
    sd_bus_slot* signal = nullptr;
    unsigned icons = 0;
    Client() { require(sd_bus_open_user(&bus) >= 0, "Cannot open private client"); sd_bus_set_method_call_timeout(bus, 500000); }
    ~Client() { sd_bus_slot_unref(signal); sd_bus_unref(bus); }
    std::string text(const std::string& owner, const char* property) {
        char* result = nullptr;
        const int code = sd_bus_get_property_string(bus, owner.c_str(), "/StatusNotifierItem",
            "org.kde.StatusNotifierItem", property, nullptr, &result);
        require(code >= 0, "Cannot read SNI text property");
        const std::string value(result);
        std::free(result);
        return value;
    }
    std::vector<uint8_t> pixels(const std::string& owner) {
        sd_bus_message* reply = nullptr;
        require(sd_bus_get_property(bus, owner.c_str(), "/StatusNotifierItem", "org.kde.StatusNotifierItem",
            "IconPixmap", nullptr, &reply, "a(iiay)") >= 0, "Cannot read pixmap");
        require(sd_bus_message_enter_container(reply, 'a', "(iiay)") > 0, "Missing pixmap array");
        std::vector<uint8_t> result;
        for (const int expected : {24, 48}) {
            int width = 0, height = 0;
            const void* bytes = nullptr;
            size_t size = 0;
            const bool valid = sd_bus_message_enter_container(reply, 'r', "iiay") > 0 &&
                sd_bus_message_read(reply, "ii", &width, &height) > 0 &&
                sd_bus_message_read_array(reply, 'y', &bytes, &size) > 0;
            require(valid && width == expected && height == expected &&
                    size == static_cast<size_t>(expected * expected * 4), "Invalid ARGB pixmap payload");
            const auto* data = static_cast<const uint8_t*>(bytes);
            bool transparent = false, opaque = false, antialiased = false;
            for (size_t i = 0; i < size; i += 4) {
                transparent |= data[i] == 0;
                opaque |= data[i] == 255 && data[i + 1] == 245;
                antialiased |= data[i] > 0 && data[i] < 255;
                require(data[i + 1] == data[i + 2] && data[i + 2] == data[i + 3],
                        "Icon must use monochrome network-order ARGB");
                if (data[i] == 0) require(data[i + 1] == 0, "Transparent pixels must have zero RGB");
            }
            require(data[0] == 0 && data[size - 4] == 0 && transparent && opaque && antialiased,
                    "Glyph needs transparent padding, opaque ink and antialiased edges at both scales");
            result.insert(result.end(), data, data + size);
            require(sd_bus_message_exit_container(reply) >= 0, "Cannot exit pixmap tuple");
        }
        require(sd_bus_message_enter_container(reply, 'r', "iiay") == 0, "Unexpected pixmap size");
        sd_bus_message_unref(reply);
        return result;
    }
    bool status(const std::string& owner, const char* expected) {
        if (std::string(expected) == "telex") return text(owner, "Title") == "OpenKey — Telex (sampled)";
        if (std::string(expected) == "vni") return text(owner, "Title") == "OpenKey — VNI (sampled)";
        return text(owner, "Title") == std::string("OpenKey: ") + expected + " (sampled)";
    }
    std::string tooltipTitle(const std::string& owner) {
        sd_bus_message* reply = nullptr;
        require(sd_bus_get_property(bus, owner.c_str(), "/StatusNotifierItem", "org.kde.StatusNotifierItem",
            "ToolTip", nullptr, &reply, "(sa(iiay)ss)") >= 0, "Cannot fetch tooltip");
        const char* value = nullptr;
        require(sd_bus_message_enter_container(reply, 'r', "sa(iiay)ss") > 0 &&
            sd_bus_message_skip(reply, "sa(iiay)") > 0 && sd_bus_message_read(reply, "s", &value) > 0,
            "Malformed tooltip title");
        const std::string title(value);
        sd_bus_message_unref(reply);
        return title;
    }
    int event(const std::string& owner, int id, const char* name) {
        return sd_bus_call_method(bus, owner.c_str(), "/Menu", "com.canonical.dbusmenu", "Event",
                                 nullptr, nullptr, "isvu", id, name, "i", 0, 0u);
    }
    bool menuItemMatches(const std::string& owner, int requested, const char* expectedLabel,
                         bool expectedEnabled, int expectedToggle = -1) {
        sd_bus_message* reply = nullptr;
        require(sd_bus_call_method(bus, owner.c_str(), "/Menu", "com.canonical.dbusmenu", "GetLayout",
            nullptr, &reply, "iias", requested, 0, 0) >= 0, "Cannot fetch menu item");
        uint32_t revision = 0;
        int id = 0;
        require(sd_bus_message_read(reply, "u", &revision) > 0 &&
            sd_bus_message_enter_container(reply, 'r', "ia{sv}av") > 0 &&
            sd_bus_message_read(reply, "i", &id) > 0 && id == requested &&
            sd_bus_message_enter_container(reply, 'a', "{sv}") > 0, "Malformed Quit item");
        bool enabled = false, labeled = false;
        bool checked = expectedToggle < 0, radio = expectedToggle < 0;
        while (sd_bus_message_enter_container(reply, 'e', "sv") > 0) {
            const char* key = nullptr;
            require(sd_bus_message_read(reply, "s", &key) > 0, "Missing Quit property");
            if (std::string(key) == "enabled") {
                int value = 0;
                require(sd_bus_message_enter_container(reply, 'v', "b") > 0 &&
                    sd_bus_message_read(reply, "b", &value) > 0, "Invalid Quit enabled flag");
                enabled = static_cast<bool>(value) == expectedEnabled;
                sd_bus_message_exit_container(reply);
            } else if (std::string(key) == "label") {
                const char* value = nullptr;
                require(sd_bus_message_enter_container(reply, 'v', "s") > 0 &&
                    sd_bus_message_read(reply, "s", &value) > 0, "Invalid Quit label");
                labeled = std::string(value) == expectedLabel;
                sd_bus_message_exit_container(reply);
            } else if (std::string(key) == "toggle-state") {
                int value = -1;
                require(sd_bus_message_enter_container(reply, 'v', "i") > 0 &&
                    sd_bus_message_read(reply, "i", &value) > 0, "Invalid toggle-state");
                checked = expectedToggle < 0 || value == expectedToggle;
                sd_bus_message_exit_container(reply);
            } else if (std::string(key) == "toggle-type") {
                const char* value = nullptr;
                require(sd_bus_message_enter_container(reply, 'v', "s") > 0 &&
                    sd_bus_message_read(reply, "s", &value) > 0, "Invalid toggle-type");
                radio = std::string(value) == "radio";
                sd_bus_message_exit_container(reply);
            } else sd_bus_message_skip(reply, "v");
            sd_bus_message_exit_container(reply);
        }
        sd_bus_message_unref(reply);
        return enabled && labeled && checked && radio;
    }
};
struct ServiceChild {
    std::string runtime;
    pid_t pid = -1;
    int exitStatus = -1;
    explicit ServiceChild(char* binary) {
        char pattern[] = "/tmp/openkey-tray-exit.XXXXXXXX";
        const auto* directory = mkdtemp(pattern);
        require(directory != nullptr, "Cannot create child runtime");
        runtime = directory;
        std::vector<std::string> environment;
        for (auto** entry = environ; *entry; ++entry)
            if (std::string(*entry).find("XDG_RUNTIME_DIR=") != 0) environment.emplace_back(*entry);
        environment.push_back("XDG_RUNTIME_DIR=" + runtime);
        std::vector<char*> env;
        for (auto& entry : environment) env.push_back(entry.data());
        env.push_back(nullptr);
        char argument[] = "--run-tray";
        char* arguments[] = {binary, argument, nullptr};
        const int code = posix_spawn(&pid, binary, nullptr, nullptr, arguments, env.data());
        if (code != 0) { rmdir(runtime.c_str()); throw std::runtime_error("Cannot spawn service"); }
    }
    bool exited() {
        if (pid < 0) return true;
        const auto result = waitpid(pid, &exitStatus, WNOHANG);
        require(result >= 0, "Cannot wait for child service");
        if (result == 0) return false;
        pid = -1;
        return true;
    }
    ~ServiceChild() {
        if (pid > 0) { kill(pid, SIGTERM); waitpid(pid, nullptr, 0); }
        // Exact files belonging to this test's mkdtemp runtime, never live IPC.
        unlink((runtime + "/openkey/session.sock").c_str());
        unlink((runtime + "/openkey/session.lock").c_str());
        rmdir((runtime + "/openkey").c_str());
        rmdir(runtime.c_str());
    }
};
}
int main(int argc, char** argv) {
    require(argc == 2, "Expected service binary for end-to-end Quit test");
    using openkey::InputStatus;
    using openkey::DesktopSession;
    require(openkey::desktopSession("wayland") == DesktopSession::Wayland &&
            openkey::desktopSession("x11") == DesktopSession::X11 &&
            openkey::desktopSession("") == DesktopSession::Unknown &&
            openkey::desktopSession("tty") == DesktopSession::Unknown,
            "Only explicit XDG session types may identify X11/Wayland");
    openkey::TrayItem disabled(false);
    disabled.tick(InputStatus::Telex);
    require(std::string(disabled.registrationState()) == "disabled", "Tray must be opt-in");
    require(std::string(openkey::TrayItem::label(InputStatus::Inactive)) == "-" &&
            std::string(openkey::TrayItem::label(InputStatus::Other)) == "O", "Non-V/E states must not be mislabeled");
    require(std::string(openkey::TrayItem::label(InputStatus::Telex)) == "T" &&
            std::string(openkey::TrayItem::label(InputStatus::Vni)) == "N" &&
            std::string(openkey::TrayItem::label(InputStatus::English)) == "E", "T/N/E labels must be distinct");
    TrayLoop tray;
    await([&] { return tray.connected.load(); });
    require(!tray.registered, "No host must not be reported registered");
    Watcher watcher;
    await([&] { return watcher.registrations > 0 && tray.registered; });
    require(watcher.noAutoStart, "Registration must not auto-start a host");
    const auto owner = watcher.owner();
    Client client;
    sd_bus_message* properties = nullptr;
    require(sd_bus_call_method(client.bus, owner.c_str(), "/StatusNotifierItem", "org.freedesktop.DBus.Properties",
        "GetAll", nullptr, &properties, "s", "org.kde.StatusNotifierItem") >= 0,
        "Host must be able to load all SNI properties including tooltip/pixmaps");
    require(sd_bus_message_has_signature(properties, "a{sv}") > 0, "Malformed SNI property dictionary");
    sd_bus_message_unref(properties);
    require(client.text(owner, "Id") == "openkey-session" && client.status(owner, "unknown"),
            "Unknown initial status must not show V/E");
    require(sd_bus_match_signal(client.bus, &client.signal, owner.c_str(), "/StatusNotifierItem",
        "org.kde.StatusNotifierItem", "NewIcon", [](sd_bus_message*, void* data, sd_bus_error*) {
            ++static_cast<Client*>(data)->icons; return 0;
        }, &client) >= 0, "Cannot subscribe to icon changes");
    tray.status = InputStatus::Telex;
    await([&] { return client.status(owner, "telex"); });
    const auto telex = client.pixels(owner);
    require(client.tooltipTitle(owner) == "OpenKey — Telex (sampled)", "Telex tooltip must name the input method");
    require(client.menuItemMatches(owner, 910, "Chuyển sang Telex", true, 1) &&
            client.menuItemMatches(owner, 911, "Chuyển sang VNI", true, 0), "Only Telex should be selected");
    require(client.menuItemMatches(owner, 920, "Chuyển sang tiếng Anh (US)", true, 0), "English must not be selected in Telex");
    // An external IM switch (e.g. a keyboard shortcut), not a menu click.
    tray.status = InputStatus::Vni;
    await([&] { return client.status(owner, "vni"); });
    const auto vni = client.pixels(owner);
    require(vni != telex && client.tooltipTitle(owner) == "OpenKey — VNI (sampled)", "VNI needs a distinct glyph and tooltip");
    require(client.menuItemMatches(owner, 910, "Chuyển sang Telex", true, 0) &&
            client.menuItemMatches(owner, 911, "Chuyển sang VNI", true, 1), "External switch must select only VNI");
    require(client.menuItemMatches(owner, 920, "Chuyển sang tiếng Anh (US)", true, 0), "English must not be selected in VNI");
    tray.status = InputStatus::English;
    await([&] { return client.status(owner, "english"); });
    require(client.text(owner, "XAyatanaLabel").empty(), "Do not duplicate the glyph with an extra visible label");
    require(client.pixels(owner) != telex && client.pixels(owner) != vni, "T/N/E must have distinct glyphs");
    for (const auto status : {InputStatus::English, InputStatus::Inactive, InputStatus::Other,
                              InputStatus::Unknown, InputStatus::Unavailable}) {
        tray.status = status;
        await([&] { return client.status(owner, openkey::statusName(status)); });
        require(client.pixels(owner) != telex && client.pixels(owner) != vni, "Fallback glyph must differ from T/N");
        require(client.menuItemMatches(owner, 910, "Chuyển sang Telex", true, 0) &&
                client.menuItemMatches(owner, 911, "Chuyển sang VNI", true, 0), "Invalid/non-OpenKey state must clear both selections");
        require(client.menuItemMatches(owner, 920, "Chuyển sang tiếng Anh (US)", true,
                status == InputStatus::English ? 1 : 0), "English must be selected only from confirmed US keyboard status");
    }
    tray.status = InputStatus::English;
    await([&] { return client.status(owner, "english"); });
    while (sd_bus_process(client.bus, nullptr) > 0) {}
    require(client.icons > 0, "State changes must notify the host");
    sd_bus_message* layout = nullptr;
    // -1 depth and empty property filter: full menu tree.
    require(sd_bus_call_method(client.bus, owner.c_str(), "/Menu", "com.canonical.dbusmenu", "GetLayout",
        nullptr, &layout, "iias", 0, -1, 0) >= 0, "Cannot fetch indicator menu");
    require(sd_bus_message_has_signature(layout, "u(ia{sv}av)") > 0, "Malformed menu layout signature");
    sd_bus_message_unref(layout);
    sd_bus_message* group = nullptr;
    require(sd_bus_call_method(client.bus, owner.c_str(), "/Menu", "com.canonical.dbusmenu", "GetGroupProperties",
        nullptr, &group, "aias", 1, 1, 0) >= 0, "Cannot fetch status menu properties");
    int menuId = 0;
    require(sd_bus_message_enter_container(group, 'a', "(ia{sv})") > 0 &&
        sd_bus_message_enter_container(group, 'r', "ia{sv}") > 0 &&
        sd_bus_message_read(group, "i", &menuId) > 0 && menuId == 1 &&
        sd_bus_message_enter_container(group, 'a', "{sv}") > 0, "Malformed status menu properties");
    bool menuDisabled = false;
    while (sd_bus_message_enter_container(group, 'e', "sv") > 0) {
        const char* key = nullptr;
        require(sd_bus_message_read(group, "s", &key) > 0, "Missing menu property name");
        if (std::string(key) == "enabled") {
            int enabled = 1;
            require(sd_bus_message_enter_container(group, 'v', "b") > 0 &&
                sd_bus_message_read(group, "b", &enabled) > 0, "Invalid menu enabled flag");
            menuDisabled = !enabled;
            sd_bus_message_exit_container(group);
        } else sd_bus_message_skip(group, "v");
        sd_bus_message_exit_container(group);
    }
    require(menuDisabled, "Read-only status menu item must be disabled");
    sd_bus_message_unref(group);
    require(client.menuItemMatches(owner, 2000, "Thoát tray OpenKey (giữ bộ gõ)", true), "Invalid Quit menu");
    require(client.menuItemMatches(owner, 1001, "Cấu hình bộ gõ Fcitx5…", true), "Missing Fcitx settings menu");
    require(client.menuItemMatches(owner, 1000, "Cài đặt OpenKey…", true), "Missing OpenKey settings menu");
    require(client.menuItemMatches(owner, 3, "Wayland: hotkey riêng chưa hỗ trợ", false) &&
            client.menuItemMatches(owner, 4, "Đổi phím tắt trong cấu hình Fcitx5", false), "Missing readonly Wayland hotkey guidance");
    require(client.event(owner, 3, "clicked") >= 0 && client.event(owner, 4, "clicked") >= 0,
            "Read-only hotkey guidance must acknowledge without any action");
    require(client.menuItemMatches(owner, 910, "Chuyển sang Telex", true) &&
            client.menuItemMatches(owner, 911, "Chuyển sang VNI", true), "Missing Telex/VNI actions");
    require(client.event(owner, 911, "hovered") >= 0 && client.event(owner, 920, "hovered") >= 0 &&
            client.event(owner, 1001, "hovered") >= 0 && client.event(owner, 1000, "hovered") >= 0, "Hover must be inert");
    require(client.menuItemMatches(owner, 2, openkey::actionLabel(openkey::ActionResult::Idle), false),
            "Hover must not dispatch an input action");
    require(client.event(owner, 911, "clicked") >= 0, "Input action must acknowledge the host");
    await([&] { return client.menuItemMatches(owner, 2,
        openkey::actionLabel(openkey::ActionResult::Unavailable), false); });
    require(client.status(owner, "english"), "Action failure must not invent a new input status");
    require(client.event(owner, 920, "clicked") >= 0, "English menu action must acknowledge the host");
    await([&] { return client.menuItemMatches(owner, 2,
        openkey::actionLabel(openkey::ActionResult::Unavailable), false); });
    require(!tray.quitRequested, "English action must never stop the tray");
    require(client.event(owner, 1001, "clicked") >= 0, "Settings menu must acknowledge the host");
    await([&] { return client.menuItemMatches(owner, 2,
        openkey::actionLabel(openkey::ActionResult::Unavailable), false); });
    require(client.status(owner, "english") && !tray.quitRequested,
            "Missing Fcitx must not change input status or stop the tray on Configure");
    require(client.event(owner, 1000, "clicked") >= 0, "OpenKey settings must acknowledge host");
    await([&] { return client.menuItemMatches(owner, 2,
        openkey::actionLabel(openkey::ActionResult::SettingsUnavailable), false); });
    require(client.status(owner, "english") && !tray.quitRequested,
            "Missing UI must not change input status or stop tray");
    require(client.event(owner, 2000, "opened") >= 0 &&
            client.event(owner, 2000, "hovered") >= 0 &&
            client.event(owner, 2000, "closed") >= 0, "Non-click menu events must be acknowledged");
    require(client.event(owner, 1, "clicked") >= 0 && client.event(owner, 0, "clicked") >= 0,
            "Root/status clicks must be inert");
    require(client.event(owner, 9999, "clicked") < 0, "Unknown menu IDs must be rejected");
    require(!tray.quitRequested, "Only an explicit Quit click may request shutdown");
    require(sd_bus_call_method(client.bus, owner.c_str(), "/StatusNotifierItem", "org.kde.StatusNotifierItem",
        "Activate", nullptr, nullptr, "ii", 0, 0) >= 0, "Read-only activate must safely acknowledge");
    require(client.status(owner, "english"), "Click must not change input status");
    tray.status = InputStatus::Unavailable;
    await([&] { return client.status(owner, "unavailable"); });
    require(client.text(owner, "Title").find("unavailable") != std::string::npos,
            "Tooltip/title must distinguish unavailable from English");
    watcher.wanted = false;
    await([&] { return !watcher.owned && !tray.registered; });
    watcher.wanted = true;
    await([&] { return watcher.registrations >= 2 && tray.registered; });
    require(!tray.quitRequested, "Registration changes must not request shutdown");
    require(client.event(owner, 2000, "clicked") >= 0, "Quit must acknowledge the menu host");
    await([&] { return tray.quitRequested.load(); });
    require(client.status(owner, "unavailable"), "Quit request must not change input state");
    const auto previousRegistrations = watcher.registrations.load();
    ServiceChild child(argv[1]);
    await([&] { return watcher.registrations > previousRegistrations && watcher.owner() != owner; });
    const auto childOwner = watcher.owner();
    require(client.menuItemMatches(childOwner, 2000, "Thoát tray OpenKey (giữ bộ gõ)", true), "Invalid child Quit menu");
    require(client.event(childOwner, 2000, "clicked") >= 0, "Running service must acknowledge Quit");
    await([&] { return child.exited(); });
    require(WIFEXITED(child.exitStatus) && WEXITSTATUS(child.exitStatus) == 0,
            "Quit must stop the actual service cleanly");
    require(access((child.runtime + "/openkey/session.sock").c_str(), F_OK) != 0,
            "Quit must clean up service IPC");
    for (const auto session : {DesktopSession::X11, DesktopSession::Unknown}) {
        const auto registrations = watcher.registrations.load();
        TrayLoop sessionTray(session);
        await([&] { return sessionTray.registered && watcher.registrations > registrations; });
        require(client.menuItemMatches(watcher.owner(), 3, openkey::hotkeyHint(session), false),
                "X11/unknown session must not claim its own hotkeys are available");
        if (session == DesktopSession::X11) {
            sessionTray.hotkey = openkey::HotkeyStatus::Active;
            await([&] { return client.menuItemMatches(watcher.owner(), 3,
                openkey::hotkeyHint(session, openkey::HotkeyStatus::Active), false); });
            sessionTray.hotkey = openkey::HotkeyStatus::Conflict;
            await([&] { return client.menuItemMatches(watcher.owner(), 3,
                openkey::hotkeyHint(session, openkey::HotkeyStatus::Conflict), false); });
        }
    }
    std::cout << "Private-bus tray: registration, glyphs, signals, menu, Quit request and watcher recovery passed\n";
}
