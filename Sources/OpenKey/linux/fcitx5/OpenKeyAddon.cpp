#include "Composition.h"
#include <fcitx-utils/capabilityflags.h>
#include <fcitx-utils/key.h>
#include <fcitx/addonfactory.h>
#include <fcitx/addonmanager.h>
#include <fcitx/inputcontext.h>
#include <fcitx/inputcontextmanager.h>
#include <fcitx/inputcontextproperty.h>
#include <fcitx/inputmethodengine.h>
#include <fcitx/inputpanel.h>
#include <fcitx/instance.h>
#include <fcitx/text.h>

namespace {
struct State : fcitx::InputContextProperty {
    openkey::Composition composition;
};

class OpenKeyAddon : public fcitx::InputMethodEngine {
public:
    explicit OpenKeyAddon(fcitx::Instance* instance) {
        instance->inputContextManager().registerProperty("openkeyState", &factory_);
    }

    void activate(const fcitx::InputMethodEntry& entry, fcitx::InputContextEvent& event) override {
        auto* state = event.inputContext()->propertyFor(&factory_);
        state->composition = openkey::Composition(entry.uniqueName() == "openkey-vni" ? vVNI : vTelex);
        update(event.inputContext());
    }

    void keyEvent(const fcitx::InputMethodEntry&, fcitx::KeyEvent& event) override {
        if (event.isRelease()) return;
        auto* ic = event.inputContext();
        auto& composition = ic->propertyFor(&factory_)->composition;
        if (ic->capabilityFlags() & fcitx::CapabilityFlag::PasswordOrSensitive) {
            composition.clear();
            update(ic);
            return;
        }
        const auto key = event.key();
        // Modifier keys alone must not commit (e.g. pressing Shift for capitals).
        if (key.isModifier()) return;
        if (key.states() & fcitx::KeyStates{fcitx::KeyState::Ctrl, fcitx::KeyState::Alt,
                            fcitx::KeyState::Super, fcitx::KeyState::Super2,
                            fcitx::KeyState::Meta}) {
            commit(ic);
            return;
        }
        if (key.sym() == FcitxKey_Escape && !composition.empty()) {
            composition.clear();
            update(ic);
            event.filterAndAccept();
            return;
        }
        if (composition.full()) commit(ic);
        if (key.sym() == FcitxKey_BackSpace) {
            if (composition.backspace()) {
                update(ic);
                event.filterAndAccept();
            }
            return;
        }
        auto symbol = fcitx::Key::keySymToUnicode(key.sym());
        if (symbol < 128 && composition.append(static_cast<char>(symbol))) {
            update(ic);
            event.filterAndAccept();
            return;
        }
        // Commit before passing Space/Enter/punctuation/navigation to the app.
        commit(ic);
    }

    void reset(const fcitx::InputMethodEntry&, fcitx::InputContextEvent& event) override {
        event.inputContext()->propertyFor(&factory_)->composition.clear();
        update(event.inputContext());
    }

    void deactivate(const fcitx::InputMethodEntry&, fcitx::InputContextEvent& event) override {
        auto* ic = event.inputContext();
        // On focus-out, Fcitx already committed client preedit, or delegated
        // that commit to a ClientUnfocusCommit frontend. Committing again here
        // duplicates the word. Server-side preedit still belongs to us.
        if (event.type() == fcitx::EventType::InputContextFocusOut &&
            (ic->capabilityFlags() & fcitx::CapabilityFlag::Preedit)) {
            ic->propertyFor(&factory_)->composition.clear();
            update(ic);
        } else commit(ic);
    }

private:
    fcitx::SimpleInputContextPropertyFactory<State> factory_;

    void update(fcitx::InputContext* ic) {
        const auto& value = ic->propertyFor(&factory_)->composition.text();
        fcitx::Text text(value);
        text.setCursor(static_cast<int>(value.size()));
        ic->inputPanel().reset();
        if (ic->capabilityFlags() & fcitx::CapabilityFlag::Preedit)
            ic->inputPanel().setClientPreedit(text);
        else ic->inputPanel().setPreedit(text);
        ic->updatePreedit();
        ic->updateUserInterface(fcitx::UserInterfaceComponent::InputPanel);
    }

    void commit(fcitx::InputContext* ic) {
        auto& composition = ic->propertyFor(&factory_)->composition;
        const auto text = composition.text();
        composition.clear();
        update(ic);
        if (!text.empty() &&
            !(ic->capabilityFlags() & fcitx::CapabilityFlag::PasswordOrSensitive))
            ic->commitString(text);
    }
};

class OpenKeyFactory : public fcitx::AddonFactory {
    fcitx::AddonInstance* create(fcitx::AddonManager* manager) override {
        return new OpenKeyAddon(manager->instance());
    }
};
} // namespace

FCITX_ADDON_FACTORY(OpenKeyFactory)
