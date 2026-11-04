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
    fcitx::Option<bool> diagnostics{this, "DirectDiagnostics", "Direct diagnostics (no text content)", false};
    fcitx::Option<bool> quickTelex{this, "QuickTelex", "Gõ nhanh Telex / Quick Telex", false};
    fcitx::Option<bool> checkSpelling{this, "CheckSpelling", "Kiểm tra chính tả / Check spelling", true};
    fcitx::Option<bool> useModernOrthography{this, "UseModernOrthography", "Đặt dấu kiểu mới / Modern tone placement", true};
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
    std::optional<openkey::Composition> previousWord;
    std::string trailingSpaces;
    openkey::DirectInput tracker;
    bool direct = false;
    bool bypass = false;
    bool emitting = false;
    bool active = false;
    bool pendingNotified = false;
    std::string recovery;
    std::string lastReason;
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
                if (!state.active) return;
                if (state.direct && state.tracker.pending()) state.pendingNotified = true;
                if (state.emitting) return;
                if (state.direct) reconcile(ic, true);
                else if (state.bypass && state.recovery.empty() && state.queue.empty() &&
                         openkey::Snapshot::read(ic->surroundingText(), true)) {
                    // A reused browser context must not require an EN-VI
                    // toggle. New input will own only a fresh suffix.
                    clearDirect(state);
                    state.composition.clear();
                    diagnose(ic, "context-ready");
                    update(ic);
                }
            });
    }

    const fcitx::Configuration* getConfig() const override { return &config_; }
    void setConfig(const fcitx::RawConfig& raw) override {
        const auto previous = config_;
        config_.load(raw, true);
        if (compositionConfigChanged(previous)) finishContexts();
        // Keep future/third-party keys when applying a partial settings update.
        fcitx::RawConfig saved;
        fcitx::readAsIni(saved, "conf/openkey.conf");
        raw.visitSubItems([&saved](const fcitx::RawConfig& item, const std::string& path) {
            saved.setValueByPath(path, item.value());
            return true;
        }, "", true);
        config_.save(saved);
        if (!fcitx::safeSaveAsIni(saved, "conf/openkey.conf"))
            FCITX_WARN() << "OpenKey could not save configuration";
    }
    void reloadConfig() override {
        const auto previous = config_;
        fcitx::readAsIni(config_, "conf/openkey.conf");
        if (compositionConfigChanged(previous)) finishContexts();
    }

    void activate(const fcitx::InputMethodEntry& entry, fcitx::InputContextEvent& event) override {
        auto* state = event.inputContext()->propertyFor(&factory_);
        state->active = true;
        state->lastReason.clear();
        clearDirect(*state);
        state->composition = openkey::Composition(entry.uniqueName() == "openkey-vni" ? vVNI : vTelex,
                                                  compositionOptions());
        update(event.inputContext());
    }

    void keyEvent(const fcitx::InputMethodEntry&, fcitx::KeyEvent& event) override {
        auto* ic = event.inputContext();
        auto& state = *ic->propertyFor(&factory_);
        auto& composition = state.composition;
        if (ic->capabilityFlags() & fcitx::CapabilityFlag::PasswordOrSensitive) {
            diagnose(ic, "bypass:sensitive-field");
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
            if (state.queue.size() >= 64) abandon(ic, "queue-overflow");
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
            openkey::Snapshot::read(ic->surroundingText(), true)) state.direct = true;
        if (state.direct) {
            if (directKey(ic, key, letterOrDigit, symbol)) {
                event.filterAndAccept();
                return;
            }
            if (state.direct || state.bypass ||
                ((ic->capabilityFlags() & fcitx::CapabilityFlag::SurroundingText) &&
                 openkey::Snapshot::read(ic->surroundingText(), true))) return;
            // If the client withdrew capability/context while idle, process
            // this same key in Preedit, rather than lose the first keystroke.
        }
        if (*config_.style == InputStyle::Direct)
            diagnose(ic, !composition.empty() ? "fallback:preedit-word-in-progress" :
                ((ic->capabilityFlags() & fcitx::CapabilityFlag::SurroundingText)
                    ? "fallback:invalid-surrounding-text" : "fallback:no-surrounding-capability"));
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

    openkey::Composition::Options compositionOptions() const {
        return {*config_.checkSpelling, *config_.useModernOrthography, *config_.quickTelex};
    }
    bool compositionConfigChanged(const InputConfig& previous) const {
        return *config_.style != *previous.style ||
            *config_.checkSpelling != *previous.checkSpelling ||
            *config_.useModernOrthography != *previous.useModernOrthography ||
            *config_.quickTelex != *previous.quickTelex;
    }

    void diagnose(fcitx::InputContext* ic, const char* reason, const char* cause = "none") {
        if (!*config_.diagnostics) return;
        auto& state = *ic->propertyFor(&factory_);
        const auto snapshot = openkey::Snapshot::read(ic->surroundingText(), true);
        const auto comparison = state.tracker.compare(snapshot);
        const std::string signature = std::string(reason) + ":" + cause + ":" + comparison.reference +
            std::to_string(comparison.comparable) + std::to_string(comparison.textChanged) +
            std::to_string(comparison.cursorChanged) + std::to_string(comparison.anchorChanged);
        if (state.lastReason == signature) return;
        state.lastReason = signature;
        // Only identifiers, enum classifications and boolean/capability state. Never log text,
        // keystrokes, URLs, selections, offsets or recovery contents.
        FCITX_INFO() << "OpenKey Direct: reason=" << reason << " app=" << ic->program()
                     << " frontend=" << ic->frontend() << " capabilities="
                     << ic->capabilityFlags().toInteger() << " cause=" << cause
                     << " snapshot=" << state.tracker.snapshotStatus(snapshot)
                     << " pending=" << state.tracker.pending()
                     << " notified=" << state.pendingNotified
                     << " reference=" << comparison.reference << " comparable=" << comparison.comparable
                     << " text_changed=" << comparison.textChanged
                     << " cursor_changed=" << comparison.cursorChanged
                     << " anchor_changed=" << comparison.anchorChanged
                     << " ack_requires_update=" << state.tracker.requiresFreshUpdate();
    }

    static void clearDirect(State& state) {
        if (state.timeout) state.timeout->setEnabled(false);
        state.tracker.clear();
        state.queue.clear();
        state.direct = state.bypass = false;
        state.pendingNotified = false;
        state.previousWord.reset();
        state.trailingSpaces.clear();
    }

    // Replay original queued keys rather than rewriting any text after loss of
    // sync. This is forwarding client events, not synthesizing Backspace keys.
    void settle(fcitx::InputContext* ic, bool forceRecovery = false) {
        auto& state = *ic->propertyFor(&factory_);
        auto queued = std::move(state.queue);
        state.queue.clear();
        if (ic->capabilityFlags() & fcitx::CapabilityFlag::PasswordOrSensitive) return;
        if (forceRecovery || (state.direct && state.tracker.observe(
                openkey::Snapshot::read(ic->surroundingText(), true)) == openkey::DirectInput::Observation::Changed)) {
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

    void abandon(fcitx::InputContext* ic, const char* cause) {
        auto& state = *ic->propertyFor(&factory_);
        // Inspect before settle()/clearDirect() change the pending state.
        diagnose(ic, "edit-aborted", cause);
        settle(ic);
        clearDirect(state);
        state.composition.clear();
        state.bypass = true; // Do not interpret an untracked suffix as a new word.
        diagnose(ic, state.recovery.empty() ? "bypass:unconfirmed-edit" : "recovery:context-changed");
        update(ic);
    }

    void finishContexts() {
        instance_->inputContextManager().foreach([this](fcitx::InputContext* ic) {
            auto& state = *ic->propertyFor(&factory_);
            if (!state.active) return true;
            if (state.composition.empty() && !state.direct && !state.bypass && state.queue.empty()) {
                state.composition.setOptions(compositionOptions());
                return true; // Never clear the panel owned by another input method.
            }
            if (state.direct) {
                settle(ic, state.bypass);
                state.composition.clear();
            } else commit(ic);
            clearDirect(state);
            state.composition.setOptions(compositionOptions());
            update(ic);
            return true;
        });
    }

    void reconcile(fcitx::InputContext* ic, bool fromUpdate = false) {
        auto& state = *ic->propertyFor(&factory_);
        const bool pending = state.tracker.pending();
        const auto snapshot = openkey::Snapshot::read(ic->surroundingText(), true);
        const auto status = state.tracker.observe(snapshot, fromUpdate);
        if (status == openkey::DirectInput::Observation::Changed) {
            if (!pending && state.queue.empty()) {
                diagnose(ic, "context-invalidated", snapshot ? "surrounding-mismatch" : "surrounding-invalid");
                // Focus/selection/completion changes while idle end ownership
                // of the old suffix, not Vietnamese input for the whole field.
                clearDirect(state);
                state.composition.clear();
                diagnose(ic, "context-reset");
                update(ic);
            } else abandon(ic, snapshot ? "surrounding-mismatch" : "surrounding-invalid");
            return;
        }
        if (status != openkey::DirectInput::Observation::Ready) {
            if (!pending) diagnose(ic, "context-wait", "late-known-echo");
            return;
        }
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
        const auto snapshot = openkey::Snapshot::read(ic->surroundingText(), true);
        if (!(ic->capabilityFlags() & fcitx::CapabilityFlag::SurroundingText) ||
            state.tracker.observe(snapshot) != openkey::DirectInput::Observation::Ready) {
            if (!state.tracker.pending() && state.queue.empty()) {
                diagnose(ic, "context-invalidated", !(ic->capabilityFlags() & fcitx::CapabilityFlag::SurroundingText)
                    ? "capability-withdrawn" : (snapshot ? "key-snapshot-not-current" : "surrounding-invalid"));
                clearDirect(state);
                state.composition.clear();
                if (!(ic->capabilityFlags() & fcitx::CapabilityFlag::SurroundingText) || !snapshot)
                    return false;
                state.direct = true;
                state.tracker.observe(snapshot);
                diagnose(ic, "context-reset");
            } else {
                abandon(ic, !(ic->capabilityFlags() & fcitx::CapabilityFlag::SurroundingText)
                    ? "capability-withdrawn" : (snapshot ? "surrounding-mismatch" : "surrounding-invalid"));
                return false;
            }
        }
        diagnose(ic, snapshot && snapshot->cursor != snapshot->anchor ? "direct:selection-start" : "direct:ready");
        if (state.composition.full()) {
            state.composition.clear();
            state.previousWord.reset();
            state.trailingSpaces.clear();
            state.tracker.clear();
            state.tracker.observe(openkey::Snapshot::read(ic->surroundingText(), true));
        }
        auto candidate = state.composition;
        const bool reopen = key.sym() == FcitxKey_BackSpace && candidate.empty() &&
            state.previousWord && !state.trailingSpaces.empty();
        std::string before = state.composition.text();
        std::string after;
        bool handled;
        if (reopen) {
            // Validate the ENTIRE retained word plus its spaces, not just one
            // blank or a matching word somewhere else in the document.
            before = state.previousWord->text() + state.trailingSpaces;
            after = before.substr(0, before.size() - 1); // ASCII Space only.
            if (state.trailingSpaces.size() == 1) candidate = *state.previousWord;
            handled = true;
        } else {
            handled = delimiter || (textKey ? candidate.append(static_cast<char>(symbol)) : candidate.backspace());
            after = delimiter ? before + fcitx::utf8::UCS4ToUTF8(symbol) : candidate.text();
        }
        if (!handled) { clearDirect(state); return false; }
        const auto edit = state.tracker.prepare(before, after);
        if (!edit) { abandon(ic, "prepare-rejected"); return false; }
        if (reopen) {
            state.trailingSpaces.pop_back();
            if (state.trailingSpaces.empty()) {
                state.previousWord.reset();
                diagnose(ic, "boundary-restored");
            }
        } else if (delimiter) {
            if (symbol == ' ' && !state.composition.empty()) {
                state.previousWord = state.composition;
                state.trailingSpaces = " ";
            } else if (symbol == ' ' && state.previousWord && state.trailingSpaces.size() < 32) {
                state.trailingSpaces.push_back(' ');
            } else {
                state.previousWord.reset();
                state.trailingSpaces.clear();
            }
            candidate.clear();
        } else {
            // Starting the next word or deleting inside it ends this bounded
            // one-word boundary history. Never scan/reconstruct arbitrary text.
            state.previousWord.reset();
            state.trailingSpaces.clear();
        }
        state.composition = std::move(candidate);
        // Set pending before emitting: some frontends synchronously update
        // surrounding text from commit/delete callbacks.
        state.pendingNotified = false;
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
                        if (auto* context = weak.get()) abandon(context, "ack-timeout");
                        return true;
                    });
            }
            state.timeout->setTime(fcitx::now(CLOCK_MONOTONIC) + 250000);
            state.timeout->setOneShot();
        }
        // Synchronous frontend notifications are observed by the watcher even
        // while emitting; polling the cache without such an event is not ACK.
        reconcile(ic, state.pendingNotified);
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
