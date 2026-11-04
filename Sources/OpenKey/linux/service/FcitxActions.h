#pragma once
#include <future>
#include <functional>

namespace openkey {
enum class InputMethod { Telex, Vni, English };
enum class ActionResult { Idle, Busy, Changed, ConfigurationRequested, Unavailable, NotConfigured, GroupChanged, Unconfirmed, Failed,
    SettingsRequested, SettingsUnavailable, SettingsUnconfirmed };
ActionResult switchFcitxInputMethod(InputMethod method);
ActionResult openFcitxConfiguration();
ActionResult openOpenKeySettings();
const char* actionLabel(ActionResult result);
// Worker owns its bus. No blocking controller calls on the service event thread.
class FcitxActions {
public:
    bool request(InputMethod method);
    bool requestConfiguration();
    bool requestSettings();
    void tick();
    ActionResult result() const { return result_; }
private:
    bool submit(std::function<ActionResult()> task);
    std::future<ActionResult> pending_;
    ActionResult result_ = ActionResult::Idle;
};
}
