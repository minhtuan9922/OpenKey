#include "DirectInput.h"
#include <fcitx-utils/event.h>
#include <fcitx/addonfactory.h>
#include <fcitx/addonmanager.h>
#include <fcitx/inputcontext.h>
#include <fcitx/inputcontextmanager.h>
#include <fcitx/inputmethodengine.h>
#include <fcitx/inputpanel.h>
#include <fcitx/instance.h>
#include <fcitx-config/iniparser.h>
#include <dlfcn.h>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <stdexcept>

namespace {
void require(bool value, const char* label) {
    if (!value) throw std::runtime_error(label);
}
class Editor : public fcitx::InputContext {
public:
    explicit Editor(fcitx::InputContextManager& manager) : InputContext(manager) {
        created();
        setCapabilityFlags(fcitx::CapabilityFlags{fcitx::CapabilityFlag::Preedit,
                                               fcitx::CapabilityFlag::SurroundingText});
        report();
    }
    ~Editor() override { destroy(); }
    const char* frontend() const override { return "openkey-direct-test"; }
    bool delayed = false;
    std::u32string document;
    unsigned cursor = 0, anchor = 0;
    unsigned deletions = 0, commits = 0, nonemptyPreedits = 0;
    struct Notification { std::string text; unsigned cursor, anchor; };
    std::deque<Notification> notifications;
    std::string text() const { return openkey::utf8(document); }
    std::string preedit() { return inputPanel().clientPreedit().toString(); }
    void notify(const Notification& value) {
        surroundingText().setText(value.text, value.cursor, value.anchor);
        updateSurroundingText();
    }
    void report() {
        Notification value{text(), cursor, anchor};
        if (delayed) notifications.push_back(value);
        else notify(value);
    }
    void ackAll() {
        unsigned count = 0;
        while (!notifications.empty()) {
            require(++count < 4096, "ACK processing loop");
            auto value = notifications.front(); notifications.pop_front();
            notify(value);
        }
    }
    void raw(const fcitx::Key& key, bool release = false) {
        if (release || key.isModifier()) return;
        if (key.states() & fcitx::KeyStates{fcitx::KeyState::Ctrl, fcitx::KeyState::Alt,
                                           fcitx::KeyState::Super}) return;
        if (key.sym() == FcitxKey_BackSpace) {
            if (cursor) { document.erase(--cursor, 1); anchor = cursor; report(); }
        } else if (key.sym() == FcitxKey_Left) {
            if (cursor) --cursor;
            anchor = cursor; report();
        } else {
            const auto ch = fcitx::Key::keySymToUnicode(key.sym());
            if (ch >= 32 && ch < 127) {
                const auto start = std::min(cursor, anchor);
                document.replace(start, std::max(cursor, anchor) - start, 1, ch);
                cursor = anchor = start + 1;
                report();
            }
        }
    }
private:
    void commitStringImpl(const std::string& text) override {
        ++commits;
        const auto value = openkey::codepoints(text);
        const auto start = std::min(cursor, anchor);
        document.replace(start, std::max(cursor, anchor) - start, value);
        cursor = anchor = start + static_cast<unsigned>(value.size());
        report();
    }
    void deleteSurroundingTextImpl(int offset, unsigned size) override {
        require(cursor == anchor, "Deletion must never operate on a selection");
        require(offset <= 0 && -offset <= static_cast<int>(cursor) && size <= cursor,
                "Invalid deletion range");
        ++deletions;
        document.erase(static_cast<unsigned>(static_cast<int>(cursor) + offset), size);
        cursor -= size; anchor = cursor;
        report();
    }
    void forwardKeyImpl(const fcitx::ForwardKeyEvent& event) override { raw(event.rawKey(), event.isRelease()); }
    void updatePreeditImpl() override { if (!preedit().empty()) ++nonemptyPreedits; }
};
void style(fcitx::InputMethodEngine& addon, const char* value) {
    fcitx::RawConfig config;
    config.setValueByPath("InputStyle", value);
    addon.setConfig(config);
}
void activate(fcitx::InputMethodEngine& addon, const fcitx::InputMethodEntry& entry, Editor& editor) {
    fcitx::InputContextEvent event(&editor, fcitx::EventType::InputContextFocusIn);
    addon.activate(entry, event);
}
void press(fcitx::InputMethodEngine& addon, const fcitx::InputMethodEntry& entry,
           Editor& editor, fcitx::Key key, bool release = false) {
    fcitx::KeyEvent event(&editor, key, release);
    addon.keyEvent(entry, event);
    if (!event.accepted()) editor.raw(key, release);
}
void type(fcitx::InputMethodEngine& addon, const fcitx::InputMethodEntry& entry,
          Editor& editor, const std::string& value) {
    for (unsigned char ch : value)
        press(addon, entry, editor, fcitx::Key(fcitx::Key::keySymFromUnicode(ch)));
}
} // namespace

