#include "FcitxActions.h"
#include <systemd/sd-bus.h>

namespace openkey {
ActionResult openOpenKeySettings(SettingsPage page) {
    const char* key = nullptr;
    switch (page) {
    case SettingsPage::General: break;
    case SettingsPage::Macro: key = "macro"; break;
    case SettingsPage::Convert: key = "convert"; break;
    case SettingsPage::About: key = "about"; break;
    default: return ActionResult::SettingsUnconfirmed;
    }
    sd_bus* bus = nullptr;
    if (sd_bus_open_user(&bus) < 0) { sd_bus_unref(bus); return ActionResult::SettingsUnavailable; }
    sd_bus_message *request = nullptr, *reply = nullptr;
    sd_bus_error error = SD_BUS_ERROR_NULL;
    int result = sd_bus_message_new_method_call(bus, &request, "org.openkey.Settings",
        "/org/openkey/Settings", "org.openkey.Settings1", key ? "ShowPage" : "Show");
    // Activation is intentional only for this explicit menu command. No shell,
    // PATH lookup, Fcitx activation, config writes or automatic retry.
    if (result >= 0) result = sd_bus_message_set_auto_start(request, 1);
    if (result >= 0 && key) result = sd_bus_message_append(request, "s", key);
    if (result >= 0) result = sd_bus_call(bus, request, 3000000, &error, &reply);
    ActionResult outcome = ActionResult::SettingsUnconfirmed;
    int accepted = 0;
    if (result >= 0 && ((!key && sd_bus_message_has_signature(reply, "") > 0) ||
        (key && sd_bus_message_has_signature(reply, "b") > 0 &&
         sd_bus_message_read(reply, "b", &accepted) > 0 && accepted))) outcome = ActionResult::SettingsRequested;
    else if (sd_bus_error_has_name(&error, SD_BUS_ERROR_SERVICE_UNKNOWN) ||
             sd_bus_error_has_name(&error, "org.freedesktop.DBus.Error.Spawn.ExecFailed") ||
             sd_bus_error_has_name(&error, SD_BUS_ERROR_NAME_HAS_NO_OWNER)) outcome = ActionResult::SettingsUnavailable;
    sd_bus_error_free(&error);
    sd_bus_message_unref(request);
    sd_bus_message_unref(reply);
    sd_bus_unref(bus);
    return outcome;
}
}
