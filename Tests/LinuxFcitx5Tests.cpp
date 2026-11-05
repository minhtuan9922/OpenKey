// Exercise the real addon with a fake application frontend. No hooks, desktop
// session, daemon restart, or user input-method configuration is needed.
#include <fcitx/addonfactory.h>
#include <fcitx/addonmanager.h>
#include <fcitx/inputcontext.h>
#include <fcitx/inputcontextmanager.h>
#include <fcitx/inputmethodengine.h>
#include <fcitx/inputpanel.h>
#include <fcitx/instance.h>
#include <fcitx-config/iniparser.h>
#include <cstdlib>
#include <dlfcn.h>
#include <filesystem>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <vector>

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
    const char* frontend() const override { return frontendName; }
    const char* frontendName = "openkey-test";
    std::vector<std::string> commits;
    bool clientPreeditSeen = false;
    std::string committed;
    std::string preedit() { return inputPanel().clientPreedit().toString(); }
private:
    void commitStringImpl(const std::string& text) override { committed += text; commits.push_back(text); }
    void deleteSurroundingTextImpl(int, unsigned int) override {
        throw std::runtime_error("Must not delete committed text");
    }
    void forwardKeyImpl(const fcitx::ForwardKeyEvent&) override {
        throw std::runtime_error("Must not synthesize keys");
    }
    void updatePreeditImpl() override { clientPreeditSeen |= !preedit().empty(); }
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
    const auto* configHome = std::getenv("XDG_CONFIG_HOME");
    require(configHome && *configHome, "Tests require an isolated XDG_CONFIG_HOME");
    std::filesystem::remove(std::filesystem::path(configHome) / "fcitx5/conf/openkey.conf");
    std::unique_ptr<fcitx::AddonInstance> addonInstance(factory()->create(&instance.addonManager()));
    auto* engine = dynamic_cast<fcitx::InputMethodEngine*>(addonInstance.get());
    require(engine != nullptr, "Module is not an input method");
    auto& addon = *engine;
    const auto configValue = [&addon](const char* path) {
        fcitx::RawConfig raw;
        addon.getConfig()->save(raw);
        const auto* value = raw.valueByPath(path);
        require(value, "Missing configuration key");
        return *value;
    };
    require(configValue("CheckSpelling") == "True" &&
            configValue("UseModernOrthography") == "True", "Composition defaults must stay enabled");
    require(configValue("QuickTelex") == "False", "QuickTelex must default to false");
    fcitx::RawConfig description;
    addon.getConfig()->dumpDescription(description);
    for (const char* path : {"InputConfig/CheckSpelling/Type", "InputConfig/UseModernOrthography/Type", "InputConfig/QuickTelex/Type"})
        require(description.valueByPath(path) && *description.valueByPath(path) == "Boolean",
                "New options must expose Boolean schema types");
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

    // Both active, already-empty contexts and pending words must receive new
    // options without activation. Retain the input mode of each context.
    first.setCapabilityFlags(fcitx::CapabilityFlag::Preedit);
    addon.activate(telex, activateFirst);
    addon.activate(vni, activateSecond);
    type(addon, telex, first, "hoas");
    const auto beforeChange = first.committed;
    fcitx::RawConfig partial;
    partial.setValueByPath("UseModernOrthography", "False");
    partial.setValueByPath("DirectDiagnostics", "True");
    partial.setValueByPath("Future/UnknownOption", "keep-me");
    second.inputPanel().setClientPreedit(fcitx::Text("other input method"));
    addon.setConfig(partial);
    require(first.committed == beforeChange + "hoá" && first.preedit().empty(),
            "Option change must commit old preedit once using old options");
    require(second.preedit() == "other input method", "Option update cleared an idle context's panel");
    require(configValue("CheckSpelling") == "True" && configValue("InputStyle") == "Preedit",
            "Partial update reset unrelated options");
    type(addon, telex, first, "hoas");
    type(addon, vni, second, "hoa1");
    require(first.preedit() == "hóa" && second.preedit() == "hóa",
            "Active pending/idle contexts did not receive classic options");

    partial.removeAll();
    partial.setValueByPath("CheckSpelling", "False");
    addon.setConfig(partial);
    require(configValue("UseModernOrthography") == "False" && configValue("DirectDiagnostics") == "True",
            "Partial spelling update reset orthography or diagnostics");
    fcitx::RawConfig saved;
    fcitx::readAsIni(saved, "conf/openkey.conf");
    require(saved.valueByPath("Future/UnknownOption") &&
                *saved.valueByPath("Future/UnknownOption") == "keep-me", "Unknown config key lost");
    type(addon, telex, first, "fas");
    type(addon, vni, second, "fa1");
    require(first.preedit() == "fá" && second.preedit() == "fá", "Active spelling disable failed");
    // A diagnostics-only or no-op update must not finish a pending word.
    const auto committedBeforeNoop = first.committed;
    partial.setValueByPath("DirectDiagnostics", "False");
    addon.setConfig(partial);
    require(first.preedit() == "fá" && first.committed == committedBeforeNoop,
            "Unchanged composition options finished a word");

    fcitx::readAsIni(saved, "conf/openkey.conf");
    saved.setValueByPath("CheckSpelling", "True");
    saved.setValueByPath("UseModernOrthography", "True");
    require(fcitx::safeSaveAsIni(saved, "conf/openkey.conf"), "Write reload fixture");
    addon.reloadConfig();
    require(first.committed == committedBeforeNoop + "fá" && first.preedit().empty(),
            "Reload lost or duplicated old preedit");
    type(addon, telex, first, "fas");
    type(addon, vni, second, "fa1");
    require(first.preedit() == "fas" && second.preedit() == "fa1", "Reload did not update active spelling");
    addon.reset(telex, activateFirst);
    addon.reset(vni, activateSecond);
    type(addon, telex, first, "hoas");
    type(addon, vni, second, "hoa1");
    require(first.preedit() == "hoá" && second.preedit() == "hoá", "Reload did not update active orthography");

    // A newly activated context also starts with the configured options.
    partial.removeAll();
    partial.setValueByPath("CheckSpelling", "False");
    partial.setValueByPath("UseModernOrthography", "False");
    addon.setConfig(partial);
    addon.activate(vni, activateSecond);
    type(addon, vni, second, "hoa1");
    require(second.preedit() == "hóa", "Activation ignored configured orthography");
    addon.reset(vni, activateSecond);
    type(addon, vni, second, "fa1");
    require(second.preedit() == "fá", "Activation ignored configured spelling");
    partial.setValueByPath("CheckSpelling", "True");
    partial.setValueByPath("UseModernOrthography", "True");
    addon.setConfig(partial);
    addon.reset(telex, activateFirst);
    addon.reset(vni, activateSecond);
    type(addon, telex, first, "gg");
    require(first.preedit() == "gg", "Default QuickTelex changed Telex");
    const auto beforeQuick = first.committed;
    partial.removeAll();
    partial.setValueByPath("QuickTelex", "True");
    addon.setConfig(partial);
    require(first.committed == beforeQuick + "gg" && first.preedit().empty(),
            "QuickTelex change must finish old preedit exactly once");
    require(configValue("QuickTelex") == "True" && configValue("CheckSpelling") == "True" &&
                configValue("UseModernOrthography") == "True" && configValue("InputStyle") == "Preedit" &&
                configValue("DirectDiagnostics") == "False", "QuickTelex partial update reset fields");
    for (const auto& [input, output] : {
            std::pair{"cc", "ch"}, {"gg", "gi"}, {"kk", "kh"}, {"nn", "ng"},
            {"qq", "qu"}, {"pp", "ph"}, {"tt", "th"}}) {
        type(addon, telex, first, input);
        type(addon, vni, second, input);
        require(first.preedit() == output && second.preedit() == input,
                "QuickTelex pairs or VNI isolation failed without reactivation");
        require(key(addon, telex, first, fcitx::Key(FcitxKey_BackSpace)), "QuickTelex Backspace");
        require(first.preedit() == std::string(output).substr(0, 1), "QuickTelex visible Backspace");
        addon.reset(telex, activateFirst);
        addon.reset(vni, activateSecond);
    }
    type(addon, telex, first, "GG");
    require(first.preedit() == "GI", "QuickTelex capitals");
    const auto beforeReload = first.committed;
    // Persist and reload both values, retaining unknown keys and unrelated fields.
    fcitx::readAsIni(saved, "conf/openkey.conf");
    require(saved.valueByPath("QuickTelex") && *saved.valueByPath("QuickTelex") == "True",
            "QuickTelex true was not persisted");
    for (const char* value : {"False", "True", "False"}) {
        fcitx::readAsIni(saved, "conf/openkey.conf");
        saved.setValueByPath("QuickTelex", value);
        require(fcitx::safeSaveAsIni(saved, "conf/openkey.conf"), "Write QuickTelex reload fixture");
        addon.reloadConfig();
        require(configValue("QuickTelex") == value, "QuickTelex reload failed");
        type(addon, telex, first, "gg");
        type(addon, vni, second, "gg");
        require(first.preedit() == (std::string(value) == "True" ? "gi" : "gg") &&
                    second.preedit() == "gg", "Reload did not reach idle Telex/VNI contexts");
        addon.reset(telex, activateFirst);
        addon.reset(vni, activateSecond);
    }
    require(first.committed == beforeReload + "GI" && first.preedit().empty(),
            "QuickTelex reload duplicated or lost preedit");
    type(addon, telex, first, "gg");
    require(first.preedit() == "gg", "Reload false did not reach active context");
    partial.setValueByPath("QuickTelex", "False");
    addon.setConfig(partial);
    require(first.preedit() == "gg", "No-op QuickTelex update finished preedit");
    fcitx::readAsIni(saved, "conf/openkey.conf");
    require(saved.valueByPath("QuickTelex") && *saved.valueByPath("QuickTelex") == "False" &&
                saved.valueByPath("Future/UnknownOption") &&
                *saved.valueByPath("Future/UnknownOption") == "keep-me" &&
                *saved.valueByPath("CheckSpelling") == "True" &&
                *saved.valueByPath("UseModernOrthography") == "True" &&
                *saved.valueByPath("InputStyle") == "Preedit" &&
                *saved.valueByPath("DirectDiagnostics") == "False",
            "QuickTelex persistence lost false, unknown keys or other fields");
    // Same mode for an ordinary editor and an append-only terminal, including
    // GNOME's ibus bridge where the two can advertise identical capabilities.
    partial.removeAll();
    partial.setValueByPath("InputStyle", "CommitOnly");
    addon.setConfig(partial);
    require(configValue("InputStyle") == "CommitOnly", "CommitOnly config rejected");
    fcitx::RawConfig commitDescription;
    addon.getConfig()->dumpDescription(commitDescription);
    require(commitDescription.valueByPath("InputConfig/InputStyle/Enum/2") &&
        *commitDescription.valueByPath("InputConfig/InputStyle/Enum/2") == "CommitOnly",
        "CommitOnly missing from schema");
    for (const char* frontend : {"openkey-test", "ibus"}) {
        for (const auto flags : {fcitx::CapabilityFlags{},
                fcitx::CapabilityFlags{fcitx::CapabilityFlag::Preedit},
                fcitx::CapabilityFlags{fcitx::CapabilityFlag::Preedit, fcitx::CapabilityFlag::SurroundingText},
                fcitx::CapabilityFlags{fcitx::CapabilityFlag::Preedit, fcitx::CapabilityFlag::Terminal}}) {
            for (const auto* entry : {&telex, &vni}) {
                Editor buffered(instance.inputContextManager());
                buffered.frontendName = frontend;
                buffered.setCapabilityFlags(flags);
                fcitx::InputContextEvent activation(&buffered, fcitx::EventType::InputContextFocusIn);
                addon.activate(*entry, activation);
                const auto input = entry == &telex ? "tieengs" : "tie6ng1";
                type(addon, *entry, buffered, input);
                require(buffered.commits.empty() && buffered.preedit().empty() &&
                    buffered.inputPanel().preedit().toString() == "tiếng", "CommitOnly must buffer in server panel");
                require(!key(addon, *entry, buffered, fcitx::Key(FcitxKey_space)), "CommitOnly Space must stay native");
                require(buffered.commits == std::vector<std::string>{"tiếng"}, "CommitOnly word not committed exactly once");
                require(!key(addon, *entry, buffered, fcitx::Key(FcitxKey_Return)), "CommitOnly Enter must stay native");
                type(addon, *entry, buffered, input);
                require(key(addon, *entry, buffered, fcitx::Key(FcitxKey_BackSpace)), "Buffered Backspace bypassed");
                require(buffered.inputPanel().preedit().toString() == "tiến" && buffered.commits.size() == 1,
                    "Buffered Backspace changed committed text");
                require(key(addon, *entry, buffered, fcitx::Key(FcitxKey_Escape)), "Buffered Escape bypassed");
                require(buffered.inputPanel().preedit().toString().empty() && buffered.commits.size() == 1, "Escape committed buffered text");
                require(!key(addon, *entry, buffered, fcitx::Key(FcitxKey_BackSpace)), "Idle Backspace swallowed");
                type(addon, *entry, buffered, input);
                require(!key(addon, *entry, buffered, fcitx::Key(FcitxKey_Tab)), "Buffered Tab swallowed");
                require(buffered.commits.size() == 2, "Tab lost buffered word");
                type(addon, *entry, buffered, input);
                require(!key(addon, *entry, buffered, fcitx::Key(FcitxKey_a, fcitx::KeyState::Ctrl)), "Shortcut swallowed");
                require(buffered.commits.size() == 3, "Shortcut lost buffered word");
                type(addon, *entry, buffered, input);
                addon.reset(*entry, activation);
                require(buffered.commits.size() == 3 && buffered.inputPanel().preedit().toString().empty(), "Reset must cancel buffer");
                type(addon, *entry, buffered, input);
                buffered.setCapabilityFlags(fcitx::CapabilityFlag::Password);
                require(!key(addon, *entry, buffered, fcitx::Key(FcitxKey_a)), "Sensitive field swallowed a key");
                require(buffered.commits.size() == 3 && buffered.inputPanel().preedit().toString().empty(), "Sensitive field exposed buffer");
                require(!buffered.clientPreeditSeen, "CommitOnly exposed client preedit");
            }
        }
    }
    Editor bounded(instance.inputContextManager());
    fcitx::InputContextEvent boundedActivation(&bounded, fcitx::EventType::InputContextFocusIn);
    addon.activate(telex, boundedActivation);
    const std::string longWord(256, 'b');
    type(addon, telex, bounded, longWord);
    require(bounded.commits.empty(), "Full CommitOnly word committed too early");
    require(!key(addon, telex, bounded, fcitx::Key(FcitxKey_BackSpace)), "Full-buffer Backspace must stay native");
    require(bounded.committed == longWord && bounded.inputPanel().preedit().toString().empty(),
        "Full-buffer Backspace must finish pending word before native edit");
    std::cout << "Fcitx5 addon: configuration, context, lifecycle and CommitOnly frontend matrix passed\n";
}
