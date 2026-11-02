// Test discovery and event routing through a real initialized Fcitx instance.
// Paths are isolated by Fcitx's test helper; no real display/frontend is loaded.
#include <fcitx-utils/testing.h>
#include <fcitx/addonmanager.h>
#include <fcitx/inputcontext.h>
#include <fcitx/inputcontextmanager.h>
#include <fcitx/inputmethodgroup.h>
#include <fcitx/inputmethodmanager.h>
#include <fcitx/inputpanel.h>
#include <fcitx/instance.h>
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
        setCapabilityFlags(fcitx::CapabilityFlag::Preedit);
    }
    ~Editor() override { destroy(); }
    const char* frontend() const override { return "openkey-integration-test"; }
    std::string committed;
    std::string preedit() { return inputPanel().clientPreedit().toString(); }
    bool send(fcitx::Key key) {
        fcitx::KeyEvent event(this, key);
        const bool handled = keyEvent(event);
        return handled;
    }
    void type(const std::string& text) {
        for (unsigned char ch : text) {
            require(send(fcitx::Key(fcitx::Key::keySymFromUnicode(ch))), "Letter bypassed addon");
        }
    }
private:
    void commitStringImpl(const std::string& text) override { committed += text; }
    void deleteSurroundingTextImpl(int, unsigned int) override {
        throw std::runtime_error("Unexpected surrounding text deletion");
    }
    void forwardKeyImpl(const fcitx::ForwardKeyEvent&) override {
        throw std::runtime_error("Unexpected synthesized key");
    }
    void updatePreeditImpl() override {}
};
} // namespace

int main(int argc, char** args) {
    require(argc == 3, "Expected install prefix and relative library directory");
    // The string-path helper also exists in Ubuntu 24.04's Fcitx 5.1.7 SDK.
    fcitx::setupTestingEnvironment(args[1], {std::string(args[2]) + "/fcitx5"},
                                   {"share/fcitx5"});
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
    require(instance.addonManager().addon("openkey", true), "Addon could not load from install prefix");

    auto group = manager.currentGroup();
    group.inputMethodList().clear();
    group.inputMethodList().emplace_back("openkey-telex");
    group.inputMethodList().emplace_back("openkey-vni");
    group.setDefaultInputMethod("openkey-telex");
    manager.setGroup(std::move(group));

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
    std::cout << "Fcitx5 integration: installed descriptors, addon loading, key pipeline, focus and method switching passed\n";
}
