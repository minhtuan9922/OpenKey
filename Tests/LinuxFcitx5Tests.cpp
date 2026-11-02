// Exercise the real addon with a fake application frontend. No hooks, desktop
// session, daemon restart, or user input-method configuration is needed.
#include <fcitx/addonfactory.h>
#include <fcitx/addonmanager.h>
#include <fcitx/inputcontext.h>
#include <fcitx/inputcontextmanager.h>
#include <fcitx/inputmethodengine.h>
#include <fcitx/inputpanel.h>
#include <fcitx/instance.h>
#include <dlfcn.h>
#include <iostream>
#include <memory>
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
    const char* frontend() const override { return "openkey-test"; }
    std::string committed;
    std::string preedit() { return inputPanel().clientPreedit().toString(); }
private:
    void commitStringImpl(const std::string& text) override { committed += text; }
    void deleteSurroundingTextImpl(int, unsigned int) override {
        throw std::runtime_error("Must not delete committed text");
    }
    void forwardKeyImpl(const fcitx::ForwardKeyEvent&) override {
        throw std::runtime_error("Must not synthesize keys");
    }
    void updatePreeditImpl() override {}
};
bool key(fcitx::InputMethodEngine& addon, const fcitx::InputMethodEntry& entry,
         Editor& editor, fcitx::Key value, bool release = false) {
    fcitx::KeyEvent event(&editor, value, release);
    addon.keyEvent(entry, event);
    return event.accepted();
}
void type(fcitx::InputMethodEngine& addon, const fcitx::InputMethodEntry& entry,
          Editor& editor, const std::string& input) {
    for (unsigned char ch : input)
        require(key(addon, entry, editor, fcitx::Key(fcitx::Key::keySymFromUnicode(ch))), "Letter not consumed");
}
} // namespace

int main(int argc, char** args) {
    require(argc == 2, "Expected addon module path");
    const auto closeModule = [](void* handle) { dlclose(handle); };
    std::unique_ptr<void, decltype(closeModule)> module(dlopen(args[1], RTLD_NOW | RTLD_LOCAL), closeModule);
    if (!module) throw std::runtime_error(dlerror());
    using FactoryFunction = fcitx::AddonFactory* (*)();
    auto factory = reinterpret_cast<FactoryFunction>(dlsym(module.get(), "fcitx_addon_factory_instance"));
    require(factory != nullptr, "Missing Fcitx factory export");
    char name[] = "openkey-test";
    char* argv[] = {name, nullptr};
    fcitx::Instance instance(1, argv);
    std::unique_ptr<fcitx::AddonInstance> addonInstance(factory()->create(&instance.addonManager()));
    auto* engine = dynamic_cast<fcitx::InputMethodEngine*>(addonInstance.get());
    require(engine != nullptr, "Module is not an input method");
    auto& addon = *engine;
    Editor first(instance.inputContextManager());
    Editor second(instance.inputContextManager());
    fcitx::InputMethodEntry telex("openkey-telex", "OpenKey Telex", "vi", "openkey");
    fcitx::InputMethodEntry vni("openkey-vni", "OpenKey VNI", "vi", "openkey");
    fcitx::InputContextEvent activateFirst(&first, fcitx::EventType::InputContextFocusIn);
    fcitx::InputContextEvent activateSecond(&second, fcitx::EventType::InputContextFocusIn);
    addon.activate(telex, activateFirst);
    addon.activate(vni, activateSecond);
    type(addon, telex, first, "tieengs");
    type(addon, vni, second, "vie6t5");
    require(first.preedit() == "tiếng" && second.preedit() == "việt", "Context isolation");
    require(!key(addon, telex, first, fcitx::Key(FcitxKey_s), true), "Key release consumed");
    require(key(addon, telex, first, fcitx::Key(FcitxKey_BackSpace)), "Backspace not consumed");
    require(first.preedit() == "tiến", "Backspace preedit");
    type(addon, telex, first, "g");
    require(!key(addon, telex, first, fcitx::Key(FcitxKey_space)), "Space must pass through");
    require(first.committed == "tiếng" && first.preedit().empty(), "Space commit");
    require(key(addon, vni, second, fcitx::Key(FcitxKey_Escape)), "Escape not consumed");
    require(second.preedit().empty() && second.committed.empty(), "Escape must cancel");
    type(addon, telex, first, "a");
    require(!key(addon, telex, first, fcitx::Key(FcitxKey_Shift_L)), "Shift consumed");
    require(first.preedit() == "a", "Modifier committed text");
    require(!key(addon, telex, first, fcitx::Key(FcitxKey_c, fcitx::KeyState::Ctrl)), "Ctrl shortcut consumed");
    require(first.committed == "tiếnga", "Shortcut must commit");
    type(addon, telex, first, "hoas");
    require(!key(addon, telex, first, fcitx::Key(FcitxKey_Left)), "Navigation consumed");
    require(first.preedit().empty() && first.committed == "tiếngahoá", "Navigation commit");
    type(addon, telex, first, "vieetj");
    addon.deactivate(telex, activateFirst);
    require(first.preedit().empty() && first.committed == "tiếngahoáviệt", "Deactivate commit");
    type(addon, telex, first, "a");
    addon.reset(telex, activateFirst);
    require(first.preedit().empty(), "Reset clears preedit");
    first.setCapabilityFlags(fcitx::CapabilityFlag::Password);
    require(!key(addon, telex, first, fcitx::Key(FcitxKey_a)), "Password key consumed");
    require(first.preedit().empty(), "Password preedit populated");
    first.setCapabilityFlags(fcitx::CapabilityFlag::Sensitive);
    require(!key(addon, telex, first, fcitx::Key(FcitxKey_a)), "Sensitive key consumed");
    first.setCapabilityFlags(fcitx::CapabilityFlag::Preedit);
    type(addon, telex, first, "secret");
    const auto previous = first.committed;
    first.setCapabilityFlags(fcitx::CapabilityFlag::Sensitive);
    addon.deactivate(telex, activateFirst);
    require(first.committed == previous && first.preedit().empty(), "Sensitive deactivate leaked composition");
    std::cout << "Fcitx5 addon: preedit, commit, context, lifecycle and password checks passed\n";
}
