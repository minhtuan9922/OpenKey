#include "Composition.h"
#include "DirectInput.h"
#include <fcitx-config/enum.h>
#include <fcitx-config/iniparser.h>
#include <fcitx-utils/event.h>
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
FCITX_CONFIG_ENUM(InputStyle, Preedit, Direct);
FCITX_CONFIGURATION(InputConfig,
    fcitx::Option<InputStyle> style{this, "InputStyle", "Chế độ gõ / Input style", InputStyle::Preedit};
);
struct QueuedKey { fcitx::Key key; bool release; int time; };
bool printable(uint32_t value) {
    return (value >= 32 && value < 127) ||
        (value >= 160 && fcitx::utf8::UCS4IsValid(value));
}
void popCharacter(std::string& text) {
    auto value = openkey::codepoints(text);
    if (!value.empty()) value.pop_back();
    text = openkey::utf8(value);
}
struct State : fcitx::InputContextProperty {
    openkey::Composition composition;
    openkey::DirectInput tracker;
    bool direct = false;
    bool bypass = false;
    bool emitting = false;
    bool active = false;
    std::string recovery;
    std::deque<QueuedKey> queue;
    std::unique_ptr<fcitx::EventSourceTime> timeout;
};

class OpenKeyAddon : public fcitx::InputMethodEngine {
public:
    explicit OpenKeyAddon(fcitx::Instance* instance) : instance_(instance) {
        instance->inputContextManager().registerProperty("openkeyState", &factory_);
        fcitx::readAsIni(config_, "conf/openkey.conf");
        surroundingWatcher_ = instance->watchEvent(fcitx::EventType::InputContextSurroundingTextUpdated,
            fcitx::EventWatcherPhase::InputMethod, [this](fcitx::Event& event) {
                auto* ic = static_cast<fcitx::InputContextEvent&>(event).inputContext();
                auto& state = *ic->propertyFor(&factory_);
                if (state.direct && !state.emitting) reconcile(ic);
            });
    }

    const fcitx::Configuration* getConfig() const override { return &config_; }
    void setConfig(const fcitx::RawConfig& raw) override {
        const auto previous = *config_.style;
        config_.load(raw, true);
        if (*config_.style != previous) finishContexts();
        if (!fcitx::safeSaveAsIni(config_, "conf/openkey.conf"))
            FCITX_WARN() << "OpenKey could not save input style";
    }
    void reloadConfig() override {
        const auto previous = *config_.style;
        fcitx::readAsIni(config_, "conf/openkey.conf");
        if (*config_.style != previous) finishContexts();
    }

    void activate(const fcitx::InputMethodEntry& entry, fcitx::InputContextEvent& event) override {
        auto* state = event.inputContext()->propertyFor(&factory_);
        state->active = true;
        clearDirect(*state);
        state->composition = openkey::Composition(entry.uniqueName() == "openkey-vni" ? vVNI : vTelex);
        update(event.inputContext());
    }

    void keyEvent(const fcitx::InputMethodEntry&, fcitx::KeyEvent& event) override {
        auto* ic = event.inputContext();
        auto& state = *ic->propertyFor(&factory_);
        auto& composition = state.composition;
        if (ic->capabilityFlags() & fcitx::CapabilityFlag::PasswordOrSensitive) {
            clearDirect(state);
            state.recovery.clear();
            composition.clear();
            update(ic);
            return;
        }
        if (!state.recovery.empty()) {
            if (event.isRelease() || event.key().isModifier()) return;
            const auto key = event.key();
            const auto modifiers = fcitx::KeyStates{fcitx::KeyState::Ctrl, fcitx::KeyState::Alt,
                fcitx::KeyState::Super, fcitx::KeyState::Super2, fcitx::KeyState::Meta};
            if (key.states() & modifiers) return;
            if (key.sym() == FcitxKey_Escape) state.recovery.clear();
            else if (key.sym() == FcitxKey_Return || key.sym() == FcitxKey_KP_Enter) {
                // Even explicit recovery is insertion only: require a valid
                // collapsed selection, never replace highlighted content.
                if (!(ic->capabilityFlags() & fcitx::CapabilityFlag::SurroundingText) ||
                    !openkey::Snapshot::read(ic->surroundingText())) {
                    update(ic);
                    event.filterAndAccept();
                    return;
                }
                const auto recovered = std::move(state.recovery);
                state.recovery.clear();
                ic->commitString(recovered); // User explicitly confirms the new destination.
            } else if (key.sym() == FcitxKey_BackSpace) popCharacter(state.recovery);
            else {
                const auto ch = fcitx::Key::keySymToUnicode(key.sym());
                if (printable(ch) && openkey::codepoints(state.recovery).size() < 256)
                    state.recovery += fcitx::utf8::UCS4ToUTF8(ch);
                else return;
            }
            update(ic);
            event.filterAndAccept();
            return;
        }
        if (state.direct && state.tracker.pending()) {
            // Preserve original event order until the previous edit is ACKed.
            if (state.queue.size() >= 64) abandon(ic);
            else {
                state.queue.push_back({event.rawKey(), event.isRelease(), event.time()});
                event.filterAndAccept();
                return;
            }
        }
        if (event.isRelease()) return;
        const auto key = event.key();
        // Modifier keys alone must not commit (e.g. pressing Shift for capitals).
        if (key.isModifier()) return;
        const auto symbol = fcitx::Key::keySymToUnicode(key.sym());
        const bool letterOrDigit = (symbol >= 'a' && symbol <= 'z') ||
            (symbol >= 'A' && symbol <= 'Z') || (symbol >= '0' && symbol <= '9');
        if (state.bypass) {
            if (!letterOrDigit) state.bypass = false;
            return;
        }
        if (!state.direct && composition.empty() && *config_.style == InputStyle::Direct &&
            (ic->capabilityFlags() & fcitx::CapabilityFlag::SurroundingText) &&
            openkey::Snapshot::read(ic->surroundingText())) state.direct = true;
        if (state.direct) {
            if (directKey(ic, key, letterOrDigit, symbol)) event.filterAndAccept();
            return;
        }
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
        if (symbol < 128 && composition.append(static_cast<char>(symbol))) {
            update(ic);
            event.filterAndAccept();
            return;
        }
        // Commit before passing Space/Enter/punctuation/navigation to the app.
        commit(ic);
    }

