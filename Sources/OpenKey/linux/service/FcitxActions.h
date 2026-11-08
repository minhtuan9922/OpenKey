#pragma once
#include <future>
#include <functional>

namespace openkey {
enum class InputMethod { Telex, Vni, SimpleTelex1, SimpleTelex2, English };
enum class SettingsPage { General, Macro, Convert, About };
enum class ActionResult { Idle, Busy, Changed, ConfigurationRequested, Unavailable, NotConfigured, GroupChanged, Unconfirmed, Failed,
    SettingsRequested, SettingsUnavailable, SettingsUnconfirmed };
ActionResult switchFcitxInputMethod(InputMethod method);
ActionResult openFcitxConfiguration();
ActionResult openOpenKeySettings(SettingsPage page = SettingsPage::General);
const char* actionLabel(ActionResult result);
// Worker owns its bus. No blocking controller calls on the service event thread.
class FcitxActions {
public:
    bool request(InputMethod method);
    bool requestConfiguration();
    bool requestSettings(SettingsPage page = SettingsPage::General);
    void tick();
    ActionResult result() const { return result_; }
private:
    bool submit(std::function<ActionResult()> task);
    std::future<ActionResult> pending_;
    ActionResult result_ = ActionResult::Idle;
};
}
