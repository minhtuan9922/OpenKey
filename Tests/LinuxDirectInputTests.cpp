#include "DirectInput.h"
#include <fcitx-utils/event.h>
#include <fcitx-utils/log.h>
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
#include <sstream>
#include <stdexcept>

namespace {
void require(bool value, const char* label) {
    if (!value) throw std::runtime_error(label);
}
struct LogCapture {
    std::ostringstream stream;
    LogCapture() { fcitx::Log::setLogStream(stream); }
    ~LogCapture() { fcitx::Log::setLogStream(std::cerr); }
};
class Editor : public fcitx::InputContext {
public:
    explicit Editor(fcitx::InputContextManager& manager, const std::string& program = {})
        : InputContext(manager, program) {
        created();
        setCapabilityFlags(fcitx::CapabilityFlags{fcitx::CapabilityFlag::Preedit,
                                               fcitx::CapabilityFlag::SurroundingText});
        report();
    }
    ~Editor() override { destroy(); }
    const char* frontendName = "openkey-direct-test";
    const char* frontend() const override { return frontendName; }
    bool delayed = false;
    std::u32string document;
    unsigned cursor = 0, anchor = 0;
    unsigned deletions = 0, commits = 0, nonemptyPreedits = 0, synthesized = 0;
    bool dropNonAsciiDeletes = false;
    bool dropAllDeletes = false;
    bool holdSurrounding = false;
    bool completeNative = false, completeCommits = false, selectionAwareDeletes = false;
    bool deletionKeepsSelection = false;
    std::u32string completionTail = U"_suggestion";
    // Behavioral fault injection for the reported post-Enter loss, NOT a full
    // Chromium/xterm.js implementation. A native text key reopens IM delivery.
    bool requireNativeAfterEnter = false;
    bool waitingForNativeText = false;
    struct Notification { std::string text; unsigned cursor, anchor; };
    std::deque<Notification> notifications;
    std::string text() const { return openkey::utf8(document); }
    std::string preedit() { return inputPanel().clientPreedit().toString(); }
    void notify(const Notification& value) {
        surroundingText().setText(value.text, value.cursor, value.anchor);
        updateSurroundingText();
    }
    void report() {
        if (holdSurrounding) return;
        Notification value{text(), cursor, anchor};
        if (delayed) notifications.push_back(value);
        else notify(value);
    }
    void reportCompletion() {
        if (cursor == anchor && cursor == document.size() && !document.empty() && document.back() != U' ') {
            document += completionTail;
            anchor = static_cast<unsigned>(document.size());
        }
        report();
    }
    void ackAll() {
        unsigned count = 0;
        while (!notifications.empty()) {
            require(++count < 4096, "ACK processing loop");
            auto value = notifications.front(); notifications.pop_front();
            notify(value);
        }
    }
    unsigned forwarded = 0;
    bool dropUnaccepted = false;
    void raw(const fcitx::Key& key, bool release = false) {
        if (release || key.isModifier()) return;
        ++forwarded;
        if (key.states() & fcitx::KeyStates{fcitx::KeyState::Ctrl, fcitx::KeyState::Alt,
                                           fcitx::KeyState::Super}) {
            if ((key.states() & fcitx::KeyState::Ctrl) && key.sym() == FcitxKey_a) {
                anchor = 0;
                cursor = static_cast<unsigned>(document.size());
                report();
            }
            return;
        }
        if (key.sym() == FcitxKey_BackSpace) {
            if (cursor != anchor) {
                const auto at = std::min(cursor, anchor);
                document.erase(at, std::max(cursor, anchor) - at);
                cursor = anchor = at; report();
            } else if (cursor) { document.erase(--cursor, 1); anchor = cursor; report(); }
        } else if (key.sym() == FcitxKey_Left) {
            if (cursor) --cursor;
            anchor = cursor; report();
        } else if (key.sym() == FcitxKey_Right) {
            if (cursor < document.size()) ++cursor;
            anchor = cursor; report();
        } else if (key.sym() == FcitxKey_Home) {
            cursor = anchor = 0; report();
        } else if (key.sym() == FcitxKey_End) {
            cursor = anchor = static_cast<unsigned>(document.size()); report();
        } else if (key.sym() == FcitxKey_Return || key.sym() == FcitxKey_KP_Enter) {
            const auto start = std::min(cursor, anchor);
            document.replace(start, std::max(cursor, anchor) - start, 1, U'\n');
            cursor = anchor = start + 1;
            report();
            if (requireNativeAfterEnter) waitingForNativeText = true;
        } else {
            const auto ch = fcitx::Key::keySymToUnicode(key.sym());
            if (ch >= 32 && ch < 127) {
                waitingForNativeText = false;
                const auto start = std::min(cursor, anchor);
                document.replace(start, std::max(cursor, anchor) - start, 1, ch);
                cursor = anchor = start + 1;
                if (completeNative && ch != ' ') reportCompletion();
                else report();
            }
        }
    }
private:
    void commitStringImpl(const std::string& text) override {
        ++commits;
        if (waitingForNativeText) return;
        const auto value = openkey::codepoints(text);
        const auto start = std::min(cursor, anchor);
        document.replace(start, std::max(cursor, anchor) - start, value);
        cursor = anchor = start + static_cast<unsigned>(value.size());
        if (completeCommits && !text.empty()) reportCompletion();
        else report();
    }
    void deleteSurroundingTextImpl(int offset, unsigned size) override {
        require(cursor == anchor || (selectionAwareDeletes && cursor < anchor && anchor == document.size()),
            "Deletion must not operate on an unverified selection");
        // A client with no character before the caret ignores the request.
        if (offset > 0 || -offset > static_cast<int>(cursor) || size > cursor) return;
        if (dropAllDeletes) return;
        if (dropNonAsciiDeletes) {
            const auto at = static_cast<unsigned>(static_cast<int>(cursor) + offset);
            for (unsigned i = 0; i < size && at + i < document.size(); ++i)
                if (document[at + i] > 127) return;
        }
        ++deletions;
        // Chromium Textfield extends its selection before deleting. Enable
        // this behavior only in explicit URL-completion fixtures.
        const auto selected = selectionAwareDeletes && !deletionKeepsSelection ? anchor - cursor : 0;
        document.erase(static_cast<unsigned>(static_cast<int>(cursor) + offset), size + selected);
        cursor -= size;
        anchor = deletionKeepsSelection ? anchor - size : cursor;
        report();
    }
    void forwardKeyImpl(const fcitx::ForwardKeyEvent& event) override { ++synthesized; raw(event.rawKey(), event.isRelease()); }
    void updatePreeditImpl() override { if (!preedit().empty()) ++nonemptyPreedits; }
};
void style(fcitx::InputMethodEngine& addon, const char* value) {
    fcitx::RawConfig config;
    config.setValueByPath("InputStyle", value);
    // Deterministic across reruns, including after an interrupted diagnostics test.
    config.setValueByPath("DirectDiagnostics", "False");
    config.setValueByPath("QuickTelex", "False");
    addon.setConfig(config);
}
void activate(fcitx::InputMethodEngine& addon, const fcitx::InputMethodEntry& entry, Editor& editor) {
    fcitx::InputContextEvent event(&editor, fcitx::EventType::InputContextFocusIn);
    addon.activate(entry, event);
}
bool press(fcitx::InputMethodEngine& addon, const fcitx::InputMethodEntry& entry,
           Editor& editor, fcitx::Key key, bool release = false) {
    fcitx::KeyEvent event(&editor, key, release);
    addon.keyEvent(entry, event);
    if (!event.accepted() && !editor.dropUnaccepted) editor.raw(key, release);
    return event.accepted();
}
void type(fcitx::InputMethodEngine& addon, const fcitx::InputMethodEntry& entry,
          Editor& editor, const std::string& value) {
    for (unsigned char ch : value)
        press(addon, entry, editor, fcitx::Key(fcitx::Key::keySymFromUnicode(ch)));
}
} // namespace