    void reset(const fcitx::InputMethodEntry&, fcitx::InputContextEvent& event) override {
        // Direct text is already in the application; never erase or re-commit it.
        settle(event.inputContext());
        clearDirect(*event.inputContext()->propertyFor(&factory_));
        event.inputContext()->propertyFor(&factory_)->composition.clear();
        update(event.inputContext());
    }

    void deactivate(const fcitx::InputMethodEntry&, fcitx::InputContextEvent& event) override {
        auto* ic = event.inputContext();
        auto& state = *ic->propertyFor(&factory_);
        state.active = false;
        if (state.direct) {
            settle(ic);
            clearDirect(state);
            state.composition.clear();
            update(ic);
            return;
        }
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
    fcitx::Instance* instance_;
    InputConfig config_;
    fcitx::SimpleInputContextPropertyFactory<State> factory_;
    std::unique_ptr<fcitx::HandlerTableEntry<fcitx::EventHandler>> surroundingWatcher_;

    static void clearDirect(State& state) {
        if (state.timeout) state.timeout->setEnabled(false);
        state.tracker.clear();
        state.queue.clear();
        state.direct = state.bypass = false;
    }

    // Replay original queued keys rather than rewriting any text after loss of
    // sync. This is forwarding client events, not synthesizing Backspace keys.
    void settle(fcitx::InputContext* ic, bool forceRecovery = false) {
        auto& state = *ic->propertyFor(&factory_);
        auto queued = std::move(state.queue);
        state.queue.clear();
        if (ic->capabilityFlags() & fcitx::CapabilityFlag::PasswordOrSensitive) return;
        if (forceRecovery || (state.direct && state.tracker.observe(
                openkey::Snapshot::read(ic->surroundingText())) == openkey::DirectInput::Observation::Changed)) {
            // Original keys no longer have a safe destination. Do not replay
            // them over a new selection/cursor: retain printable text for an
            // explicit Enter/Escape choice instead of silently losing it.
            for (const auto& item : queued) {
                const auto modifiers = fcitx::KeyStates{fcitx::KeyState::Ctrl, fcitx::KeyState::Alt,
                    fcitx::KeyState::Super, fcitx::KeyState::Super2, fcitx::KeyState::Meta};
                if (item.release || (item.key.states() & modifiers)) continue;
                const auto ch = fcitx::Key::keySymToUnicode(item.key.sym());
                if (item.key.sym() == FcitxKey_BackSpace && !state.recovery.empty())
                    popCharacter(state.recovery);
                else if (printable(ch)) state.recovery += fcitx::utf8::UCS4ToUTF8(ch);
            }
            return;
        }
        const bool emitting = state.emitting;
        state.emitting = true;
        for (const auto& item : queued) ic->forwardKey(item.key, item.release, item.time);
        state.emitting = emitting;
    }

    void abandon(fcitx::InputContext* ic) {
        auto& state = *ic->propertyFor(&factory_);
        settle(ic);
        clearDirect(state);
        state.composition.clear();
        state.bypass = true; // Do not interpret an untracked suffix as a new word.
        update(ic);
    }

    void finishContexts() {
        instance_->inputContextManager().foreach([this](fcitx::InputContext* ic) {
            auto& state = *ic->propertyFor(&factory_);
            if (!state.active) return true;
            if (state.composition.empty() && !state.direct && !state.bypass && state.queue.empty())
                return true; // Never clear the panel owned by another input method.
            if (state.direct) {
                settle(ic, state.bypass);
                state.composition.clear();
            } else commit(ic);
            clearDirect(state);
            update(ic);
            return true;
        });
    }

    void reconcile(fcitx::InputContext* ic) {
        auto& state = *ic->propertyFor(&factory_);
        const auto status = state.tracker.observe(openkey::Snapshot::read(ic->surroundingText()));
        if (status == openkey::DirectInput::Observation::Changed) { abandon(ic); return; }
        if (status != openkey::DirectInput::Observation::Ready) return;
        if (state.timeout) state.timeout->setEnabled(false);
        auto queued = std::move(state.queue);
        state.queue.clear();
        while (!queued.empty() && !state.tracker.pending()) {
            auto item = queued.front();
            queued.pop_front();
            if (item.release || item.key.isModifier()) {
                ic->forwardKey(item.key, item.release, item.time);
                continue;
            }
            // Route queued events through exactly the same input-method logic.
            fcitx::KeyEvent event(ic, item.key, false, item.time);
            fcitx::InputMethodEntry unused("openkey", "OpenKey", "vi", "openkey");
            keyEvent(unused, event);
            if (!event.accepted()) ic->forwardKey(item.key, false, item.time);
            if (!state.direct) {
                // A boundary or failed validation invalidates the context for
                // later queued events. Forward them after the boundary, not
                // before it and never silently discard them.
                state.queue = std::move(queued);
                settle(ic, state.bypass);
                if (!state.recovery.empty()) update(ic);
                return;
            }
        }
        state.queue.insert(state.queue.end(), queued.begin(), queued.end());
    }

    bool directKey(fcitx::InputContext* ic, const fcitx::Key& key, bool textKey, uint32_t symbol) {
        auto& state = *ic->propertyFor(&factory_);
        const auto modifiers = fcitx::KeyStates{fcitx::KeyState::Ctrl, fcitx::KeyState::Alt,
            fcitx::KeyState::Super, fcitx::KeyState::Super2, fcitx::KeyState::Meta};
        const bool delimiter = !textKey && printable(symbol);
        if ((!textKey && !delimiter && key.sym() != FcitxKey_BackSpace) || (key.states() & modifiers)) {
            clearDirect(state);
            state.composition.clear();
            return false;
        }
        if (!(ic->capabilityFlags() & fcitx::CapabilityFlag::SurroundingText) ||
            state.tracker.observe(openkey::Snapshot::read(ic->surroundingText())) !=
                openkey::DirectInput::Observation::Ready) {
            abandon(ic);
            return false;
        }
        if (state.composition.full()) {
            state.composition.clear();
            state.tracker.clear();
            state.tracker.observe(openkey::Snapshot::read(ic->surroundingText()));
        }
        auto candidate = state.composition;
        const bool handled = delimiter || (textKey ? candidate.append(static_cast<char>(symbol)) : candidate.backspace());
        if (!handled) { clearDirect(state); return false; }
        const auto edit = state.tracker.prepare(state.composition.text(),
            delimiter ? state.composition.text() + fcitx::utf8::UCS4ToUTF8(symbol) : candidate.text());
        if (!edit) { abandon(ic); return false; }
        if (delimiter) candidate.clear();
        state.composition = std::move(candidate);
        // Set pending before emitting: some frontends synchronously update
        // surrounding text from commit/delete callbacks.
        state.emitting = true;
        if (edit->removed) ic->deleteSurroundingText(-static_cast<int>(edit->removed), edit->removed);
        if (!edit->inserted.empty()) ic->commitString(edit->inserted);
        state.emitting = false;
        if (state.tracker.pending()) {
            if (!state.timeout) {
                state.timeout = instance_->eventLoop().addTimeEvent(CLOCK_MONOTONIC,
                    fcitx::now(CLOCK_MONOTONIC) + 250000, 0,
                    [this, weak = ic->watch()](fcitx::EventSourceTime* source, uint64_t) {
                        source->setEnabled(false);
                        if (auto* context = weak.get()) abandon(context);
                        return true;
                    });
            }
            state.timeout->setTime(fcitx::now(CLOCK_MONOTONIC) + 250000);
            state.timeout->setOneShot();
        }
        reconcile(ic);
        return true;
    }

    void update(fcitx::InputContext* ic) {
        const auto& state = *ic->propertyFor(&factory_);
        const auto value = state.direct ? std::string{} : state.composition.text();
        fcitx::Text text(value);
        text.setCursor(static_cast<int>(value.size()));
        ic->inputPanel().reset();
        if (!state.recovery.empty()) {
            ic->inputPanel().setAuxUp(fcitx::Text("Phím chưa nhập / Pending: " + state.recovery));
            ic->inputPanel().setAuxDown(fcitx::Text("Bỏ chọn vùng / place caret; Enter: chèn / insert; Escape: hủy / cancel"));
        }
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
