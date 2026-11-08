// Test discovery and event routing through a real initialized Fcitx instance.
// Paths are isolated by Fcitx's test helper; no real display/frontend is loaded.
#include "DirectInput.h"
#include "../Sources/OpenKey/linux/ApplicationMemory.h"
#include <fcitx-config/iniparser.h>
#include <fcitx-utils/testing.h>
#include <fcitx/addonmanager.h>
#include <fcitx/inputcontext.h>
#include <fcitx/inputcontextmanager.h>
#include <fcitx/inputmethodgroup.h>
#include <fcitx/inputmethodmanager.h>
#include <fcitx/inputmethodengine.h>
#include <fcitx/inputpanel.h>
#include <fcitx/instance.h>
#include <iostream>
#include <filesystem>
#include <fstream>
#include <cstdlib>
#include <stdexcept>

namespace {
void require(bool value, const char* label) {
    if (!value) throw std::runtime_error(label);
}
class Editor : public fcitx::InputContext {
public:
    explicit Editor(fcitx::InputContextManager& manager, const std::string& program = {}) : InputContext(manager, program) {
        created();
        setCapabilityFlags(fcitx::CapabilityFlag::Preedit);
    }
    ~Editor() override { destroy(); }
    const char* frontend() const override { return "openkey-integration-test"; }
    std::string committed;
    bool directDocument = false;
    std::string preedit() { return inputPanel().clientPreedit().toString(); }
    void report() {
        if (!directDocument) return;
        const auto count = static_cast<unsigned>(openkey::codepoints(committed).size());
        surroundingText().setText(committed, count, count);
        updateSurroundingText();
    }
    bool send(fcitx::Key key) {
        fcitx::KeyEvent event(this, key);
        if (keyEvent(event)) return true;
        // Unaccepted keys reach the client (Preedit Space, Direct boundary keys).
        if (!directDocument || key.isModifier() ||
            (key.states() & fcitx::KeyStates{fcitx::KeyState::Ctrl, fcitx::KeyState::Alt,
                                             fcitx::KeyState::Super}))
            return false;
        const auto ch = fcitx::Key::keySymToUnicode(key.sym());
        if (ch < 32 || ch >= 127) return false;
        committed += fcitx::utf8::UCS4ToUTF8(ch);
        report();
        return true;
    }
    void type(const std::string& text) {
        for (unsigned char ch : text) {
            require(send(fcitx::Key(fcitx::Key::keySymFromUnicode(ch))), "Letter bypassed addon");
        }
    }
private:
    void commitStringImpl(const std::string& text) override { committed += text; report(); }
    void deleteSurroundingTextImpl(int offset, unsigned int size) override {
        if (!directDocument) throw std::runtime_error("Unexpected surrounding text deletion");
        auto value = openkey::codepoints(committed);
        require(offset == -static_cast<int>(size) && size <= value.size(), "Invalid Direct edit range");
        value.resize(value.size() - size);
        committed = openkey::utf8(value);
        report();
    }
    void forwardKeyImpl(const fcitx::ForwardKeyEvent&) override {
        throw std::runtime_error("Unexpected synthesized key");
    }
    void updatePreeditImpl() override {}
};
} // namespace