int main(int argc, char** args) {
    require(argc == 2, "Module path required");
    LogCapture logs;
    // Diagnostic inspection must be read-only, including while awaiting ACK.
    openkey::DirectInput inspected;
    const openkey::Snapshot empty{U"", 0, 0}, inserted{U"a", 1, 1}, different{U"z", 1, 1};
    const auto statusIs = [&](const std::optional<openkey::Snapshot>& snapshot, const char* value) {
        require(std::string(inspected.snapshotStatus(snapshot)) == value, "Snapshot diagnostic classification");
    };
    statusIs(std::nullopt, "invalid");
    statusIs(empty, "initial");
    inspected.observe(empty);
    statusIs(empty, "current");
    statusIs(different, "changed");
    require(inspected.prepare("", "a").has_value(), "Diagnostic tracker setup");
    statusIs(empty, "stale");
    statusIs(different, "unexpected");
    statusIs(inserted, "expected");
    require(inspected.pending(), "Inspection acknowledged a predicted edit");
    inspected.observe(inserted);
    statusIs(inserted, "current");
    const auto compareIs = [&](const std::optional<openkey::Snapshot>& snapshot, const char* reference,
                               bool comparable, bool text, bool cursor, bool anchor) {
        const auto value = inspected.compare(snapshot);
        require(std::string(value.reference) == reference && value.comparable == comparable &&
                    value.textChanged == text && value.cursorChanged == cursor && value.anchorChanged == anchor,
                "Snapshot comparison must report only the changed components");
    };
    compareIs(inserted, "confirmed", true, false, false, false);
    compareIs(different, "confirmed", true, true, false, false);
    compareIs(openkey::Snapshot{U"a", 0, 1}, "confirmed", true, false, true, false);
    compareIs(openkey::Snapshot{U"a", 1, 0}, "confirmed", true, false, false, true);
    compareIs(std::nullopt, "confirmed", false, false, false, false);
    require(inspected.prepare("a", "ab").has_value(), "Pending comparison setup");
    compareIs(openkey::Snapshot{U"ab", 2, 2}, "expected", true, false, false, false);
    compareIs(inserted, "expected", true, true, true, true);
    require(inspected.pending(), "Diagnostic comparison must not acknowledge an edit");
    inspected.clear();
    compareIs(inserted, "none", false, false, false, false);
    inspected.observe(empty);
    require(inspected.prepare("", "a").has_value(), "Restore late-echo setup");
    inspected.observe(inserted);
    statusIs(empty, "stale");
    require(inspected.observe(empty) == openkey::DirectInput::Observation::Waiting,
            "Late echo must not replace the confirmed snapshot");
    require(!inspected.pending(), "Late echo created a pending edit");
    require(inspected.observe(inserted) == openkey::DirectInput::Observation::Ready,
            "Current snapshot after late echo must restore readiness");
    require(inspected.prepare("a", "").has_value() && inspected.requiresFreshUpdate(),
            "Backspace to previous snapshot must await a notification instead of being rejected");
    require(inspected.observe(empty) == openkey::DirectInput::Observation::Waiting && inspected.pending(),
            "Cached repeated snapshot must not acknowledge a new edit");
    require(inspected.observe(inserted, true) == openkey::DirectInput::Observation::Waiting,
            "Notification of the previous state must not acknowledge Backspace");
    require(inspected.observe(empty, true) == openkey::DirectInput::Observation::Ready && !inspected.pending(),
            "Fresh matching notification must acknowledge repeated snapshot");
    inspected.clear();
    require(!inspected.requiresFreshUpdate(), "Clearing tracker must clear notification requirement");
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
    const openkey::Snapshot typedGo{U"go", 2, 2};
    require(openkey::selectedUrlCompletion(typedGo, {U"google", 2, 6}), "URL completion shape rejected");
    for (const auto& invalid : {openkey::Snapshot{U"google", 6, 2},
            openkey::Snapshot{U"googleX", 2, 6}, openkey::Snapshot{U"agogle", 2, 6},
            openkey::Snapshot{U"google", 1, 6}, openkey::Snapshot{U"go", 2, 2}})
        require(!openkey::selectedUrlCompletion(typedGo, invalid), "URL completion matcher accepted another selection/context");
    require(!openkey::selectedUrlCompletion({U"go tail", 2, 2}, {U"google", 2, 6}),
        "Middle-of-document state must not authorize completion edits");
    style(*addon, "NativeDirect");
    for (const char* frontend : {"ibus", "openkey-direct-test"}) {
        for (const auto* entry : {&telex, &vni}) {
            Editor native(instance.inputContextManager());
            native.frontendName = frontend;
            native.waitingForNativeText = true;
            native.requireNativeAfterEnter = true;
            activate(*addon, *entry, native);
            for (int line = 0; line < 3; ++line) {
                const auto commits = native.commits;
                type(*addon, *entry, native, "ls git cd ");
                require(native.commits == commits, "NativeDirect ASCII appends must use only native keys");
                type(*addon, *entry, native, entry == &telex ? "tieengs vieetj" : "tie6ng1 vie6t5");
                require(!press(*addon, *entry, native, fcitx::Key(FcitxKey_Return)), "NativeDirect Enter swallowed");
            }
            require(native.text() == "ls git cd tiếng việt\nls git cd tiếng việt\nls git cd tiếng việt\n",
                "NativeDirect Telex/VNI multiline result");
            require(native.nonemptyPreedits == 0 && native.inputPanel().preedit().toString().empty(),
                "NativeDirect must not display client or server preedit");
            require(native.synthesized == 0, "NativeDirect must not synthesize native keys");
            type(*addon, *entry, native, entry == &telex ? "coong" : "co6ng");
            require(!press(*addon, *entry, native, fcitx::Key(FcitxKey_BackSpace)), "NativeDirect ordinary Backspace not native");
            type(*addon, *entry, native, entry == &telex ? "j" : "5");
            require(native.text().ends_with("cộn"), "NativeDirect Backspace lost engine history");
            const auto beforeOut = native.text();
            fcitx::InputContextEvent out(&native, fcitx::EventType::InputContextFocusOut);
            addon->deactivate(*entry, out);
            require(native.text() == beforeOut, "NativeDirect focus-out duplicated text");
        }
    }
    {
        Editor cached(instance.inputContextManager());
        cached.frontendName = "ibus";
        activate(*addon, telex, cached);
        type(*addon, telex, cached, "a");
        cached.delayed = true;
        type(*addon, telex, cached, "a"); // document â, delayed notification
        // Matching cache without the corresponding notification is not ACK.
        cached.surroundingText().setText("â", 1, 1);
        type(*addon, telex, cached, "s");
        require(cached.text() == "âs", "Cache-only match authorized an unconfirmed NativeDirect rewrite");
        cached.ackAll();
        require(cached.nonemptyPreedits == 0 && cached.synthesized == 0, "Late ACK caused NativeDirect fallback/forwarding");
    }
    {
        Editor lag(instance.inputContextManager());
        lag.frontendName = "ibus";
        activate(*addon, telex, lag);
        lag.delayed = true;
        type(*addon, telex, lag, "tieengs");
        require(lag.text() == "tiếng" && lag.preedit().empty() && lag.nonemptyPreedits == 0,
            "Unconfirmed NativeDirect word must still convert");
        lag.ackAll();
        type(*addon, telex, lag, "j");
        require(lag.text() == "tiệng", "Tone key still belongs to the syllable until a boundary");
        type(*addon, telex, lag, " ");
        lag.ackAll(); lag.delayed = false;
        type(*addon, telex, lag, "tieengs");
        require(lag.text() == "tiệng tiếng" && lag.nonemptyPreedits == 0,
            "NativeDirect must resume conversion at the next confirmed word");
    }
    {
        // Focus leaves the previous field while its native key is still
        // unconfirmed. The first syllable in Cursor must convert (oo -> ô).
        Editor switched(instance.inputContextManager());
        switched.frontendName = "ibus";
        switched.document = U"elsewhere";
        switched.cursor = switched.anchor = 9;
        switched.report();
        activate(*addon, telex, switched);
        switched.delayed = true;
        type(*addon, telex, switched, "o");
        switched.delayed = false;
        switched.document.clear();
        switched.cursor = switched.anchor = 0;
        switched.notifications.clear();
        switched.report();
        switched.delayed = true;
        type(*addon, telex, switched, "oo");
        require(switched.text() == "ô" && switched.preedit().empty() && switched.nonemptyPreedits == 0,
            "NativeDirect focus change left oo raw");
        switched.ackAll();
        switched.delayed = false;
        type(*addon, telex, switched, " tieen");
        require(switched.text() == "ô tiên", "NativeDirect first word after focus did not convert tieen");
    }
    {
        // Focus: the first key arrives before GNOME advertises SurroundingText
        // (caps 18). It stays native. The next key has caps 82 and turns oo into ô.
        Editor lateCap(instance.inputContextManager());
        lateCap.frontendName = "ibus";
        lateCap.setCapabilityFlags(fcitx::CapabilityFlags{fcitx::CapabilityFlag::Preedit,
                                                          fcitx::CapabilityFlag::FormattedPreedit});
        activate(*addon, telex, lateCap);
        require(!press(*addon, telex, lateCap, fcitx::Key(FcitxKey_o)),
            "first key before SurroundingText must stay native");
        require(lateCap.text() == "o" && lateCap.preedit().empty() && lateCap.commits == 0 &&
                    lateCap.deletions == 0 && lateCap.nonemptyPreedits == 0,
            "first key before SurroundingText was rewritten");
        lateCap.setCapabilityFlags(fcitx::CapabilityFlags{fcitx::CapabilityFlag::Preedit,
                                                          fcitx::CapabilityFlag::FormattedPreedit,
                                                          fcitx::CapabilityFlag::SurroundingText});
        require(press(*addon, telex, lateCap, fcitx::Key(FcitxKey_o)),
            "second o must be consumed once SurroundingText exists");
        require(lateCap.text() == "ô" && lateCap.preedit().empty() && lateCap.nonemptyPreedits == 0,
            "oo stayed raw after SurroundingText arrived");
        type(*addon, telex, lateCap, " tieen");
        require(lateCap.text() == "ô tiên" && lateCap.preedit().empty(),
            "tieen stayed raw after the capability arrived");
    }
    {
        // Cursor chat sends surrounding text that is not the word at the caret.
        // That notification must not drop tieengs, and must not show a preedit.
        Editor noise(instance.inputContextManager());
        noise.frontendName = "ibus";
        activate(*addon, telex, noise);
        type(*addon, telex, noise, "ti");
        noise.surroundingText().setText("not the caret word", 4, 4);
        noise.updateSurroundingText();
        type(*addon, telex, noise, "eengs ");
        require(noise.text() == "tiếng " && noise.preedit().empty() && noise.nonemptyPreedits == 0,
            "unrelated surrounding text dropped tieengs");
        type(*addon, telex, noise, "ddang gox vieetj nef ");
        require(noise.text() == "tiếng đang gõ việt nè " && noise.preedit().empty() &&
                    noise.nonemptyPreedits == 0,
            "words after a noisy snapshot stayed raw");
    }
    {
        // Desktop chat: the first word has no SurroundingText. Later words do,
        // but after every key the client also pushes surrounding text that is
        // not the caret word. That used to leave tieengs/vieetj raw.
        Editor lied(instance.inputContextManager());
        lied.frontendName = "ibus";
        const auto capsNoSt = fcitx::CapabilityFlags{fcitx::CapabilityFlag::Preedit,
            fcitx::CapabilityFlag::FormattedPreedit, fcitx::CapabilityFlag::UppwercaseSentences};
        lied.setCapabilityFlags(capsNoSt);
        lied.holdSurrounding = true;
        activate(*addon, telex, lied);
        type(*addon, telex, lied, "ddang ");
        lied.setCapabilityFlags(capsNoSt | fcitx::CapabilityFlag::SurroundingText);
        lied.holdSurrounding = false;
        lied.surroundingText().setText(lied.text(), lied.cursor, lied.anchor);
        lied.updateSurroundingText();
        for (unsigned char ch : std::string("gox tieengs vieetj nef ")) {
            press(*addon, telex, lied, fcitx::Key(fcitx::Key::keySymFromUnicode(ch)));
            lied.surroundingText().setText("not the caret word", 4, 4);
            lied.updateSurroundingText();
        }
        require(lied.text() == "ddang gõ tiếng việt nè " && lied.preedit().empty() &&
                    lied.nonemptyPreedits == 0,
            "foreign surrounding text after each key left tieengs raw");
    }
    for (const bool terminal : {false, true}) {
        Editor rawContext(instance.inputContextManager());
        rawContext.frontendName = "ibus";
        rawContext.setCapabilityFlags(terminal ?
            fcitx::CapabilityFlags{fcitx::CapabilityFlag::Preedit, fcitx::CapabilityFlag::SurroundingText,
                fcitx::CapabilityFlag::Terminal} : fcitx::CapabilityFlags{fcitx::CapabilityFlag::Preedit});
        rawContext.dropAllDeletes = true;
        activate(*addon, telex, rawContext);
        type(*addon, telex, rawContext, "pwd tieengs vieetj");
        require(rawContext.text() == "pwd tieengs vieetj" && rawContext.commits == 0 && rawContext.deletions == 0 &&
            rawContext.nonemptyPreedits == 0 && rawContext.inputPanel().preedit().toString().empty(),
            "Unsupported NativeDirect context must be native without popup or word loss");
    }
    for (const auto* entry : {&telex, &vni}) {
        Editor omnibox(instance.inputContextManager());
        omnibox.frontendName = "ibus";
        omnibox.setCapabilityFlags(fcitx::CapabilityFlags{fcitx::CapabilityFlag::Preedit,
            fcitx::CapabilityFlag::FormattedPreedit, fcitx::CapabilityFlag::SurroundingText,
            fcitx::CapabilityFlag::Url}); // GNOME reports 4178 for the URL context
        omnibox.document = U"https://example.invalid";
        omnibox.cursor = static_cast<unsigned>(omnibox.document.size()); omnibox.anchor = 0;
        omnibox.report();
        activate(*addon, *entry, omnibox);
        const std::string phrase = entry == &telex ? "ddaay laf tieengs vieetj" : "d9a6y la2 tie6ng1 vie6t5";
        for (unsigned char ch : phrase) {
            press(*addon, *entry, omnibox, fcitx::Key(fcitx::Key::keySymFromUnicode(ch)));
            require(omnibox.preedit().empty() && omnibox.inputPanel().preedit().toString().empty(),
                "NativeDirect URL input must not expose client or server preedit");
        }
        require(omnibox.text() == "đây là tiếng việt" && omnibox.deletions > 0 && omnibox.synthesized == 0,
            "NativeDirect URL fields must convert NFC Telex/VNI through confirmed surrounding edits");
        require(!press(*addon, *entry, omnibox, fcitx::Key(FcitxKey_a, fcitx::KeyState::Ctrl)),
            "URL select-all shortcut must remain native");
        type(*addon, *entry, omnibox, entry == &telex ? "coong" : "co6ng");
        require(!press(*addon, *entry, omnibox, fcitx::Key(FcitxKey_BackSpace)), "URL Backspace must stay native");
        type(*addon, *entry, omnibox, entry == &telex ? "j" : "5");
        require(omnibox.text() == "cộn", "URL Backspace must retain confirmed composition history");
        const auto beforeEnter = omnibox.text();
        require(!press(*addon, *entry, omnibox, fcitx::Key(FcitxKey_Return)), "URL Enter must remain native");
        require(omnibox.text() == beforeEnter + "\n", "URL Enter duplicated/lost committed text");
    }
    {
        Editor suggested(instance.inputContextManager());
        suggested.frontendName = "ibus";
        suggested.setCapabilityFlags(fcitx::CapabilityFlags{fcitx::CapabilityFlag::Preedit,
            fcitx::CapabilityFlag::SurroundingText, fcitx::CapabilityFlag::Url});
        activate(*addon, telex, suggested);
        type(*addon, telex, suggested, "g");
        suggested.document = U"google";
        suggested.cursor = 1; suggested.anchor = 6; // browser-selected completion
        suggested.report();
        const auto deletes = suggested.deletions;
        type(*addon, telex, suggested, "oo");
        require(suggested.text() == "gô" && suggested.deletions == deletes + 1 && suggested.commits == 1,
            "Confirmed URL completion must preserve the word across native input and a later tone edit");
    }
    for (const auto* entry : {&telex, &vni}) {
        for (const unsigned variant : {0u, 1u, 2u, 3u}) {
            Editor completion(instance.inputContextManager());
            completion.frontendName = "ibus";
            completion.completeNative = true;
            completion.completeCommits = (variant & 1u) != 0;
            completion.selectionAwareDeletes = true;
            completion.deletionKeepsSelection = (variant & 2u) != 0;
            completion.setCapabilityFlags(fcitx::CapabilityFlags{fcitx::CapabilityFlag::Preedit,
                fcitx::CapabilityFlag::FormattedPreedit, fcitx::CapabilityFlag::SurroundingText,
                fcitx::CapabilityFlag::Url});
            activate(*addon, *entry, completion);
            const std::string input = entry == &telex ? "coongj hoaf xax hooij chur nghiax " :
                "co6ng5 hoa2 xa4 ho6i5 chu3 nghia4 ";
            for (unsigned char ch : input) {
                press(*addon, *entry, completion, fcitx::Key(fcitx::Key::keySymFromUnicode(ch)));
                require(completion.preedit().empty() && completion.inputPanel().preedit().toString().empty() &&
                    completion.synthesized == 0, "URL completion must not introduce preedit or synthetic keys");
            }
            if (completion.text() != "cộng hoà xã hội chủ nghĩa ")
                throw std::runtime_error("URL completion sentence: [" + completion.text() + "]");
            type(*addon, *entry, completion, entry == &telex ? "coong" : "co6ng");
            require(!press(*addon, *entry, completion, fcitx::Key(FcitxKey_BackSpace)),
                "Backspace on URL completion must remain native");
            require(completion.text() == "cộng hoà xã hội chủ nghĩa công",
                "URL Backspace must remove the selected suggestion, keeping typed letters");
            type(*addon, *entry, completion, entry == &telex ? "j " : "5 ");
            require(completion.text() == "cộng hoà xã hội chủ nghĩa cộng ", "Suggestion Backspace lost tone-edit history");
            require(!press(*addon, *entry, completion, fcitx::Key(FcitxKey_a, fcitx::KeyState::Ctrl)),
                "URL completion select-all swallowed");
            type(*addon, *entry, completion, entry == &telex ? "tieengs " : "tie6ng1 ");
            require(completion.text() == "tiếng ", "URL completion must not retain another selection's word");
        }
    }
    for (const bool url : {false, true}) {
        Editor unsafe(instance.inputContextManager());
        unsafe.frontendName = "ibus";
        auto flags = fcitx::CapabilityFlags{fcitx::CapabilityFlag::Preedit, fcitx::CapabilityFlag::SurroundingText};
        if (url) flags |= fcitx::CapabilityFlag::Url;
        unsafe.setCapabilityFlags(flags);
        activate(*addon, telex, unsafe);
        type(*addon, telex, unsafe, "go");
        if (url) {
            unsafe.delayed = true;
            unsafe.document = U"google"; unsafe.cursor = 2; unsafe.anchor = 6;
            unsafe.surroundingText().setText("google", 2, 6); // no notification
        } else {
            unsafe.document = U"google"; unsafe.cursor = 2; unsafe.anchor = 6; unsafe.report();
        }
        const auto deletes = unsafe.deletions;
        type(*addon, telex, unsafe, "o");
        require(unsafe.text() == "goo" && unsafe.deletions == deletes && unsafe.commits == 0,
            "Non-URL selection or cache-only URL completion must not authorize deletion");
    }
    for (const auto& invalid : {Editor::Notification{"agogle", 2, 6},
            Editor::Notification{"google", 6, 2}, Editor::Notification{"googleX", 2, 6},
            Editor::Notification{"google", 1, 6}}) {
        Editor changedSelection(instance.inputContextManager());
        changedSelection.frontendName = "ibus";
        changedSelection.setCapabilityFlags(fcitx::CapabilityFlags{fcitx::CapabilityFlag::Preedit,
            fcitx::CapabilityFlag::SurroundingText, fcitx::CapabilityFlag::Url});
        activate(*addon, telex, changedSelection);
        type(*addon, telex, changedSelection, "go");
        changedSelection.document = openkey::codepoints(invalid.text);
        changedSelection.cursor = invalid.cursor; changedSelection.anchor = invalid.anchor;
        changedSelection.report();
        type(*addon, telex, changedSelection, "o");
        require(changedSelection.deletions == 0 && changedSelection.commits == 0,
            "Changed/reversed/partial URL selection must not grant composition edit ownership");
    }
    {
        // A lying frontend can corrupt the FIRST replacement. Do not claim to
        // repair that: require that subsequent keys never chain blind rewrites.
        Editor ignoresDelete(instance.inputContextManager());
        ignoresDelete.frontendName = "ibus";
        ignoresDelete.setCapabilityFlags(fcitx::CapabilityFlags{fcitx::CapabilityFlag::Preedit,
            fcitx::CapabilityFlag::SurroundingText, fcitx::CapabilityFlag::Url});
        ignoresDelete.dropAllDeletes = true;
        activate(*addon, telex, ignoresDelete);
        type(*addon, telex, ignoresDelete, "aa");
        const auto afterFault = ignoresDelete.text();
        const auto commits = ignoresDelete.commits;
        type(*addon, telex, ignoresDelete, "sj");
        require(ignoresDelete.text() == afterFault + "sj" && ignoresDelete.commits == commits &&
            ignoresDelete.nonemptyPreedits == 0, "Rejected delete must stop further NativeDirect rewrites");
    }
    {
        Editor moved(instance.inputContextManager());
        moved.frontendName = "ibus";
        activate(*addon, telex, moved);
        type(*addon, telex, moved, "tieengs");
        moved.document = U"unrelated tiếng";
        moved.cursor = moved.anchor = static_cast<unsigned>(moved.document.size());
        moved.report();
        const auto deletes = moved.deletions;
        type(*addon, telex, moved, "j");
        require(moved.text() == "unrelated tiếngj" && moved.deletions == deletes,
            "Matching suffix in a changed context must not authorize NativeDirect deletion");
        type(*addon, telex, moved, " ");
        moved.document = U"selected"; moved.cursor = 8; moved.anchor = 0; moved.report();
        type(*addon, telex, moved, "vieetj");
        require(moved.text() == "việt", "NativeDirect initial selection must be handled by the native key");
        type(*addon, telex, moved, " ");
        type(*addon, telex, moved, "w ");
        require(moved.text() == "việt ư ", "NativeDirect first-key transformation must finish at boundary");
        const auto beforeConfig = moved.text();
        type(*addon, telex, moved, "tieengs");
        style(*addon, "CommitOnly");
        require(moved.text() == beforeConfig + "tiếng", "Leaving NativeDirect duplicated a word");
    }
    style(*addon, "NativeDirect");
    {
        Editor privateField(instance.inputContextManager());
        privateField.frontendName = "ibus";
        activate(*addon, telex, privateField);
        type(*addon, telex, privateField, "tieengs");
        const auto commits = privateField.commits;
        const auto deletes = privateField.deletions;
        privateField.setCapabilityFlags(fcitx::CapabilityFlag::Password);
        type(*addon, telex, privateField, "vieetj");
        require(privateField.text() == "tiếngvieetj" && privateField.commits == commits && privateField.deletions == deletes,
            "NativeDirect must not transform or replay text into sensitive fields");
        require(privateField.nonemptyPreedits == 0, "Sensitive NativeDirect field exposed a preedit");
    }
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

    // Reproduce a frontend reporting an old snapshot after synchronous ACK,
    // then restoring its current cache before the next physical keystroke.
    Editor lateEcho(instance.inputContextManager());
    activate(*addon, telex, lateEcho);
    for (const char ch : std::string("coongj hoaf")) {
        const Editor::Notification previous{lateEcho.text(), lateEcho.cursor, lateEcho.anchor};
        press(*addon, telex, lateEcho, fcitx::Key(fcitx::Key::keySymFromUnicode(ch)));
        lateEcho.notify(previous);
        lateEcho.report();
    }
    require(lateEcho.text() == "cộng hoà" && lateEcho.nonemptyPreedits == 0,
            "Late post-ACK echo erased Telex history");

    Editor lateVni(instance.inputContextManager());
    activate(*addon, vni, lateVni);
    for (const char ch : std::string("vie6t5")) {
        const Editor::Notification previous{lateVni.text(), lateVni.cursor, lateVni.anchor};
        press(*addon, vni, lateVni, fcitx::Key(fcitx::Key::keySymFromUnicode(ch)));
        lateVni.notify(previous);
        lateVni.report();
    }
    require(lateVni.text() == "việt" && lateVni.nonemptyPreedits == 0,
            "Late post-ACK echo erased VNI history");

    Editor corrected(instance.inputContextManager());
    activate(*addon, telex, corrected);
    type(*addon, telex, corrected, "coongg");
    require(corrected.text() == "côngg", "Extra consonant correction setup");
    press(*addon, telex, corrected, fcitx::Key(FcitxKey_BackSpace));
    type(*addon, telex, corrected, "j");
    require(corrected.text() == "cộng" && corrected.nonemptyPreedits == 0,
            "Backspace returning to an old snapshot must preserve tone conversion");

    press(*addon, telex, corrected, fcitx::Key(FcitxKey_BackSpace));
    require(corrected.text() == "cộn", "Backspace must delete a visible character, not a Telex key");
    type(*addon, telex, corrected, "g");
    require(corrected.text() == "cộng", "Retyping a deleted consonant lost Direct history");
    type(*addon, telex, corrected, "sj");
    require(corrected.text() == "cộng", "Changing a tone back to a previous state was rejected");

    Editor correctedVni(instance.inputContextManager());
    activate(*addon, vni, correctedVni);
    type(*addon, vni, correctedVni, "co6ngg");
    press(*addon, vni, correctedVni, fcitx::Key(FcitxKey_BackSpace));
    type(*addon, vni, correctedVni, "5");
    require(correctedVni.text() == "cộng" && correctedVni.nonemptyPreedits == 0,
            "VNI Backspace correction lost tone conversion");

    Editor delayedCorrection(instance.inputContextManager());
    activate(*addon, telex, delayedCorrection);
    type(*addon, telex, delayedCorrection, "coongg");
    delayedCorrection.delayed = true;
    press(*addon, telex, delayedCorrection, fcitx::Key(FcitxKey_BackSpace));
    const auto correctionDeletes = delayedCorrection.deletions;
    type(*addon, telex, delayedCorrection, "j");
    delayedCorrection.notify({"côngg", 5, 5});
    require(delayedCorrection.text() == "công" && delayedCorrection.deletions == correctionDeletes,
            "Old notification released queued correction before Backspace ACK");
    delayedCorrection.ackAll();
    require(delayedCorrection.text() == "cộng", "Delayed Backspace ACK lost the queued tone key");

    Editor correctionSelection(instance.inputContextManager());
    activate(*addon, telex, correctionSelection);
    type(*addon, telex, correctionSelection, "coongg");
    correctionSelection.delayed = true;
    press(*addon, telex, correctionSelection, fcitx::Key(FcitxKey_BackSpace));
    type(*addon, telex, correctionSelection, "j");
    const auto beforeSelection = correctionSelection.deletions;
    correctionSelection.document = U"SAFE";
    correctionSelection.cursor = 4; correctionSelection.anchor = 0;
    correctionSelection.delayed = false;
    correctionSelection.report();
    require(correctionSelection.text() == "SAFE" && correctionSelection.deletions == beforeSelection &&
                correctionSelection.inputPanel().auxUp().toString().find("j") != std::string::npos,
            "Selection change during repeated-state ACK must not replay queued edits over new content");

    Editor burst(instance.inputContextManager());
    activate(*addon, telex, burst);
    burst.delayed = true;
    type(*addon, telex, burst, "tieengs");
    // The unaccepted t stays in the document. The rest of the syllable is preedit
    // immediately, including ee → ê, so a chat box does not wait on commitString.
    require(burst.text() == "t" && burst.preedit() == "tiếng" && burst.nonemptyPreedits > 0 &&
                !burst.deletions,
            "UnACKed burst must show the syllable as preedit");
    burst.notify({"", 0, 0});
    require(burst.text() == "t" && burst.preedit() == "tiếng", "Old snapshot cleared the preedit");
    burst.ackAll();
    require(burst.text() == "t" && burst.preedit() == "tiếng",
            "Replaying notifications cleared the preedit");
    burst.delayed = false;
    require(press(*addon, telex, burst, fcitx::Key(FcitxKey_BackSpace)),
            "Backspace must edit the preedit");
    require(burst.text() == "t" && burst.preedit() == "tiến",
            "Backspace must remove one composed character");

    Editor reopenWord(instance.inputContextManager());
    activate(*addon, telex, reopenWord);
    type(*addon, telex, reopenWord, "coong ");
    press(*addon, telex, reopenWord, fcitx::Key(FcitxKey_BackSpace));
    type(*addon, telex, reopenWord, "j");
    require(reopenWord.text() == "cộng" && reopenWord.nonemptyPreedits == 0,
            "Deleting an accidental Space must restore the previous Telex word");

    type(*addon, telex, reopenWord, " ");
    press(*addon, telex, reopenWord, fcitx::Key(FcitxKey_BackSpace));
    type(*addon, telex, reopenWord, "s");
    require(reopenWord.text() == "cống", "Repeated Space correction lost the previous tone history");

    Editor reopenVni(instance.inputContextManager());
    activate(*addon, vni, reopenVni);
    type(*addon, vni, reopenVni, "co6ng  ");
    press(*addon, vni, reopenVni, fcitx::Key(FcitxKey_BackSpace));
    require(reopenVni.text() == "công ", "First Backspace must remove only one Space");
    press(*addon, vni, reopenVni, fcitx::Key(FcitxKey_BackSpace));
    type(*addon, vni, reopenVni, "5");
    require(reopenVni.text() == "cộng" && !reopenVni.nonemptyPreedits,
            "Multiple Space correction must restore VNI history");

    Editor reopenUnicode(instance.inputContextManager());
    reopenUnicode.document = U"😀 e\u0301 ";
    reopenUnicode.cursor = reopenUnicode.anchor = static_cast<unsigned>(reopenUnicode.document.size());
    reopenUnicode.report(); activate(*addon, telex, reopenUnicode);
    type(*addon, telex, reopenUnicode, "ho ");
    press(*addon, telex, reopenUnicode, fcitx::Key(FcitxKey_BackSpace));
    type(*addon, telex, reopenUnicode, "af");
    require(reopenUnicode.text() == "😀 é hoà", "Reopening partial word damaged Unicode prefix");

    Editor reopenDelayed(instance.inputContextManager());
    activate(*addon, telex, reopenDelayed);
    reopenDelayed.delayed = true;
    type(*addon, telex, reopenDelayed, "coong ");
    press(*addon, telex, reopenDelayed, fcitx::Key(FcitxKey_BackSpace));
    type(*addon, telex, reopenDelayed, "j");
    reopenDelayed.ackAll();
    require(reopenDelayed.text() == "cộng" && reopenDelayed.preedit().empty(),
            "Queued Space/Backspace/tone sequence must wait for ACK and preserve order");

    Editor reopenMoved(instance.inputContextManager());
    activate(*addon, telex, reopenMoved);
    type(*addon, telex, reopenMoved, "coong ");
    reopenMoved.cursor = reopenMoved.anchor = 0;
    reopenMoved.report();
    const auto movedDeletes = reopenMoved.deletions;
    type(*addon, telex, reopenMoved, "j");
    require(reopenMoved.text() == "jcông " && reopenMoved.deletions == movedDeletes,
            "Caret move must discard retained boundary history");

    Editor reopenSelected(instance.inputContextManager());
    activate(*addon, telex, reopenSelected);
    type(*addon, telex, reopenSelected, "coong ");
    reopenSelected.anchor = 0;
    reopenSelected.report();
    const auto selectedDeletes = reopenSelected.deletions;
    type(*addon, telex, reopenSelected, "j");
    require(reopenSelected.text() == "j" && reopenSelected.deletions == selectedDeletes,
            "New selection must never be rewritten from boundary history");

    Editor reopenFocus(instance.inputContextManager());
    activate(*addon, telex, reopenFocus);
    type(*addon, telex, reopenFocus, "coong ");
    fcitx::InputContextEvent leaveBoundary(&reopenFocus, fcitx::EventType::InputContextFocusOut);
    addon->deactivate(telex, leaveBoundary);
    activate(*addon, telex, reopenFocus);
    press(*addon, telex, reopenFocus, fcitx::Key(FcitxKey_BackSpace));
    type(*addon, telex, reopenFocus, "j");
    require(reopenFocus.text() == "côngj", "Focus-out must discard retained boundary history");

    Editor noOlderWord(instance.inputContextManager());
    activate(*addon, telex, noOlderWord);
    type(*addon, telex, noOlderWord, "coong a");
    press(*addon, telex, noOlderWord, fcitx::Key(FcitxKey_BackSpace));
    press(*addon, telex, noOlderWord, fcitx::Key(FcitxKey_BackSpace));
    type(*addon, telex, noOlderWord, "j");
    require(noOlderWord.text() == "côngj", "Boundary history must not scan back through a later word");

    Editor spaceLimit(instance.inputContextManager());
    activate(*addon, telex, spaceLimit);
    type(*addon, telex, spaceLimit, "coong" + std::string(33, ' '));
    for (unsigned i = 0; i < 33; ++i)
        press(*addon, telex, spaceLimit, fcitx::Key(FcitxKey_BackSpace));
    type(*addon, telex, spaceLimit, "j");
    require(spaceLimit.text() == "côngj", "Boundary retention must stop after 32 Spaces");

    Editor reopenPendingSelection(instance.inputContextManager());
    activate(*addon, telex, reopenPendingSelection);
    type(*addon, telex, reopenPendingSelection, "coong ");
    reopenPendingSelection.delayed = true;
    press(*addon, telex, reopenPendingSelection, fcitx::Key(FcitxKey_BackSpace));
    type(*addon, telex, reopenPendingSelection, "j");
    const auto pendingBoundaryDeletes = reopenPendingSelection.deletions;
    reopenPendingSelection.document = U"SAFE";
    reopenPendingSelection.cursor = 4; reopenPendingSelection.anchor = 0;
    reopenPendingSelection.delayed = false;
    reopenPendingSelection.report();
    require(reopenPendingSelection.text() == "SAFE" &&
                reopenPendingSelection.deletions == pendingBoundaryDeletes &&
                reopenPendingSelection.inputPanel().auxUp().toString().find("j") != std::string::npos,
            "Selection change while reopening a word must preserve queued text without overwriting selection");

    Editor boundary(instance.inputContextManager());
    activate(*addon, telex, boundary);
    boundary.delayed = true;
    type(*addon, telex, boundary, "tieengs vieetj");
    require(boundary.text() == "tiếng v" && boundary.preedit() == "việt",
            "Space lost/reordered queued input or disabled conversion");
    boundary.ackAll();
    require(boundary.text() == "tiếng v", "Replaying notifications erased the committed syllable");

    Editor punctuation(instance.inputContextManager());
    activate(*addon, telex, punctuation);
    punctuation.delayed = true;
    type(*addon, telex, punctuation, "a,tieengs.vieetj");
    require(punctuation.text() == "a,tiếng.v" && punctuation.preedit() == "việt",
            "Punctuation disabled conversion of queued suffixes");
    punctuation.ackAll();
    require(punctuation.text() == "a,tiếng.v", "Replaying notifications erased the committed syllable");

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
    require(pendingSelection.text() == "SAFE t" && pendingSelection.preedit() == "ti",
            "Second letter must show as preedit before ACK");
    const auto pendingSelectionDeletes = pendingSelection.deletions;
    pendingSelection.anchor = 0; pendingSelection.cursor = 5;
    pendingSelection.notify({pendingSelection.text(), 5, 0});
    require(pendingSelection.text() == "SAFE t" && pendingSelection.preedit() == "ti" &&
                pendingSelection.deletions == pendingSelectionDeletes,
            "Selection change must not commit the preedit");
    pendingSelection.dropUnaccepted = true;
    require(!press(*addon, telex, pendingSelection, fcitx::Key(FcitxKey_Return)),
            "Enter must stay unaccepted");
    require(pendingSelection.text() == "SAFE t", "Enter must not be replayed over the selection");

    Editor recovery(instance.inputContextManager());
    activate(*addon, telex, recovery);
    recovery.delayed = true;
    type(*addon, telex, recovery, "ti");
    require(recovery.text() == "t" && recovery.preedit() == "ti",
            "Fast second letter must show as preedit");
    recovery.document = U"SAFE "; recovery.cursor = recovery.anchor = 5;
    recovery.notify({recovery.text(), 5, 5});
    require(recovery.text() == "SAFE " && recovery.preedit() == "ti",
            "Surrounding noise must not erase the syllable being typed");
    require(!press(*addon, telex, recovery, fcitx::Key(FcitxKey_Return)),
            "Enter after a context change must stay unaccepted");
    require(recovery.text() == "SAFE \n" && recovery.preedit().empty(),
            "Enter must insert a newline instead of replaying the word");

    Editor continued(instance.inputContextManager());
    activate(*addon, telex, continued);
    continued.delayed = true;
    type(*addon, telex, continued, "ti");
    continued.document = U"OTHER"; continued.cursor = continued.anchor = 5;
    continued.notify({continued.text(), 5, 5});
    type(*addon, telex, continued, "eengs");
    require(continued.preedit() == "tiếng", "Surrounding update split tieengs into the raw tail");

    Editor unusable(instance.inputContextManager());
    activate(*addon, telex, unusable);
    unusable.surroundingText().setText(std::string(65537, 'x'), 0, 0);
    unusable.updateSurroundingText();
    type(*addon, telex, unusable, "chees");
    require(unusable.preedit() == "chế" && unusable.text().find("chees") == std::string::npos,
            "chees must convert when Direct surrounding text is unusable");

    // Direct on ibus must show the composed word before Space and must not underline.
    // The focus-entry bug is independent of Enter: an IM-only word may be
    // ignored until Space. Fault injection captures that observation, not the
    // complete Chromium event protocol. No synthetic keys are permitted.
    for (const auto* entry : {&telex, &vni}) {
        Editor focused(instance.inputContextManager());
        focused.frontendName = "ibus";
        focused.waitingForNativeText = true;
        activate(*addon, *entry, focused);
        const auto commits = focused.commits;
        type(*addon, *entry, focused, "ls abc");
        require(focused.text() == "ls abc" && focused.commits == commits,
                "focus-entry ASCII words must be native, without a leading Space");
        fcitx::InputContextEvent out(&focused, fcitx::EventType::InputContextFocusOut);
        addon->deactivate(*entry, out);
        require(focused.text() == "ls abc", "focus-out must not duplicate native text");
        focused.document.clear(); focused.cursor = focused.anchor = 0; focused.report();
        focused.waitingForNativeText = true;
        activate(*addon, *entry, focused);
        type(*addon, *entry, focused, "git");
        require(focused.text() == "git", "refocus must not require Space or Enter first");

        // Reused context: moving from an unfinished native editor word to a
        // new field need not send activate(). A mismatching ACK must abandon
        // the old word without deleting it into the new destination.
        focused.document = U"prompt> "; focused.cursor = focused.anchor = 8; focused.report();
        focused.waitingForNativeText = true;
        const auto deletes = focused.deletions;
        type(*addon, *entry, focused, "ls");
        require(focused.text() == "prompt> ls" && focused.deletions == deletes,
                "reused focus context must not delete old editor text in the terminal");
    }

    {
        Editor shapedFirst(instance.inputContextManager());
        shapedFirst.frontendName = "ibus";
        activate(*addon, telex, shapedFirst);
        type(*addon, telex, shapedFirst, "w");
        require(shapedFirst.text() == "ư", "fresh-word native path must preserve first-key Telex transformations");
    }

    Editor cursorChat(instance.inputContextManager());
    cursorChat.frontendName = "ibus";
    activate(*addon, telex, cursorChat);
    type(*addon, telex, cursorChat, "keets");
    require(openkey::nfcVietnamese(cursorChat.text()) == "kết" && cursorChat.preedit().empty() &&
                cursorChat.nonemptyPreedits == 0,
            "ibus Direct hid the word until Space");
    require(!press(*addon, telex, cursorChat, fcitx::Key(FcitxKey_space)),
            "Space must stay unaccepted");
    type(*addon, telex, cursorChat, "vieetj soanj ");
    if (openkey::nfcVietnamese(cursorChat.text()) != "kết việt soạn " || !cursorChat.preedit().empty())
        throw std::runtime_error("ibus Direct words: [" + cursorChat.text() + "] preedit [" +
                                 cursorChat.preedit() + "]");
    type(*addon, telex, cursorChat, "gox tieengs vieetj trong chees ddooj khoong gachj chaan");
    require(cursorChat.preedit().empty() && cursorChat.nonemptyPreedits == 0,
            "Direct ibus showed an underline while typing");
    require(!press(*addon, telex, cursorChat, fcitx::Key(FcitxKey_Return)),
            "Enter must stay unaccepted");
    if (openkey::nfcVietnamese(cursorChat.text()) !=
            "kết việt soạn gõ tiếng việt trong chế độ không gạch chân\n" ||
        !cursorChat.preedit().empty())
        throw std::runtime_error("ibus Direct sentence: [" + cursorChat.text() + "] preedit [" +
                                 cursorChat.preedit() + "]");

    // Boundary regression only: the document frontend retains the first word
    // across repeated Enter cycles. This is NOT an xterm.js/browser event model
    // and does not reproduce or prove a fix for the native Wayland report.
    {
        for (const auto* entry : {&telex, &vni}) {
            Editor afterEnter(instance.inputContextManager());
            afterEnter.frontendName = "ibus";
            activate(*addon, *entry, afterEnter);
            for (unsigned line = 0; line < 3; ++line) {
                type(*addon, *entry, afterEnter, "abc");
                require(!press(*addon, *entry, afterEnter, fcitx::Key(FcitxKey_Return)),
                        "ibus Enter must remain native");
                const auto commitsBeforePrime = afterEnter.commits;
                require(!press(*addon, *entry, afterEnter, fcitx::Key(FcitxKey_t)),
                        "ibus first post-Enter key must be delivered natively");
                require(afterEnter.commits == commitsBeforePrime,
                        "ibus native prime must not also commit the same character");
                type(*addon, *entry, afterEnter,
                     entry == &telex ? "ieengs vieetj " : "ie6ng1 vie6t5 ");
                require(afterEnter.preedit().empty() && afterEnter.nonemptyPreedits == 0,
                        "ibus post-Enter must not start client composition");
                require(!press(*addon, *entry, afterEnter, fcitx::Key(FcitxKey_Return)),
                        "ibus second Enter must remain native");
            }
            require(afterEnter.text() == "abc\ntiếng việt \nabc\ntiếng việt \nabc\ntiếng việt \n",
                    "ibus post-Enter first word regression");
        }
    }

    // Reported ASCII symptom: after every Enter, IM-only commits disappear
    // until a native text key (previously Space). The first character must
    // itself be that native key, with no synthesized events or duplicate text.
    {
        Editor terminalGate(instance.inputContextManager());
        terminalGate.frontendName = "ibus";
        terminalGate.requireNativeAfterEnter = true;
        activate(*addon, telex, terminalGate);
        for (unsigned line = 0; line < 3; ++line) {
            require(!press(*addon, telex, terminalGate, fcitx::Key(FcitxKey_Return)),
                    "post-Enter gate: Enter native");
            type(*addon, telex, terminalGate, "abc xyz");
        }
        require(terminalGate.text() == "\nabc xyz\nabc xyz\nabc xyz",
                "post-Enter native prime must preserve the first ASCII word");

        for (const auto* command : {"ls", "git", "cd"}) {
            press(*addon, telex, terminalGate, fcitx::Key(FcitxKey_Return));
            const auto before = terminalGate.text();
            const auto commits = terminalGate.commits;
            for (char ch : std::string(command))
                require(!press(*addon, telex, terminalGate, fcitx::Key(fcitx::Key::keySymFromUnicode(ch))),
                        "post-Enter unchanged ASCII must stay native, not only the first letter");
            require(terminalGate.text() == before + command,
                    "post-Enter shell command must remain intact");
            require(terminalGate.commits == commits,
                    "post-Enter unchanged ASCII command must not use commitString");
        }

        // No ACK: pass raw keys, do not delete or claim ownership of a word
        // whose first native character has not been confirmed by surrounding.
        Editor latePrime(instance.inputContextManager());
        latePrime.frontendName = "ibus";
        activate(*addon, telex, latePrime);
        press(*addon, telex, latePrime, fcitx::Key(FcitxKey_Return));
        latePrime.delayed = true;
        type(*addon, telex, latePrime, "abc xyz ");
        require(latePrime.text() == "\nabc xyz " && latePrime.deletions == 0,
                "unconfirmed native prime must not swallow keys or delete text");

        Editor emptyLines(instance.inputContextManager());
        emptyLines.frontendName = "ibus";
        activate(*addon, telex, emptyLines);
        press(*addon, telex, emptyLines, fcitx::Key(FcitxKey_KP_Enter));
        press(*addon, telex, emptyLines, fcitx::Key(FcitxKey_Return));
        require(!press(*addon, telex, emptyLines, fcitx::Key(FcitxKey_Shift_L)),
                "modifier must not consume the post-Enter native prime");
        require(!press(*addon, telex, emptyLines, fcitx::Key(FcitxKey_a)),
                "empty lines and modifiers must preserve the native prime");
        fcitx::InputContextEvent resetPrime(&emptyLines, fcitx::EventType::InputContextReset);
        addon->reset(telex, resetPrime);
        type(*addon, telex, emptyLines, "bc ");
        require(emptyLines.text() == "\n\nabc ", "reset must not reinsert a native prime");

        Editor noAckCapability(instance.inputContextManager());
        noAckCapability.frontendName = "ibus";
        activate(*addon, telex, noAckCapability);
        press(*addon, telex, noAckCapability, fcitx::Key(FcitxKey_Return));
        noAckCapability.setCapabilityFlags(fcitx::CapabilityFlag::Preedit);
        type(*addon, telex, noAckCapability, "abc ");
        require(noAckCapability.text() == "\nabc " && noAckCapability.commits == 0 &&
                    noAckCapability.deletions == 0,
                "missing surrounding capability must keep the whole first word native");
    }

    // Every Direct field except URL/email bars must get precomposed NFC letters,
    // never a base letter plus a combining mark (mẻ, tiếng, bằng).
    {
        const fcitx::CapabilityFlags nfcCaps[] = {
            fcitx::CapabilityFlags{fcitx::CapabilityFlag::Preedit, fcitx::CapabilityFlag::FormattedPreedit,
                                   fcitx::CapabilityFlag::SurroundingText},
            fcitx::CapabilityFlags{fcitx::CapabilityFlag::Preedit, fcitx::CapabilityFlag::FormattedPreedit,
                                   fcitx::CapabilityFlag::SurroundingText, fcitx::CapabilityFlag::UppwercaseSentences},
            fcitx::CapabilityFlags{fcitx::CapabilityFlag::Preedit, fcitx::CapabilityFlag::FormattedPreedit,
                                   fcitx::CapabilityFlag::SurroundingText, fcitx::CapabilityFlag::UppwercaseSentences,
                                   fcitx::CapabilityFlag::SpellCheck},
        };
        for (const auto& caps : nfcCaps) {
            Editor nfc(instance.inputContextManager());
            nfc.frontendName = "ibus";
            nfc.setCapabilityFlags(caps);
            activate(*addon, telex, nfc);
            type(*addon, telex, nfc, "mer bawngf tieengs vieetj nooij ddaay ");
            const std::string want = "mẻ bằng tiếng việt nội đây ";
            bool combining = false;
            for (auto ch : openkey::codepoints(nfc.text())) combining |= (ch >= 0x300 && ch <= 0x36F);
            if (nfc.text() != want || combining || !nfc.preedit().empty())
                throw std::runtime_error("NFC Direct caps=" + std::to_string(caps.toInteger()) + ": [" +
                                         nfc.text() + "]");
        }
    }

    // a/e/i/o/u/y must be the precomposed letter. Quick Telex is off here:
    // icsc is ícc. ích is ichs. With Quick Telex, the second c is h (see below).
    Editor exact(instance.inputContextManager());
    exact.frontendName = "ibus";
    activate(*addon, telex, exact);
    type(*addon, telex, exact, "gox ichs icsc");
    if (exact.text() != "gõ ích ícc" || exact.text().find("\u0301") != std::string::npos ||
        exact.text().find("\u0303") != std::string::npos || !exact.preedit().empty())
        throw std::runtime_error("ibus precomposed tone: [" + exact.text() + "]");
    Editor shaped(instance.inputContextManager());
    shaped.frontendName = "ibus";
    activate(*addon, telex, shaped);
    type(*addon, telex, shaped, "goo ");
    if (shaped.text() != "gô " || shaped.text().find("\u0302") != std::string::npos ||
        !shaped.preedit().empty())
        throw std::runtime_error("ibus precomposed goo: [" + shaped.text() + "]");

    // Chrome advertises sentence-case and ignores every surrounding delete, so
    // committing ô after o produces goô. Attach U+0302 instead.
    Editor chrome(instance.inputContextManager());
    chrome.frontendName = "ibus";
    chrome.dropAllDeletes = true;
    chrome.setCapabilityFlags(fcitx::CapabilityFlags{fcitx::CapabilityFlag::Preedit,
                                                     fcitx::CapabilityFlag::FormattedPreedit,
                                                     fcitx::CapabilityFlag::SurroundingText,
                                                     fcitx::CapabilityFlag::Url});
    activate(*addon, telex, chrome);
    type(*addon, telex, chrome, "goo");
    if (openkey::nfcVietnamese(chrome.text()) != "gô" || chrome.text().find("goô") != std::string::npos ||
        chrome.text().find("\u00f4") != std::string::npos || !chrome.preedit().empty())
        throw std::runtime_error("chrome goo: [" + chrome.text() + "]");
    type(*addon, telex, chrome, "x aa ee ow uw aw ");
    if (openkey::nfcVietnamese(chrome.text()) != "gỗ â ê ơ ư ă " || !chrome.preedit().empty())
        throw std::runtime_error("chrome shapes: [" + chrome.text() + "] nfc [" +
                                 openkey::nfcVietnamese(chrome.text()) + "]");

    // VS Code/Cursor terminal (xterm.js): deletes never reach the shell, so a
    // Direct rewrite leaves noô/laà and loses đ/ă/tones. Its textarea reports
    // WordCompletion (no autocomplete="off"); some terminals report no
    // surrounding text or the Terminal flag. Use a client preedit there.
    {
        const fcitx::CapabilityFlags terminalCaps[] = {
            fcitx::CapabilityFlags{fcitx::CapabilityFlag::Preedit, fcitx::CapabilityFlag::SurroundingText,
                                   fcitx::CapabilityFlag::Terminal},
        };
        for (const auto& caps : terminalCaps) {
            Editor terminal(instance.inputContextManager());
            terminal.frontendName = "ibus";
            terminal.dropAllDeletes = true;
            terminal.setCapabilityFlags(caps);
            activate(*addon, telex, terminal);
            // xterm.js diffs its textarea and appends every change: a client
            // preedit leaks as dđaây/laà. Those clients get only a final commit.
            const bool appendOnly = static_cast<bool>(caps & fcitx::CapabilityFlag::Terminal);
            type(*addon, telex, terminal, "ddaay");
            const auto shown = appendOnly ? terminal.inputPanel().preedit().toString() : terminal.preedit();
            require(shown == "đây" && terminal.text().empty(),
                    "terminal word must stay unsent until a boundary");
            if (appendOnly)
                require(terminal.preedit().empty() && terminal.nonemptyPreedits == 0,
                        "append-only terminal must never get a client preedit (dđaây)");
            type(*addon, telex, terminal, " laf nooij dung nhaapj bawngf tieengs vieetj");
            require(!press(*addon, telex, terminal, fcitx::Key(FcitxKey_Return)),
                    "terminal Enter must stay unaccepted");
            if (terminal.text() != "đây là nội dung nhập bằng tiếng việt\n" || !terminal.preedit().empty() ||
                terminal.deletions != 0 || (appendOnly && terminal.nonemptyPreedits != 0) ||
                terminal.text().find("dđ") != std::string::npos || terminal.text().find("laà") != std::string::npos)
                throw std::runtime_error("terminal Telex: [" + terminal.text() + "] preedit [" +
                                         terminal.preedit() + "]");
            require(!press(*addon, telex, terminal, fcitx::Key(FcitxKey_space)),
                    "terminal Space must stay unaccepted");
            type(*addon, telex, terminal, "ls");
            require((appendOnly ? terminal.inputPanel().preedit().toString() : terminal.preedit()) == "ls",
                    "terminal ASCII word");
            press(*addon, telex, terminal, fcitx::Key(FcitxKey_Return));
            require(terminal.text() == "đây là nội dung nhập bằng tiếng việt\n ls\n" && terminal.preedit().empty(),
                    "terminal ASCII command");
        }
        // Editor (Monaco: autocomplete=off, no WordCompletion) keeps Direct.
        Editor monaco(instance.inputContextManager());
        monaco.frontendName = "ibus";
        monaco.setCapabilityFlags(fcitx::CapabilityFlags{fcitx::CapabilityFlag::Preedit,
            fcitx::CapabilityFlag::FormattedPreedit, fcitx::CapabilityFlag::SurroundingText});
        activate(*addon, telex, monaco);
        type(*addon, telex, monaco, "nooij");
        require(openkey::nfcVietnamese(monaco.text()) == "nội" && monaco.preedit().empty() &&
                    monaco.nonemptyPreedits == 0,
                "Cursor editor must stay Direct (no underline)");
        // Focus moves to the terminal mid-word: keep the placed word, never re-insert it.
        monaco.setCapabilityFlags(fcitx::CapabilityFlags{fcitx::CapabilityFlag::Preedit,
            fcitx::CapabilityFlag::FormattedPreedit, fcitx::CapabilityFlag::SurroundingText,
            fcitx::CapabilityFlag::Terminal});
        type(*addon, telex, monaco, " ddi");
        press(*addon, telex, monaco, fcitx::Key(FcitxKey_space));
        if (openkey::nfcVietnamese(monaco.text()) != "nội đi " || !monaco.preedit().empty())
            throw std::runtime_error("editor->terminal: [" + monaco.text() + "]");
    }

    // alpha.29 regression: GNOME reports SurroundingText late on focus, and
    // WordCompletion on ordinary fields. Those must stay Direct: mer is mẻ
    // (precomposed, no loose U+0309), never mr, and no underline.
    {
        const fcitx::CapabilityFlags normalCaps[] = {
            fcitx::CapabilityFlags{fcitx::CapabilityFlag::Preedit, fcitx::CapabilityFlag::FormattedPreedit},
            fcitx::CapabilityFlags{fcitx::CapabilityFlag::Preedit, fcitx::CapabilityFlag::FormattedPreedit,
                                   fcitx::CapabilityFlag::UppwercaseSentences},
            fcitx::CapabilityFlags{fcitx::CapabilityFlag::Preedit, fcitx::CapabilityFlag::FormattedPreedit,
                                   fcitx::CapabilityFlag::SurroundingText, fcitx::CapabilityFlag::WordCompletion},
            fcitx::CapabilityFlags{fcitx::CapabilityFlag::Preedit, fcitx::CapabilityFlag::FormattedPreedit,
                                   fcitx::CapabilityFlag::SurroundingText},
        };
        for (const auto& caps : normalCaps) {
            Editor field(instance.inputContextManager());
            field.frontendName = "ibus";
            field.setCapabilityFlags(caps);
            activate(*addon, telex, field);
            type(*addon, telex, field, "mer");
            if (field.text() != "mẻ" || field.text().find("\u0309") != std::string::npos ||
                !field.preedit().empty() || field.nonemptyPreedits != 0)
                throw std::runtime_error("ibus Direct mer caps=" + std::to_string(caps.toInteger()) + ": [" + field.text() + "] preedit [" + field.preedit() + "]");
            type(*addon, telex, field, " ");
            require(field.text() == "mẻ ", "ibus Direct mer space");
        }
        // Surrounding text arrives after the first key (focus race).
        Editor late(instance.inputContextManager());
        late.frontendName = "ibus";
        late.setCapabilityFlags(fcitx::CapabilityFlags{fcitx::CapabilityFlag::Preedit,
                                                       fcitx::CapabilityFlag::FormattedPreedit});
        activate(*addon, telex, late);
        type(*addon, telex, late, "m");
        late.setCapabilityFlags(fcitx::CapabilityFlags{fcitx::CapabilityFlag::Preedit,
            fcitx::CapabilityFlag::FormattedPreedit, fcitx::CapabilityFlag::SurroundingText});
        type(*addon, telex, late, "er");
        if (late.text() != "mẻ" || !late.preedit().empty())
            throw std::runtime_error("late surrounding mer: [" + late.text() + "] preedit [" + late.preedit() + "]");
    }

    // Gõ nhanh Telex: the second c of icsc stands for h, so the syllable is ích.
    {
        fcitx::RawConfig quickOn;
        quickOn.setValueByPath("QuickTelex", "True");
        addon->setConfig(quickOn);
        Editor quickIbus(instance.inputContextManager());
        quickIbus.frontendName = "ibus";
        activate(*addon, telex, quickIbus);
        type(*addon, telex, quickIbus, "icsc");
        if (quickIbus.text() != "ích" || !quickIbus.preedit().empty())
            throw std::runtime_error("ibus QuickTelex icsc: [" + quickIbus.text() + "]");
        type(*addon, telex, quickIbus, " ");
        if (quickIbus.text() != "ích " || !quickIbus.preedit().empty())
            throw std::runtime_error("ibus QuickTelex icsc space: [" + quickIbus.text() + "]");
        fcitx::RawConfig quickOff;
        quickOff.setValueByPath("QuickTelex", "False");
        addon->setConfig(quickOff);
    }

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

    // EN/VI switches the method away and back. Recovery left from the old
    // method must not swallow the first keys after Vietnamese returns.
    Editor switchBack(instance.inputContextManager());
    activate(*addon, telex, switchBack);
    switchBack.delayed = true;
    type(*addon, telex, switchBack, "a");
    press(*addon, telex, switchBack, fcitx::Key(fcitx::Key::keySymFromUnicode(U'é')));
    switchBack.document = U"SAFE "; switchBack.cursor = switchBack.anchor = 5;
    switchBack.notify({switchBack.text(), 5, 5});
    require(switchBack.inputPanel().auxUp().toString().find("é") != std::string::npos,
            "EN/VI setup did not enter recovery");
    fcitx::InputContextEvent switchAway(&switchBack, fcitx::EventType::InputContextSwitchInputMethod);
    addon->deactivate(telex, switchAway);
    fcitx::InputContextEvent switchBackOn(&switchBack, fcitx::EventType::InputContextSwitchInputMethod);
    addon->activate(telex, switchBackOn);
    switchBack.delayed = false;
    type(*addon, telex, switchBack, "a");
    require(switchBack.text().find('a') != std::string::npos || switchBack.preedit().find('a') != std::string::npos,
            "EN/VI switch left the context swallowing keys");

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

    // Config changes settle pending edits with the same safe forwarding path
    // as InputStyle changes, then refresh both modes without reactivation.
    Editor configTelex(instance.inputContextManager());
    Editor configVni(instance.inputContextManager());
    activate(*addon, telex, configTelex);
    activate(*addon, vni, configVni);
    configTelex.delayed = configVni.delayed = true;
    type(*addon, telex, configTelex, "hoas");
    type(*addon, vni, configVni, "hoa1");
    require(configTelex.text() == "h" && configTelex.preedit() == "hoá" &&
                configVni.text() == "h" && configVni.preedit() == "hoá",
            "Pending config setup");
    const auto setCompositionOptions = [&](const char* spelling, const char* modern) {
        fcitx::RawConfig options;
        options.setValueByPath("CheckSpelling", spelling);
        options.setValueByPath("UseModernOrthography", modern);
        addon->setConfig(options);
    };
    setCompositionOptions("False", "False");
    require(configTelex.text() == "hoá" && configTelex.preedit().empty() &&
                configVni.text() == "hoá" && configVni.preedit().empty(),
            "Option change must commit the visible preedit once");
    configTelex.ackAll();
    configVni.ackAll();
    configTelex.delayed = configVni.delayed = false;
    type(*addon, telex, configTelex, " hoas fas");
    type(*addon, vni, configVni, " hoa1 fa1");
    require(configTelex.text() == "hoá hóa fá" && configVni.text() == "hoá hóa fá",
            "Active Direct contexts did not receive new options");
    const auto telexCommits = configTelex.commits;
    const auto vniCommits = configVni.commits;
    setCompositionOptions("True", "True");
    require(configTelex.commits == telexCommits && configVni.commits == vniCommits,
            "Config change recommitted Direct text");
    type(*addon, telex, configTelex, " hoas fas");
    type(*addon, vni, configVni, " hoa1 fa1");
    require(configTelex.text() == "hoá hóa fá hoá fas" && configVni.text() == "hoá hóa fá hoá fa1",
            "Active Direct contexts did not restore enabled options");

    const auto setQuick = [&](const char* value) {
        fcitx::RawConfig options;
        options.setValueByPath("QuickTelex", value);
        addon->setConfig(options);
    };
    Editor quickIdle(instance.inputContextManager());
    Editor quickVni(instance.inputContextManager());
    activate(*addon, telex, quickIdle);
    activate(*addon, vni, quickVni);
    type(*addon, telex, quickIdle, "gg ");
    require(quickIdle.text() == "gg ", "Direct QuickTelex default must be false");
    setQuick("True");
    type(*addon, telex, quickIdle, "gg ");
    type(*addon, vni, quickVni, "gg ");
    require(quickIdle.text() == "gg gi " && quickVni.text() == "gg ",
            "QuickTelex change did not reach idle Direct contexts or affected VNI");
    for (const auto& [input, output] : {
            std::pair{"cc", "ch"}, {"gg", "gi"}, {"kk", "kh"}, {"nn", "ng"},
            {"qq", "qu"}, {"pp", "ph"}, {"tt", "th"}}) {
        Editor quick(instance.inputContextManager());
        Editor numeric(instance.inputContextManager());
        activate(*addon, telex, quick);
        activate(*addon, vni, numeric);
        quick.delayed = numeric.delayed = true;
        type(*addon, telex, quick, input);
        type(*addon, vni, numeric, input);
        quick.ackAll();
        numeric.ackAll();
        require(quick.text() == std::string(input).substr(0, 1) && quick.preedit() == output &&
                    numeric.text() == std::string(input).substr(0, 1) && numeric.preedit() == input,
                "Delayed QuickTelex pairs / VNI isolation failed");
        press(*addon, telex, quick, fcitx::Key(FcitxKey_BackSpace));
        quick.ackAll();
        require(quick.text() == std::string(input).substr(0, 1) &&
                    quick.preedit() == std::string(output).substr(0, 1),
                "Direct QuickTelex Backspace");
        type(*addon, telex, quick, std::string(input).substr(0, 1) + "a");
        quick.ackAll();
        require(quick.preedit() == std::string(output) + "a", "Direct QuickTelex replay after Backspace");
    }
    Editor quickCopy(instance.inputContextManager());
    activate(*addon, telex, quickCopy);
    type(*addon, telex, quickCopy, "GG ");
    press(*addon, telex, quickCopy, fcitx::Key(FcitxKey_BackSpace));
    type(*addon, telex, quickCopy, "a");
    require(quickCopy.text() == "GIa" && !quickCopy.nonemptyPreedits,
            "QuickTelex capitals or retained previous-word copy failed");

    Editor quickPending(instance.inputContextManager());
    activate(*addon, telex, quickPending);
    quickPending.delayed = true;
    type(*addon, telex, quickPending, "gg");
    require(quickPending.text() == "g" && quickPending.preedit() == "gi", "QuickTelex pending ACK setup");
    setQuick("False");
    require(quickPending.text() == "gi" && quickPending.preedit().empty(),
            "QuickTelex toggle must commit the visible pair once");
    const auto pendingCommits = quickPending.commits;
    quickPending.ackAll();
    require(quickPending.text() == "gi" && quickPending.commits == pendingCommits,
            "Late ACK duplicated QuickTelex text after toggle");
    quickPending.delayed = false;
    type(*addon, telex, quickPending, " gg");
    require(quickPending.text() == "gi gg", "New word after disabling QuickTelex");
    const auto beforeQuickEnable = quickPending.commits;
    setQuick("True");
    require(quickPending.text() == "gi gg" && quickPending.commits == beforeQuickEnable,
            "Enabling QuickTelex recommitted Direct text");
    type(*addon, telex, quickPending, " gg");
    require(quickPending.text() == "gi gg gi", "New word after enabling QuickTelex without activation");
    setQuick("False");

    require(logs.stream.str().find("OpenKey Direct:") == std::string::npos,
            "Disabled diagnostics must not log input events");
    fcitx::RawConfig diagnostics;
    diagnostics.setValueByPath("InputStyle", "Direct");
    diagnostics.setValueByPath("DirectDiagnostics", "True");
    addon->setConfig(diagnostics);

    // Exercise the actual logger before it clears the idle tracker. Each
    // notification changes only one component of the acknowledged snapshot.
    const auto changedComponent = [&](const Editor::Notification& snapshot, const char* fields) {
        Editor observed(instance.inputContextManager());
        activate(*addon, telex, observed);
        type(*addon, telex, observed, "b");
        const auto before = logs.stream.str().size();
        observed.notify(snapshot);
        const auto emitted = logs.stream.str().substr(before);
        require(emitted.find("reason=context-invalidated") != std::string::npos &&
                    emitted.find(fields) != std::string::npos,
                "Idle reset log must preserve snapshot comparison before clearing state");
        require(!observed.deletions && observed.text() == "b", "Diagnostics must not mutate the document");
    };
    changedComponent({"z", 1, 1}, "reference=confirmed comparable=1 text_changed=1 cursor_changed=0 anchor_changed=0");
    changedComponent({"b", 0, 1}, "reference=confirmed comparable=1 text_changed=0 cursor_changed=1 anchor_changed=0");
    changedComponent({"b", 1, 0}, "reference=confirmed comparable=1 text_changed=0 cursor_changed=0 anchor_changed=1");

    Editor mismatch(instance.inputContextManager());
    mismatch.document = U"PRIVATE_DOCUMENT_SENTINEL";
    mismatch.cursor = mismatch.anchor = static_cast<unsigned>(mismatch.document.size());
    mismatch.report(); activate(*addon, telex, mismatch);
    mismatch.delayed = true;
    type(*addon, telex, mismatch, "a");
    mismatch.notify({"OTHER_PRIVATE_SENTINEL", 22, 22});
    require(!mismatch.deletions, "Mismatched snapshot must not cause deletion");

    Editor invalid(instance.inputContextManager());
    activate(*addon, telex, invalid);
    invalid.delayed = true;
    type(*addon, telex, invalid, "a");
    invalid.surroundingText().invalidate();
    invalid.updateSurroundingText();
    require(!invalid.deletions, "Invalid snapshot must not cause deletion");

    diagnostics.setValueByPath("DirectDiagnostics", "False");
    addon->setConfig(diagnostics);
    const auto beforeDisabled = logs.stream.str();
    Editor disabled(instance.inputContextManager());
    activate(*addon, telex, disabled);
    type(*addon, telex, disabled, "hoaf");
    require(logs.stream.str() == beforeDisabled, "Disabling diagnostics must stop logging");
    diagnostics.setValueByPath("DirectDiagnostics", "True");
    addon->setConfig(diagnostics);

    diagnostics.setValueByPath("InputStyle", "NativeDirect");
    addon->setConfig(diagnostics);
    {
        // A text editor resets the context after commitString. hoo → hô must
        // stay one syllable so the following tone key lands on ô, not on i.
        Editor editorReset(instance.inputContextManager());
        editorReset.frontendName = "ibus";
        activate(*addon, telex, editorReset);
        type(*addon, telex, editorReset, "xax");
        require(!press(*addon, telex, editorReset, fcitx::Key(FcitxKey_Return)),
            "NativeDirect Enter must stay unaccepted");
        for (unsigned char ch : std::string("hooij")) {
            const auto before = editorReset.text();
            press(*addon, telex, editorReset, fcitx::Key(fcitx::Key::keySymFromUnicode(ch)));
            if (editorReset.text() != before + std::string(1, static_cast<char>(ch))) {
                fcitx::InputContextEvent reset(&editorReset, fcitx::EventType::InputContextReset);
                addon->reset(telex, reset);
            }
        }
        require(editorReset.text() == "xã\nhội" && editorReset.preedit().empty(),
            "Editor reset after ô split hội into hôị");
        Editor wiped(instance.inputContextManager());
        wiped.frontendName = "ibus";
        activate(*addon, telex, wiped);
        auto stroke = [&](const std::string& keys) {
            for (unsigned char ch : keys) {
                const auto before = wiped.text();
                press(*addon, telex, wiped, fcitx::Key(fcitx::Key::keySymFromUnicode(ch)));
                if (wiped.text() != before + std::string(1, static_cast<char>(ch))) {
                    fcitx::InputContextEvent reset(&wiped, fcitx::EventType::InputContextReset);
                    addon->reset(telex, reset);
                    wiped.surroundingText().setText("", 0, 0);
                    wiped.updateSurroundingText();
                }
            }
        };
        stroke("xax");
        require(!press(*addon, telex, wiped, fcitx::Key(FcitxKey_Return)),
            "Enter before hội must stay unaccepted");
        stroke("hooij");
        require(!press(*addon, telex, wiped, fcitx::Key(FcitxKey_Return)),
            "Enter before cộng must stay unaccepted");
        stroke("coongj");
        require(wiped.text() == "xã\nhội\ncộng" && wiped.preedit().empty(),
            "Empty surrounding cache after ô left hôị and côngj");
        Editor moved(instance.inputContextManager());
        moved.frontendName = "ibus";
        activate(*addon, telex, moved);
        type(*addon, telex, moved, "hoo");
        fcitx::InputContextEvent resetMoved(&moved, fcitx::EventType::InputContextReset);
        addon->reset(telex, resetMoved);
        moved.document = U"other";
        moved.cursor = moved.anchor = static_cast<unsigned>(moved.document.size());
        moved.report();
        const auto deletes = moved.deletions;
        type(*addon, telex, moved, "j");
        require(moved.text() == "otherj" && moved.deletions == deletes,
            "Editor reset must not keep deleting after the caret moves");
        // The committed word is hô, but the client's next surrounding update is
        // still the pre-transform document. That echo used to close the syllable,
        // so ij became ị (hôị) and ngj left a literal j (côngj).
        Editor stale(instance.inputContextManager());
        stale.frontendName = "ibus";
        activate(*addon, telex, stale);
        type(*addon, telex, stale, "xax");
        require(!press(*addon, telex, stale, fcitx::Key(FcitxKey_Return)),
            "Enter before a stale echo must stay unaccepted");
        type(*addon, telex, stale, "hoo");
        require(stale.text() == "xã\nhô", "Stale-echo setup");
        stale.notify({"xã\nho", 5, 5});
        type(*addon, telex, stale, "ij");
        require(stale.text() == "xã\nhội" && stale.preedit().empty(),
            "Pre-transform echo split hội into hôị");
        require(!press(*addon, telex, stale, fcitx::Key(FcitxKey_Return)),
            "Enter before cộng must stay unaccepted");
        type(*addon, telex, stale, "coo");
        std::string echo = stale.text();
        echo.replace(echo.size() - std::string("ô").size(), std::string("ô").size(), "o");
        const auto echoChars = static_cast<unsigned>(openkey::codepoints(echo).size());
        stale.notify({echo, echoChars, echoChars});
        type(*addon, telex, stale, "ngj");
        require(stale.text() == "xã\nhội\ncộng" && stale.preedit().empty(),
            "Pre-transform echo left côngj");
    }
    diagnostics.setValueByPath("InputStyle", "Direct");
    addon->setConfig(diagnostics);
    // Word-boundary keys must reach the client and cut composition tracking.
    Editor enterCut(instance.inputContextManager());
    activate(*addon, telex, enterCut);
    type(*addon, telex, enterCut, "xax");
    require(enterCut.text() == "xã", "Enter-cut setup");
    require(!press(*addon, telex, enterCut, fcitx::Key(FcitxKey_Return)),
            "Enter must stay unaccepted so the client inserts a newline");
    require(enterCut.text() == "xã\n", "Enter must insert a newline after the word");
    type(*addon, telex, enterCut, "hooij");
    require(enterCut.text() == "xã\nhội", "Enter must cut the previous word before the next");

    Editor punctCut(instance.inputContextManager());
    activate(*addon, telex, punctCut);
    type(*addon, telex, punctCut, "xax.");
    require(punctCut.text() == "xã.", "Punctuation must commit as a delimiter");
    type(*addon, telex, punctCut, "hooij");
    require(punctCut.text() == "xã.hội", "Punctuation must cut the previous word");

    Editor spaceCut(instance.inputContextManager());
    activate(*addon, telex, spaceCut);
    type(*addon, telex, spaceCut, "xax hooij");
    require(spaceCut.text() == "xã hội", "Space must cut words in Direct mode");

    // First Direct letter after focus: unaccepted native insert (Qt wake), no preedit.
    Editor wordStart(instance.inputContextManager());
    activate(*addon, telex, wordStart);
    const auto preeditsBefore = wordStart.nonemptyPreedits;
    require(!press(*addon, telex, wordStart, fcitx::Key(FcitxKey_a)),
            "First Direct letter must stay unaccepted for native insert");
    require(wordStart.text() == "a" && wordStart.nonemptyPreedits == preeditsBefore,
            "Native first letter must appear without client preedit");
    type(*addon, telex, wordStart, "bc");
    require(wordStart.text() == "abc" && wordStart.nonemptyPreedits == preeditsBefore,
            "Later Direct letters must Surrounding-edit without preedit");

    // LibreOffice-like program: SurroundingText is unreliable — force Preedit under Direct.
    Editor libre(instance.inputContextManager(), "soffice.bin");
    activate(*addon, telex, libre);
    type(*addon, telex, libre, "tieengs");
    require(libre.preedit() == "tiếng" && libre.text().empty(),
            "soffice.bin must use Preedit so the first character is not dropped");
    require(!press(*addon, telex, libre, fcitx::Key(FcitxKey_Return)),
            "Enter under LibreOffice Preedit fallback must stay unaccepted");
    require(libre.text() == "tiếng\n" || libre.text() == "tiếng",
            "LibreOffice Preedit fallback must commit the full word including the first character");

    // Home/End must stay unaccepted and end Direct ownership of the old word.
    Editor homeEnd(instance.inputContextManager());
    activate(*addon, telex, homeEnd);
    type(*addon, telex, homeEnd, "tieengs");
    require(homeEnd.text() == "tiếng", "Home/End setup");
    const auto homeForwarded = homeEnd.forwarded;
    require(!press(*addon, telex, homeEnd, fcitx::Key(FcitxKey_Home)),
            "Home must stay unaccepted for native caret move");
    require(homeEnd.cursor == 0 && homeEnd.forwarded == homeForwarded + 1,
            "Home must move the caret without IM capture");
    type(*addon, telex, homeEnd, "a");
    require(homeEnd.text().find('a') != std::string::npos && homeEnd.text() != "átiếng",
            "Typing after Home must not rewrite the old Direct suffix as one word");
    Editor endNav(instance.inputContextManager());
    activate(*addon, telex, endNav);
    endNav.document = U"prefix ";
    endNav.cursor = endNav.anchor = 7;
    endNav.report();
    type(*addon, telex, endNav, "xax");
    require(!press(*addon, telex, endNav, fcitx::Key(FcitxKey_End)), "End must stay unaccepted");
    require(endNav.cursor == endNav.document.size(), "End must move to the end of the document");

    // Shortcuts while composing must stay unaccepted (not queued as text).
    Editor shortcut(instance.inputContextManager());
    activate(*addon, telex, shortcut);
    type(*addon, telex, shortcut, "xax");
    require(shortcut.text() == "xã", "Shortcut setup");
    const auto beforeShortcut = shortcut.text();
    const auto shortcutAccepted = press(*addon, telex, shortcut,
        fcitx::Key(FcitxKey_a, fcitx::KeyStates{fcitx::KeyState::Ctrl}));
    require(!shortcutAccepted, "Ctrl+A must stay unaccepted by Direct");
    require(shortcut.anchor == 0 && shortcut.cursor == shortcut.document.size(),
            "Ctrl+A must select all via native delivery");
    require(shortcut.text() == beforeShortcut, "Ctrl+A must not repeat or mutate the Direct word");

    // Pending ACK + Enter: assumeAcked, flush composing queue, leave Enter unaccepted.
    Editor pendingEnter(instance.inputContextManager());
    activate(*addon, telex, pendingEnter);
    pendingEnter.delayed = true;
    type(*addon, telex, pendingEnter, "xa");
    require(pendingEnter.text() == "x" && pendingEnter.preedit() == "xa",
            "Second letter must show the syllable as preedit before ACK");
    require(!press(*addon, telex, pendingEnter, fcitx::Key(FcitxKey_Return)),
            "Enter during pending ACK must stay unaccepted for native newline");
    pendingEnter.ackAll();
    require(pendingEnter.text().find('\n') != std::string::npos,
            "Enter during pending ACK must still insert a newline");

    // Clients without Preedit must not lose keys into invisible composition.
    Editor noPreedit(instance.inputContextManager());
    noPreedit.setCapabilityFlags(fcitx::CapabilityFlag::SurroundingText);
    activate(*addon, telex, noPreedit);
    // Force Direct off by invalidating surrounding so fallback is considered.
    noPreedit.surroundingText().invalidate();
    const auto noPreeditAccepted = press(*addon, telex, noPreedit, fcitx::Key(FcitxKey_a));
    require(!noPreeditAccepted && noPreedit.text() == "a" && noPreedit.preedit().empty(),
            "Without Preedit capability keys must pass through, not enter server preedit");

    Editor overflow(instance.inputContextManager());
    activate(*addon, telex, overflow);
    overflow.delayed = true;
    type(*addon, telex, overflow, std::string(80, 'b'));
    require(overflow.text() == "b" && overflow.preedit() == std::string(80, 'b') && overflow.nonemptyPreedits > 0,
            "Fast Direct word must stay visible as preedit");
    // Queue overflow still applies to a later edit, after the native prime was ACKed.
    Editor queued(instance.inputContextManager());
    activate(*addon, telex, queued);
    type(*addon, telex, queued, "b");
    queued.delayed = true;
    type(*addon, telex, queued, std::string(66, 'b'));
    require(queued.text() == std::string(67, 'b') && !queued.deletions,
            "Queue overflow must not guess a replacement range");

    Editor longWord(instance.inputContextManager());
    activate(*addon, telex, longWord);
    type(*addon, telex, longWord, std::string(300, 'b'));
    require(longWord.text() == std::string(300, 'b') && longWord.nonemptyPreedits == 0,
            "Long Direct word dropped characters or switched to preedit");

    Editor timeout(instance.inputContextManager());
    activate(*addon, telex, timeout);
    timeout.delayed = true;
    type(*addon, telex, timeout, "t");
    require(timeout.text() == "t" && timeout.preedit().empty() && !timeout.commits,
            "A single unACKed prime must stay pending until the timeout");
    Editor staleTimeout(instance.inputContextManager());
    activate(*addon, telex, staleTimeout);
    staleTimeout.delayed = true;
    type(*addon, telex, staleTimeout, "h");
    staleTimeout.notify({"", 0, 0}); // Notification arrived, but cache is still the old snapshot.
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
    // Client that drops the unaccepted first key (LibreOffice/Qt on the GNOME
    // ibus bridge). The timer runs with the others; assertions are after exec.
    Editor dropped(instance.inputContextManager());
    dropped.dropUnaccepted = true;
    dropped.delayed = true;
    activate(*addon, telex, dropped);
    type(*addon, telex, dropped, "tieengs");
    require(dropped.text().empty() && dropped.preedit() == "tiếng",
            "A dropped first key must reappear inside the preedit syllable");
    // Exiting an sd-event loop is terminal: run this after all tests that add
    // timers, not midway through the test executable.
    // Leave room for sd-event's default timer coalescing: a 400 ms stop
    // could race the second 250 ms timeout and miss its diagnostic.
    auto stop = instance.eventLoop().addTimeEvent(CLOCK_MONOTONIC,
        fcitx::now(CLOCK_MONOTONIC) + 1000000, 1,
        [&instance](fcitx::EventSourceTime*, uint64_t) {
            instance.eventLoop().exit();
            return true;
        });
    instance.eventLoop().exec();
    require(dropped.text().empty() && dropped.preedit() == "tiếng",
            "Timeout must not rewrite a syllable already shown as preedit");
    require(!press(*addon, telex, dropped, fcitx::Key(FcitxKey_Return)),
            "Enter after preedit must stay unaccepted");
    require(dropped.text() == "tiếng" && dropped.preedit().empty(),
            "Enter must commit the preedit once, including the first character");
    require(timeout.text() == "t" && timeout.preedit() == "t" && !timeout.deletions && !timeout.commits,
            "UnACKed native prime must become preedit, not a forwarded rewrite");
    timeout.ackAll();
    require(timeout.text() == "t" && timeout.commits == 0,
            "Late ACK duplicated the primed character");
    timeout.notify({"t", 1, 1});
    require(!press(*addon, telex, timeout, fcitx::Key(FcitxKey_Return)),
            "Enter after a landed native prime must stay unaccepted");
    require(timeout.text() == "t\n" && timeout.preedit().empty(),
            "Enter commits the promoted character and still inserts a newline");
    diagnostics.setValueByPath("InputStyle", "NativeDirect");
    addon->setConfig(diagnostics);
    {
        const auto begin = logs.stream.str().size();
        Editor email(instance.inputContextManager());
        email.frontendName = "ibus";
        email.setCapabilityFlags(fcitx::CapabilityFlags{fcitx::CapabilityFlag::Preedit,
            fcitx::CapabilityFlag::FormattedPreedit, fcitx::CapabilityFlag::SurroundingText,
            fcitx::CapabilityFlag::Email});
        activate(*addon, telex, email);
        type(*addon, telex, email, "tieengs");
        require(email.text() == "tieengs" && email.commits == 0, "Email diagnostics must not change bypass policy");
        const auto emitted = logs.stream.str().substr(begin);
        require(emitted.find("capabilities=210 cause=email-context snapshot=untracked") != std::string::npos,
            "NativeDirect diagnostics must identify explicit email bypass");
    }
    {
        const auto begin = logs.stream.str().size();
        Editor noNotification(instance.inputContextManager());
        noNotification.frontendName = "ibus";
        activate(*addon, telex, noNotification);
        noNotification.delayed = true;
        type(*addon, telex, noNotification, "b");
        noNotification.surroundingText().setText("b", 1, 1);
        type(*addon, telex, noNotification, "c");
        const auto emitted = logs.stream.str().substr(begin);
        require(emitted.find("cause=awaiting-surrounding-update snapshot=expected") != std::string::npos &&
            emitted.find("reference=native-expected comparable=1 text_changed=0 cursor_changed=0 anchor_changed=0") != std::string::npos &&
            emitted.find("native_waiting_update=1 native_raw_word=0") != std::string::npos,
            "NativeDirect diagnostics must distinguish matching cache from missing notification");
        type(*addon, telex, noNotification, "d");
        require(logs.stream.str().substr(begin).find("cause=raw-word-in-progress") != std::string::npos,
            "Raw remainder must not be misreported as another missing ACK");
    }
    {
        const auto begin = logs.stream.str().size();
        Editor changed(instance.inputContextManager());
        activate(*addon, telex, changed);
        type(*addon, telex, changed, "b");
        changed.notify({"PRIVATE_DOCUMENT_SENTINEL", 2, 1});
        type(*addon, telex, changed, "a");
        const auto emitted = logs.stream.str().substr(begin);
        require(emitted.find("cause=snapshot-mismatch snapshot=unexpected") != std::string::npos &&
            emitted.find("reference=native-expected comparable=1 text_changed=1 cursor_changed=1 anchor_changed=0") != std::string::npos,
            "NativeDirect diagnostics must compare against its own expected context");
    }
    const auto diagnosticLog = logs.stream.str();
    for (const char* cause : {"cause=queue-overflow snapshot=stale pending=1",
                             "cause=ack-timeout snapshot=stale pending=1 notified=0",
                             "cause=ack-timeout snapshot=stale pending=1 notified=1",
                             "cause=surrounding-mismatch snapshot=unexpected pending=1",
                             "cause=surrounding-invalid snapshot=invalid pending=1"})
        require(diagnosticLog.find(cause) != std::string::npos, cause);
    for (const char* secret : {"PRIVATE_DOCUMENT_SENTINEL", "OTHER_PRIVATE_SENTINEL", "tieengs"})
        require(diagnosticLog.find(secret) == std::string::npos, "Diagnostics exposed document/keystroke content");
    style(*addon, "Preedit");
    std::cout << "Direct input: Telex/VNI, no preedit, ACK serialization, Unicode, context guards and fallback passed\n";
}