int main(int argc, char** args) {
    require(argc == 2, "Module path required");
    // Also isolate manual/debugger runs, not just the CTest invocation.
    const auto stateDirectory = std::filesystem::absolute(args[1]).parent_path() / "direct-test-state";
    for (const auto& [variable, directory] : {
            std::pair{"XDG_CONFIG_HOME", "config"}, std::pair{"XDG_DATA_HOME", "data"},
            std::pair{"XDG_CACHE_HOME", "cache"}}) {
        const auto path = (stateDirectory / directory).string();
        require(setenv(variable, path.c_str(), 1) == 0, "Cannot isolate test configuration");
    }
    const auto close = [](void* handle) { dlclose(handle); };
    std::unique_ptr<void, decltype(close)> module(dlopen(args[1], RTLD_NOW | RTLD_LOCAL), close);
    if (!module) throw std::runtime_error(dlerror());
    auto factory = reinterpret_cast<fcitx::AddonFactory* (*)()>(dlsym(module.get(), "fcitx_addon_factory_instance"));
    require(factory, "Factory missing");
    char name[] = "openkey-direct-test";
    char* argv[] = {name, nullptr};
    fcitx::Instance instance(1, argv);
    std::unique_ptr<fcitx::AddonInstance> owned(factory()->create(&instance.addonManager()));
    auto* addon = dynamic_cast<fcitx::InputMethodEngine*>(owned.get());
    require(addon, "Engine missing");
    fcitx::InputMethodEntry telex("openkey-telex", "Telex", "vi", "openkey");
    fcitx::InputMethodEntry vni("openkey-vni", "VNI", "vi", "openkey");
    style(*addon, "Direct");
    fcitx::RawConfig savedSettings;
    fcitx::readAsIni(savedSettings, "conf/openkey.conf");
    const auto* savedStyle = savedSettings.valueByPath("InputStyle");
    require(savedStyle && *savedStyle == "Direct", "Input style was not persisted in isolated config");

    // Browser address bars select their old value on focus. The first typed
    // character should replace that selection without entering preedit.
    Editor address(instance.inputContextManager());
    address.document = U"https://example.invalid";
    address.cursor = static_cast<unsigned>(address.document.size()); address.anchor = 0;
    address.report(); activate(*addon, telex, address);
    type(*addon, telex, address, "tieengs");
    require(address.text() == "tiếng" && address.preedit().empty() && address.nonemptyPreedits == 0,
            "Initially selected address bar required EN-VI toggle");

    // Browsers can reuse an input context for a different editor without a
    // matching IM activation. An idle surrounding-text change starts a fresh
    // word; it must not lock the next field in bypass until EN-VI is toggled.
    address.document.clear(); address.cursor = address.anchor = 0;
    address.report();
    type(*addon, telex, address, "vieetj");
    require(address.text() == "việt" && address.preedit().empty(),
            "Idle context change left Direct stuck in bypass");

    Editor selectedBurst(instance.inputContextManager());
    selectedBurst.document = U"old address";
    selectedBurst.cursor = 0; selectedBurst.anchor = 11; // reversed selection
    selectedBurst.report(); activate(*addon, vni, selectedBurst);
    selectedBurst.delayed = true;
    type(*addon, vni, selectedBurst, "vie6t5");
    require(selectedBurst.text() == "v" && selectedBurst.deletions == 0,
            "Initial selection must use normal commit, not surrounding deletion");
    selectedBurst.ackAll();
    require(selectedBurst.text() == "việt" && selectedBurst.nonemptyPreedits == 0,
            "Delayed/reversed initial selection lost Direct mode");

    Editor missingNotification(instance.inputContextManager());
    activate(*addon, telex, missingNotification);
    type(*addon, telex, missingNotification, "a");
    missingNotification.document.clear(); missingNotification.cursor = missingNotification.anchor = 0;
    missingNotification.surroundingText().setText("", 0, 0); // frontend did not dispatch a separate notification
    type(*addon, telex, missingNotification, "tieengs");
    require(missingNotification.text() == "tiếng", "First key after idle context change was lost");

    Editor withdrawn(instance.inputContextManager());
    activate(*addon, telex, withdrawn);
    type(*addon, telex, withdrawn, "a");
    withdrawn.setCapabilityFlags(fcitx::CapabilityFlag::Preedit);
    type(*addon, telex, withdrawn, "tieengs");
    require(withdrawn.text() == "a" && withdrawn.preedit() == "tiếng",
            "Capability withdrawal lost the first fallback key");

    Editor editor(instance.inputContextManager());
    activate(*addon, telex, editor);
    type(*addon, telex, editor, "print");
    require(editor.text() == "print", "Identifiers must be committed before a delimiter");
    type(*addon, telex, editor, " ");
    type(*addon, telex, editor, "tieengs vieetj ");
    require(editor.text() == "print tiếng việt ", "Direct Telex text");
    require(editor.nonemptyPreedits == 0 && editor.preedit().empty(), "Direct must not send composition");
    type(*addon, telex, editor, "dduowngf");
    const auto committed = editor.text();
    fcitx::InputContextEvent focusOut(&editor, fcitx::EventType::InputContextFocusOut);
    addon->deactivate(telex, focusOut);
    require(editor.text() == committed, "Direct focus-out must not duplicate text");
    activate(*addon, vni, editor);
    type(*addon, vni, editor, " VIE6T5");
    require(editor.text() == committed + " VIỆT", "VNI uppercase");
    press(*addon, vni, editor, fcitx::Key(FcitxKey_Escape));
    require(editor.text() == committed + " VIỆT", "Direct Escape must not erase committed text");

    // Unicode code-point offsets must not count emoji as UTF-16 units/bytes.
    Editor emoji(instance.inputContextManager());
    emoji.document = U"😀 e\u0301 "; emoji.cursor = emoji.anchor = static_cast<unsigned>(emoji.document.size());
    emoji.report(); activate(*addon, telex, emoji);
    type(*addon, telex, emoji, "tieengs");
    require(emoji.text() == "😀 é tiếng", "Unicode prefix was damaged");

    Editor burst(instance.inputContextManager());
    activate(*addon, telex, burst);
    burst.delayed = true;
    type(*addon, telex, burst, "tieengs");
    require(burst.text() == "t" && burst.deletions == 0, "Must serialize edits pending ACK");
    // Duplicate old snapshots cannot authorize another deletion.
    burst.notify({"", 0, 0});
    require(burst.text() == "t", "Old snapshot advanced pending keys");
    burst.ackAll();
    require(burst.text() == "tiếng", "Queued burst lost/reordered characters");
    require(burst.nonemptyPreedits == 0, "Delayed Direct unexpectedly used preedit");
    burst.delayed = false;
    press(*addon, telex, burst, fcitx::Key(FcitxKey_BackSpace));
    require(burst.text() == "tiến", "Backspace must remove one visible character");

    Editor boundary(instance.inputContextManager());
    activate(*addon, telex, boundary);
    boundary.delayed = true;
    type(*addon, telex, boundary, "tieengs vieetj");
    boundary.ackAll();
    require(boundary.text() == "tiếng việt", "Space lost/reordered queued input or disabled conversion");

    Editor punctuation(instance.inputContextManager());
    activate(*addon, telex, punctuation);
    punctuation.delayed = true;
    type(*addon, telex, punctuation, "a,tieengs.vieetj");
    punctuation.ackAll();
    require(punctuation.text() == "a,tiếng.việt", "Punctuation disabled conversion of queued suffixes");

    Editor moved(instance.inputContextManager());
    activate(*addon, telex, moved);
    type(*addon, telex, moved, "tieeng");
    moved.cursor = moved.anchor = 0;
    moved.report();
    const auto deletedBefore = moved.deletions;
    type(*addon, telex, moved, "s");
    require(moved.text() == "stiêng" && moved.deletions == deletedBefore, "Cursor change rewrote old word");

    Editor selected(instance.inputContextManager());
    activate(*addon, telex, selected);
    type(*addon, telex, selected, "tieeng");
    selected.anchor = 0; selected.report();
    const auto deletes = selected.deletions;
    type(*addon, telex, selected, "s");
    require(selected.text() == "s" && selected.deletions == deletes, "Selection must use original key");

    Editor pendingSelection(instance.inputContextManager());
    pendingSelection.document = U"SAFE "; pendingSelection.cursor = pendingSelection.anchor = 5;
    pendingSelection.report(); activate(*addon, telex, pendingSelection);
    pendingSelection.delayed = true;
    type(*addon, telex, pendingSelection, "ti");
    require(pendingSelection.text() == "SAFE t", "Pending selection setup");
    pendingSelection.anchor = 0; pendingSelection.cursor = 5;
    pendingSelection.notify({pendingSelection.text(), 5, 0});
    require(pendingSelection.text() == "SAFE t", "Queued text overwrote a new selection");
    require(pendingSelection.inputPanel().auxUp().toString().find("i") != std::string::npos,
            "Queued text must remain available for explicit recovery");
    press(*addon, telex, pendingSelection, fcitx::Key(FcitxKey_Return));
    require(pendingSelection.text() == "SAFE t", "Recovery must not overwrite a highlighted region");
    press(*addon, telex, pendingSelection, fcitx::Key(FcitxKey_Escape));
    require(pendingSelection.text() == "SAFE t" && pendingSelection.inputPanel().auxUp().empty(),
            "Recovery cancellation changed the selected document");

    Editor recovery(instance.inputContextManager());
    activate(*addon, telex, recovery);
    recovery.delayed = true;
    type(*addon, telex, recovery, "ti");
    recovery.document = U"SAFE "; recovery.cursor = recovery.anchor = 5;
    recovery.notify({recovery.text(), 5, 5});
    require(recovery.text() == "SAFE ", "Context change replayed queued text implicitly");
    press(*addon, telex, recovery, fcitx::Key(FcitxKey_Return));
    require(recovery.text() == "SAFE i", "Explicit recovery lost pending text");

    Editor unicodeRecovery(instance.inputContextManager());
    activate(*addon, telex, unicodeRecovery);
    unicodeRecovery.delayed = true;
    type(*addon, telex, unicodeRecovery, "a");
    press(*addon, telex, unicodeRecovery, fcitx::Key(fcitx::Key::keySymFromUnicode(U'é')));
    unicodeRecovery.document = U"SAFE "; unicodeRecovery.cursor = unicodeRecovery.anchor = 5;
    unicodeRecovery.notify({unicodeRecovery.text(), 5, 5});
    require(unicodeRecovery.inputPanel().auxUp().toString().find("é") != std::string::npos,
            "Unicode queued input was lost");
    press(*addon, telex, unicodeRecovery, fcitx::Key(fcitx::Key::keySymFromUnicode(U'😀')));
    press(*addon, telex, unicodeRecovery, fcitx::Key(FcitxKey_BackSpace));
    press(*addon, telex, unicodeRecovery, fcitx::Key(FcitxKey_Return));
    require(unicodeRecovery.text() == "SAFE é", "Recovery Backspace split a Unicode code point");

    Editor unsupported(instance.inputContextManager());
    unsupported.setCapabilityFlags(fcitx::CapabilityFlag::Preedit);
    activate(*addon, telex, unsupported);
    type(*addon, telex, unsupported, "tieengs");
    require(unsupported.text().empty() && unsupported.preedit() == "tiếng", "Missing capability must fall back to preedit");
    // Changing mode commits existing preedit once; Direct history is never reinserted.
    style(*addon, "Preedit");
    require(unsupported.text() == "tiếng" && unsupported.preedit().empty(), "Switching settings lost preedit");
    style(*addon, "Direct");
    require(unsupported.text() == "tiếng", "Setting toggle duplicated committed text");
    Editor otherIM(instance.inputContextManager());
    otherIM.inputPanel().setClientPreedit(fcitx::Text("other input method"));
    style(*addon, "Preedit");
    require(otherIM.preedit() == "other input method", "Settings cleared another input method's preedit");
    style(*addon, "Direct");

    Editor overflow(instance.inputContextManager());
    activate(*addon, telex, overflow);
    overflow.delayed = true;
    type(*addon, telex, overflow, std::string(80, 'b'));
    require(overflow.text() == std::string(80, 'b'), "Queue overflow lost original input");
    require(!overflow.deletions, "Overflow must not guess replacement ranges");

    Editor longWord(instance.inputContextManager());
    activate(*addon, telex, longWord);
    type(*addon, telex, longWord, std::string(300, 'b'));
    require(longWord.text() == std::string(300, 'b') && longWord.nonemptyPreedits == 0,
            "Long Direct word dropped characters or switched to preedit");

    Editor timeout(instance.inputContextManager());
    activate(*addon, telex, timeout);
    timeout.delayed = true;
    type(*addon, telex, timeout, "tieengs");
    require(timeout.text() == "t", "Timeout setup did not await ACK");
    Editor history(instance.inputContextManager());
    activate(*addon, telex, history);
    type(*addon, telex, history, "aas");
    require(history.text() == "ấ", "History setup");
    const auto historyDeletes = history.deletions;
    history.notify({"a", 1, 1});
    type(*addon, telex, history, "s");
    require(history.text() == "ấs" && history.deletions == historyDeletes,
            "Out-of-order echo after ACK allowed a stale rewrite");

    Editor sensitive(instance.inputContextManager());
    sensitive.setCapabilityFlags(fcitx::CapabilityFlag::Password);
    activate(*addon, telex, sensitive);
    type(*addon, telex, sensitive, "aas");
    require(sensitive.text() == "aas" && !sensitive.deletions && !sensitive.commits,
            "Password must bypass engine");
    // Exiting an sd-event loop is terminal: run this after all tests that add
    // timers, not midway through the test executable.
    auto stop = instance.eventLoop().addTimeEvent(CLOCK_MONOTONIC,
        fcitx::now(CLOCK_MONOTONIC) + 400000, 0,
        [&instance](fcitx::EventSourceTime*, uint64_t) {
            instance.eventLoop().exit();
            return true;
        });
    instance.eventLoop().exec();
    require(timeout.text() == "tieengs" && !timeout.deletions,
            "Timeout must forward queued input without rewriting unconfirmed text");
    timeout.ackAll();
    require(timeout.text() == "tieengs", "Late ACK duplicated timed-out keys");
    style(*addon, "Preedit");
    std::cout << "Direct input: Telex/VNI, no preedit, ACK serialization, Unicode, context guards and fallback passed\n";
}