int main(int argc, char** args) {
    require(argc == 3, "Expected install prefix and relative library directory");
    // Unlike setupTestingEnvironment (which disables all config writes), this
    // fixture needs persistence. All paths are owned temporary directories;
    // no installed addon/data/configuration is modified.
    struct Paths {
        std::filesystem::path root;
        Paths() {
            char pattern[] = "/tmp/openkey-fcitx-test-XXXXXX";
            const auto* directory = mkdtemp(pattern);
            require(directory, "Cannot create private test paths");
            root = directory;
        }
        ~Paths() { std::filesystem::remove_all(root); }
    } paths;
    std::filesystem::create_directories(paths.root / "config");
    std::filesystem::create_directories(paths.root / "data/inputmethod");
    // Test descriptor for the English slot; no real keyboard frontend needed.
    std::ofstream(paths.root / "data/inputmethod/keyboard-us.conf") <<
        "[InputMethod]\nName=English fixture\nUniqueName=keyboard-us\nAddon=openkey\n";
    setenv("SKIP_FCITX_PATH", "1", 1);
    unsetenv("SKIP_FCITX_USER_PATH");
    setenv("FCITX_CONFIG_HOME", (paths.root / "config").c_str(), 1);
    setenv("FCITX_DATA_HOME", (paths.root / "user-data").c_str(), 1);
    setenv("FCITX_CACHE_HOME", (paths.root / "cache").c_str(), 1);
    setenv("FCITX_ADDON_DIRS", (std::filesystem::path(args[1]) / args[2] / "fcitx5").c_str(), 1);
    const auto dataDirs = (paths.root / "data").string() + ":" + (std::filesystem::path(args[1]) / "share/fcitx5").string();
    setenv("FCITX_DATA_DIRS", dataDirs.c_str(), 1);
    char name[] = "openkey-integration-test";
    char disable[] = "--disable=all";
    char enable[] = "--enable=openkey";
    char* argv[] = {name, disable, enable, nullptr};
    fcitx::Instance instance(3, argv);
    instance.addonManager().registerDefaultLoader(nullptr);
    instance.initialize();
    auto& manager = instance.inputMethodManager();
    require(manager.entry("openkey-telex"), "Telex descriptor not discovered");
    require(manager.entry("openkey-vni"), "VNI descriptor not discovered");
    require(instance.addonManager().addon("openkey", false), "Memory addon must load at startup before selecting a Vietnamese method");

    auto group = manager.currentGroup();
    group.inputMethodList().clear();
    group.inputMethodList().emplace_back("openkey-telex");
    group.inputMethodList().emplace_back("openkey-vni");
    for (const auto* name : {"openkey-simple-telex1", "openkey-simple-telex2"}) {
        require(manager.entry(name), "Simple Telex descriptor not discovered");
        group.inputMethodList().emplace_back(name);
    }
    group.setDefaultInputMethod("openkey-telex");
    manager.setGroup(std::move(group));

    for (const auto* name : {"openkey-simple-telex1", "openkey-simple-telex2"}) {
        Editor simple(instance.inputContextManager());
        simple.focusIn();
        instance.setCurrentInputMethod(&simple, name, false);
        simple.type("tieengs");
        require(simple.preedit() == "tiếng", "Simple Telex pipeline preedit");
        require(simple.send(fcitx::Key(FcitxKey_BackSpace)), "Simple Backspace not consumed");
        require(simple.preedit() == "tiến", "Simple Backspace result");
        simple.type("g");
        simple.send(fcitx::Key(FcitxKey_space));
        require(simple.committed == "tiếng", "Simple Telex commit");
        simple.type("w");
        require(simple.preedit() == (std::string(name) == "openkey-simple-telex1" ? "w" : "ư"),
                "Simple Telex standalone W semantics");
        simple.focusOut();
    }
    Editor first(instance.inputContextManager());
    first.focusIn();
    instance.setCurrentInputMethod(&first, "openkey-telex", false);
    require(instance.inputMethod(&first) == "openkey-telex", "Telex not activated");
    first.type("tieengs");
    require(first.preedit() == "tiếng", "Telex pipeline preedit");
    first.focusOut();
    require(first.preedit().empty() && first.committed == "tiếng", "Focus-out lost composition");

    Editor second(instance.inputContextManager());
    second.focusIn();
    instance.setCurrentInputMethod(&second, "openkey-vni", false);
    second.type("vie6t5");
    require(second.preedit() == "việt", "VNI pipeline preedit");
    require(second.send(fcitx::Key(FcitxKey_BackSpace)), "Pipeline Backspace not consumed");
    require(second.preedit() == "việ", "Pipeline Backspace result");
    second.type("t");
    require(!second.send(fcitx::Key(FcitxKey_space)), "Space should reach the application");
    require(second.committed == "việt" && second.preedit().empty(), "VNI pipeline commit");
    second.type("tie6ng1");
    instance.setCurrentInputMethod(&second, "openkey-telex", false);
    require(second.committed == "việttiếng" && second.preedit().empty(), "Method switch lost composition");
    second.type("hoas");
    require(second.preedit() == "hoá", "Mode switch retained old VNI engine state");
    second.focusOut();
    first.focusIn();
    instance.setCurrentInputMethod(&first, "openkey-telex", false);
    first.type("dduowngf");
    require(first.preedit() == "đường", "Focus return did not start a new composition");
    first.focusOut();
    require(first.committed == "tiếngđường", "Focus return duplicated a word");

    // Some frontends commit preedit themselves. The engine/core must not send
    // another commitString; the frontend's own insertion is outside this mock.
    Editor clientCommit(instance.inputContextManager());
    clientCommit.setCapabilityFlags(fcitx::CapabilityFlags{
        fcitx::CapabilityFlag::Preedit, fcitx::CapabilityFlag::ClientUnfocusCommit});
    clientCommit.focusIn();
    instance.setCurrentInputMethod(&clientCommit, "openkey-telex", false);
    clientCommit.type("tieengs");
    require(clientCommit.preedit() == "tiếng", "Client commit preedit missing");
    clientCommit.focusOut();
    require(clientCommit.committed.empty() && clientCommit.preedit().empty(),
            "Server duplicated a client-owned focus-out commit");

    // Without client preedit, the addon owns the server-side composition.
    Editor serverPreedit(instance.inputContextManager());
    serverPreedit.setCapabilityFlags(fcitx::CapabilityFlags{});
    serverPreedit.focusIn();
    instance.setCurrentInputMethod(&serverPreedit, "openkey-telex", false);
    serverPreedit.type("vieetj");
    require(serverPreedit.inputPanel().preedit().toString() == "việt", "Server preedit missing");
    serverPreedit.focusOut();
    require(serverPreedit.committed == "việt" && serverPreedit.inputPanel().preedit().toString().empty(),
            "Server preedit focus-out lost or duplicated a word");

    auto* addon = instance.addonManager().addon("openkey", true);
    fcitx::RawConfig directSettings;
    directSettings.setValueByPath("InputStyle", "Direct");
    addon->setConfig(directSettings);
    Editor direct(instance.inputContextManager());
    direct.directDocument = true;
    direct.setCapabilityFlags(fcitx::CapabilityFlags{
        fcitx::CapabilityFlag::Preedit, fcitx::CapabilityFlag::SurroundingText});
    direct.report();
    direct.focusIn();
    instance.setCurrentInputMethod(&direct, "openkey-telex", false);
    direct.type("print");
    require(direct.committed == "print" && direct.preedit().empty(), "Direct identifier pipeline");
    require(direct.send(fcitx::Key(FcitxKey_space)), "Direct Space must be serialized");
    direct.type("tieengs");
    require(direct.committed == "print tiếng" && direct.preedit().empty(), "Direct Telex pipeline");
    instance.setCurrentInputMethod(&direct, "openkey-vni", false);
    direct.type("vie6t5");
    require(direct.committed == "print tiếngviệt" && direct.preedit().empty(), "Direct VNI pipeline");
    direct.focusOut();
    require(direct.committed == "print tiếngviệt", "Direct pipeline focus-out duplicated text");

    fcitx::RawConfig bufferedSettings;
    bufferedSettings.setValueByPath("InputStyle", "CommitOnly");
    addon->setConfig(bufferedSettings);
    Editor buffered(instance.inputContextManager());
    buffered.setCapabilityFlags(fcitx::CapabilityFlags{
        fcitx::CapabilityFlag::Preedit, fcitx::CapabilityFlag::ClientUnfocusCommit});
    buffered.focusIn();
    instance.setCurrentInputMethod(&buffered, "openkey-telex", false);
    buffered.type("tieengs");
    require(buffered.committed.empty() && buffered.preedit().empty() &&
        buffered.inputPanel().preedit().toString() == "tiếng", "CommitOnly pipeline leaked incremental text");
    buffered.focusOut();
    require(buffered.committed == "tiếng" && buffered.inputPanel().preedit().toString().empty(),
        "CommitOnly focus-out lost or duplicated server-owned text");
    buffered.focusIn();
    instance.setCurrentInputMethod(&buffered, "openkey-telex", false);
    buffered.type("tieengs");
    instance.setCurrentInputMethod(&buffered, "openkey-vni", false);
    require(buffered.committed == "tiếngtiếng", "CommitOnly method switch lost word");
    buffered.type("vie6t5");
    require(!buffered.send(fcitx::Key(FcitxKey_Return)), "CommitOnly Enter must remain native");
    require(buffered.committed == "tiếngtiếngviệt" && buffered.preedit().empty(), "CommitOnly VNI pipeline commit");
    buffered.type("tie6ng1");
    bufferedSettings.setValueByPath("InputStyle", "Preedit");
    addon->setConfig(bufferedSettings);
    require(buffered.committed == "tiếngtiếngviệttiếng" && buffered.preedit().empty(),
        "Leaving CommitOnly must finish the word once");
    buffered.type("vie6t5");
    require(buffered.preedit() == "việt", "Leaving CommitOnly did not restore client preedit");
    buffered.focusOut();
    fcitx::RawConfig nativeSettings;
    nativeSettings.setValueByPath("InputStyle", "NativeDirect");
    addon->setConfig(nativeSettings);
    Editor native(instance.inputContextManager());
    native.directDocument = true;
    native.setCapabilityFlags(fcitx::CapabilityFlags{
        fcitx::CapabilityFlag::Preedit, fcitx::CapabilityFlag::SurroundingText,
        fcitx::CapabilityFlag::ClientUnfocusCommit});
    native.report();
    native.focusIn();
    instance.setCurrentInputMethod(&native, "openkey-telex", false);
    native.type("print tieengs vieetj");
    require(native.committed == "print tiếng việt" && native.preedit().empty() &&
        native.inputPanel().preedit().toString().empty(), "NativeDirect real pipeline lost text or displayed a popup");
    native.focusOut();
    require(native.committed == "print tiếng việt", "NativeDirect pipeline focus-out duplicated a word");
    Editor nativeUrl(instance.inputContextManager());
    nativeUrl.directDocument = true;
    nativeUrl.setCapabilityFlags(fcitx::CapabilityFlags{fcitx::CapabilityFlag::Preedit,
        fcitx::CapabilityFlag::FormattedPreedit, fcitx::CapabilityFlag::SurroundingText,
        fcitx::CapabilityFlag::Url});
    nativeUrl.report(); nativeUrl.focusIn();
    instance.setCurrentInputMethod(&nativeUrl, "openkey-telex", false);
    nativeUrl.type("tieengs vieetj");
    require(nativeUrl.committed == "tiếng việt" && nativeUrl.preedit().empty(),
        "URL capability must not disable NativeDirect conversion in the key pipeline");
    nativeUrl.focusOut();
    require(nativeUrl.committed == "tiếng việt", "NativeDirect URL focus-out duplicated text");

    // Real focus/switch pipeline, including English when OpenKey is inactive.
    // No desktop process or user's input-method group is involved.
    openkey::ApplicationMemory model;
    require(!model.remember("gnome-shell", "openkey-telex") &&
            !model.remember("gtk:unknown", "keyboard-us"), "Bridge ID must not be remembered");
    for (unsigned i = 0; i < openkey::ApplicationMemory::MaxApplications; ++i)
        require(model.remember("app-" + std::to_string(i), "openkey-vni"), "Memory capacity fixture");
    require(!model.remember("over-limit", "keyboard-us"), "Memory must be bounded");
    const auto encoded = model.encode();
    require(!model.load("not-encoded") && model.encode() == encoded, "Malformed memory must not replace valid state");
    openkey::ApplicationMemory loaded;
    require(loaded.load(encoded) && loaded.lookup("app-0") == "openkey-vni", "Memory persistence roundtrip");
    require(loaded.remember("app-0", "unsupported-ime") && loaded.lookup("app-0").empty(),
            "Explicit unsupported method must discard the prior preference");

    auto memoryGroup = manager.currentGroup();
    memoryGroup.inputMethodList().insert(memoryGroup.inputMethodList().begin(), fcitx::InputMethodGroupItem("keyboard-us"));
    manager.setGroup(std::move(memoryGroup));
    auto* engine = dynamic_cast<fcitx::InputMethodEngine*>(instance.addonManager().addon("openkey", true));
    require(engine, "Memory engine missing");
    fcitx::RawConfig remember;
    remember.setValueByPath("RememberInputMethodPerApplication", "True");
    remember.setValueByPath("InputStyle", "Preedit");
    engine->setConfig(remember);
    Editor browser(instance.inputContextManager(), "test-browser");
    Editor code(instance.inputContextManager(), "test-code");
    for (const auto* desired : {"openkey-telex", "openkey-vni", "openkey-simple-telex1", "openkey-simple-telex2", "keyboard-us"}) {
        browser.focusIn();
        instance.setCurrentInputMethod(&browser, desired, false);
        if (instance.inputMethod(&browser) != desired) throw std::runtime_error(std::string("User-selected method: ") + desired + " actual: " + instance.inputMethod(&browser));
        browser.focusOut();
        code.focusIn();
        instance.setCurrentInputMethod(&code, std::string(desired) == "openkey-vni" ? "openkey-telex" : "openkey-vni", false);
        code.focusOut();
        const auto defaultBefore = manager.currentGroup().defaultInputMethod();
        Editor nextWindow(instance.inputContextManager(), "test-browser");
        nextWindow.focusIn();
        require(instance.inputMethod(&nextWindow) == desired, "New context must restore the application's remembered method");
        require(manager.currentGroup().defaultInputMethod() == defaultBefore, "Restore must not change global group default");
        nextWindow.focusOut();
    }
    fcitx::RawConfig savedMemory;
    fcitx::readAsIni(savedMemory, "conf/openkey-app-memory.conf");
    const auto* entries = savedMemory.valueByPath("Entries");
    require(entries && loaded.load(*entries) && loaded.lookup("test-browser") == "keyboard-us",
            "English selection must persist while OpenKey is inactive");
    require(loaded.lookup("test-code") == "openkey-vni", "Applications must have separate remembered methods");
    // Change the fixture file independently: reload must actually read it,
    // not just preserve a previously cached preference.
    loaded.remember("test-code", "openkey-simple-telex2");
    savedMemory.setValueByPath("Entries", loaded.encode());
    require(fcitx::safeSaveAsIni(savedMemory, "conf/openkey-app-memory.conf"), "Cannot save private memory fixture");
    engine->reloadConfig();
    code.focusIn();
    require(instance.inputMethod(&code) == "openkey-simple-telex2", "Changed file must load on config reload");
    instance.setCurrentInputMethod(&code, "openkey-vni", false);
    code.type("vie6t5");
    code.focusOut();
    require(code.committed == "việt", "Focus restore must preserve normal composition/focus-out commit");
    // Shared bridge and password contexts must neither learn nor restore.
    Editor bridge(instance.inputContextManager(), "gnome-shell");
    bridge.focusIn(); instance.setCurrentInputMethod(&bridge, "openkey-telex", false); bridge.focusOut();
    Editor secret(instance.inputContextManager(), "test-private");
    secret.setCapabilityFlags(fcitx::CapabilityFlag::Password);
    secret.focusIn(); instance.setCurrentInputMethod(&secret, "openkey-vni", false); secret.focusOut();
    Editor knownSecret(instance.inputContextManager(), "test-code");
    knownSecret.setCapabilityFlags(fcitx::CapabilityFlag::Password);
    knownSecret.focusIn(); knownSecret.focusOut();
    fcitx::readAsIni(savedMemory, "conf/openkey-app-memory.conf");
    entries = savedMemory.valueByPath("Entries");
    require(entries && loaded.load(*entries) && loaded.lookup("gnome-shell").empty() && loaded.lookup("test-private").empty() &&
            loaded.lookup("test-code") == "openkey-vni",
            "Bridge and sensitive contexts must never be stored");
    const auto completeGroup = manager.currentGroup();
    auto reducedGroup = completeGroup;
    auto& list = reducedGroup.inputMethodList();
    list.erase(std::remove_if(list.begin(), list.end(), [](const auto& item) { return item.name() == "openkey-vni"; }), list.end());
    reducedGroup.setDefaultInputMethod("openkey-telex");
    manager.setGroup(std::move(reducedGroup));
    Editor missingMethod(instance.inputContextManager(), "test-code");
    missingMethod.focusIn();
    require(instance.inputMethod(&missingMethod) != "openkey-vni", "Absent method must not be restored or added");
    missingMethod.focusOut();
    fcitx::readAsIni(savedMemory, "conf/openkey-app-memory.conf");
    require(savedMemory.valueByPath("Entries") && loaded.load(*savedMemory.valueByPath("Entries")) &&
            loaded.lookup("test-code") == "openkey-vni", "Missing group entry must not overwrite the saved preference");
    manager.setGroup(completeGroup);
    remember.setValueByPath("RememberInputMethodPerApplication", "False"); engine->setConfig(remember);
    code.focusIn(); instance.setCurrentInputMethod(&code, "openkey-telex", false); code.focusOut();
    fcitx::readAsIni(savedMemory, "conf/openkey-app-memory.conf");
    require(savedMemory.valueByPath("Entries") && loaded.load(*savedMemory.valueByPath("Entries")) &&
            loaded.lookup("test-code") == "openkey-vni", "Disabled feature must not learn new selections");
    Editor disabled(instance.inputContextManager(), "test-browser");
    instance.setCurrentInputMethod(&disabled, "openkey-telex", true);
    disabled.focusIn();
    require(instance.inputMethod(&disabled) == "openkey-telex", "Disabled feature must not restore saved English preference");
    disabled.focusOut();
    std::cout << "Fcitx5 integration: installed descriptors, addon loading, key pipeline, focus and method switching passed\n";
}
