#pragma once
#include "FcitxStatus.h"
#include "FcitxActions.h"
#include "SessionInfo.h"
#include <systemd/sd-bus.h>
#include <chrono>
#include <string>

namespace openkey {
// Optional SNI exporter. All bus work runs on the service event thread.
class TrayItem {
public:
    explicit TrayItem(bool enabled, DesktopSession session = currentDesktopSession())
        : enabled_(enabled), session_(session) {}
    ~TrayItem();
    TrayItem(const TrayItem&) = delete;
    TrayItem& operator=(const TrayItem&) = delete;
    void tick(InputStatus status, HotkeyStatus hotkey = HotkeyStatus::Disabled);
    void requestConfiguration() { if (!quitRequested_) actions_.requestConfiguration(); }
    const char* registrationState() const;
    bool quitRequested() const { return quitRequested_; }
    static const char* label(InputStatus status);
private:
    using Clock = std::chrono::steady_clock;
    void disconnect();
    bool connect();
    void registerItem();
    static int property(sd_bus*, const char*, const char*, const char*, sd_bus_message*, void*, sd_bus_error*);
    static int menuProperty(sd_bus*, const char*, const char*, const char*, sd_bus_message*, void*, sd_bus_error*);
    static int noAction(sd_bus_message*, void*, sd_bus_error*);
    static int menuEvent(sd_bus_message*, void*, sd_bus_error*);
    static int getLayout(sd_bus_message*, void*, sd_bus_error*);
    static int groupProperties(sd_bus_message*, void*, sd_bus_error*);
    static int aboutToShow(sd_bus_message*, void*, sd_bus_error*);
    static int registered(sd_bus_message*, void*, sd_bus_error*);
    static int ownerChanged(sd_bus_message*, void*, sd_bus_error*);
    bool enabled_, registered_ = false;
    DesktopSession session_;
    HotkeyStatus hotkey_ = HotkeyStatus::Disabled;
    bool quitRequested_ = false;
    sd_bus* bus_ = nullptr;
    sd_bus_slot *item_ = nullptr, *menu_ = nullptr, *watch_ = nullptr, *registration_ = nullptr;
    InputStatus status_ = InputStatus::Unknown;
    FcitxActions actions_;
    ActionResult actionResult_ = ActionResult::Idle;
    uint32_t revision_ = 1;
    Clock::time_point retry_{};
    std::string title() const;
};
}
