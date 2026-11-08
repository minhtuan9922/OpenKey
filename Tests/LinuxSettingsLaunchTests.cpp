// The CTest wrapper supplies a private session bus, never the desktop bus.
#include "FcitxActions.h"
#include <systemd/sd-bus.h>
#include <atomic>
#include <chrono>
#include <iostream>
#include <stdexcept>
#include <thread>

namespace {
void require(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
struct Mock {
    sd_bus* bus = nullptr;
    sd_bus_slot* slot = nullptr;
    std::atomic<bool> stop{false}, fail{false}, slow{false}, autoStart{false};
    std::atomic<bool> noPages{false}, rejectPage{false};
    std::atomic<int> page{0};
    std::atomic<int> requests{0};
    std::thread worker;
    ~Mock() {
        stop = true;
        if (worker.joinable()) worker.join();
        sd_bus_slot_unref(slot);
        sd_bus_unref(bus);
    }
    static int show(sd_bus_message* message, void* data, sd_bus_error*) {
        auto& self = *static_cast<Mock*>(data);
        ++self.requests;
        self.autoStart = sd_bus_message_get_auto_start(message) > 0;
        const bool navigation = std::string(sd_bus_message_get_member(message)) == "ShowPage";
        if (navigation) {
            if (self.noPages) return sd_bus_reply_method_errorf(message, SD_BUS_ERROR_UNKNOWN_METHOD, "Old UI");
            const char* key = nullptr;
            if (sd_bus_message_read(message, "s", &key) < 0) return -1;
            self.page = std::string(key) == "macro" ? 1 : std::string(key) == "convert" ? 2 :
                std::string(key) == "about" ? 3 : -1;
        }
        if (self.slow) std::this_thread::sleep_for(std::chrono::milliseconds(3300));
        if (self.fail) return sd_bus_reply_method_errorf(message, SD_BUS_ERROR_FAILED, "Fixture failure");
        if (navigation) return sd_bus_reply_method_return(message, "b", !self.rejectPage);
        return sd_bus_reply_method_return(message, "");
    }
    void start() {
        static const sd_bus_vtable vtable[] = {
            SD_BUS_VTABLE_START(0),
            SD_BUS_METHOD("Show", "", "", show, SD_BUS_VTABLE_UNPRIVILEGED),
            SD_BUS_METHOD("ShowPage", "s", "b", show, SD_BUS_VTABLE_UNPRIVILEGED),
            SD_BUS_VTABLE_END
        };
        require(sd_bus_open_user(&bus) >= 0, "Cannot connect fixture bus");
        require(sd_bus_add_object_vtable(bus, &slot, "/org/openkey/Settings", "org.openkey.Settings1", vtable, this) >= 0,
                "Cannot export fixture UI");
        require(sd_bus_request_name(bus, "org.openkey.Settings", 0) >= 0, "Cannot own fixture UI name");
        worker = std::thread([this] {
            while (!stop) {
                const int result = sd_bus_process(bus, nullptr);
                if (result < 0) return;
                if (!result) sd_bus_wait(bus, 10000);
            }
        });
    }
};
}
int main() {
    using openkey::ActionResult;
    require(openkey::openOpenKeySettings() == ActionResult::SettingsUnavailable, "Missing UI must report unavailable");
    Mock mock;
    mock.start();
    require(openkey::openOpenKeySettings() == ActionResult::SettingsRequested && mock.requests == 1 && mock.autoStart,
            "Explicit request must call the fixed Show activation method");
    int page = 0;
    for (const auto target : {openkey::SettingsPage::Macro, openkey::SettingsPage::Convert, openkey::SettingsPage::About}) {
        require(openkey::openOpenKeySettings(target) == ActionResult::SettingsRequested && mock.page == ++page,
                "Page request must send its exact allowlisted key");
    }
    const int beforeInvalid = mock.requests;
    require(openkey::openOpenKeySettings(static_cast<openkey::SettingsPage>(99)) == ActionResult::SettingsUnconfirmed &&
            mock.requests == beforeInvalid, "Invalid page must not dispatch");
    mock.noPages = true;
    require(openkey::openOpenKeySettings(openkey::SettingsPage::Macro) == ActionResult::SettingsUnconfirmed,
            "Old UI must not claim navigation succeeded");
    mock.noPages = false;
    mock.rejectPage = true;
    require(openkey::openOpenKeySettings(openkey::SettingsPage::Convert) == ActionResult::SettingsUnconfirmed,
            "Rejected page must not claim navigation succeeded");
    mock.rejectPage = false;
    mock.fail = true;
    const int beforeFailure = mock.requests;
    require(openkey::openOpenKeySettings() == ActionResult::SettingsUnconfirmed && mock.requests == beforeFailure + 1,
            "Error must not claim the UI is shown");
    mock.fail = false;
    mock.slow = true;
    openkey::FcitxActions actions;
    const int beforeSlow = mock.requests;
    const auto started = std::chrono::steady_clock::now();
    require(actions.requestSettings(openkey::SettingsPage::About) && !actions.requestSettings() && !actions.requestConfiguration(),
            "UI and Fcitx commands must share a single in-flight slot");
    require(std::chrono::steady_clock::now() - started < std::chrono::milliseconds(100), "UI request blocks the tray");
    while (actions.result() == ActionResult::Busy) {
        actions.tick();
        require(std::chrono::steady_clock::now() - started < std::chrono::seconds(5), "Unbounded UI request");
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    require(actions.result() == ActionResult::SettingsUnconfirmed && mock.requests == beforeSlow + 1, "Timeout must not retry ShowPage");
    std::cout << "Settings activation endpoint, error, timeout and nonblocking worker passed\n";
}
