// Runs the real engine without installing hooks or changing user settings.
#include "../Sources/OpenKey/engine/Engine.h"
#include "../Sources/OpenKey/engine/CaretToneEdit.h"
#include "../Sources/OpenKey/win32/OpenKey/OpenKey/CaretSettingsProtocol.h"
#include "../Sources/OpenKey/win32/OpenKey/OpenKey/KeyboardCaseState.h"
#include "../Sources/OpenKey/win32/OpenKey/OpenKey/TsfTyping.h"
#include <iostream>
#include <stdexcept>

int vLanguage = 1;
int vInputType = vTelex;
int vFreeMark = 1;
int vCodeTable = 0;
int vSwitchKeyStatus = 0;
int vCheckSpelling = 1;
int vUseModernOrthography = 1;
int vQuickTelex = 0;
int vRestoreIfWrongSpelling = 0;
int vFixRecommendBrowser = 0;
int vUseMacro = 0;
int vUseMacroInEnglishMode = 0;
int vAutoCapsMacro = 0;
int vUseSmartSwitchKey = 0;
int vUpperCaseFirstChar = 0;
int vTempOffSpelling = 0;
int vAllowConsonantZFWJ = 0;
int vQuickStartConsonant = 0;
int vQuickEndConsonant = 0;
int vRememberCode = 0;
int vOtherLanguage = 0;
int vTempOffOpenKey = 0;

// '|' simulates Caps Lock down/up without inserting a visible character.
std::wstring type(const char* input, const int inputType, const int modern, bool caps = false) {
    vInputType = inputType;
    vUseModernOrthography = modern;
    auto* state = static_cast<vKeyHookState*>(vKeyInit());
    startNewSession();
    std::wstring output;
    for (const char* key = input; *key; ++key) {
        if (*key == '|') {
            // Same routing policy as the Windows hook, after hotkey handling.
            if (!OpenKeyKeyboardCase::IsCaseToggleKey(VK_CAPITAL)) {
                vKeyHandleEvent(Keyboard, KeyDown, VK_CAPITAL, caps ? 2 : 0);
            }
            caps = !caps;
            continue;
        }
        const Uint16 code = *key >= 'a' && *key <= 'z' ? *key - 'a' + KEY_A : *key;
        vKeyHandleEvent(Keyboard, KeyDown, code, caps ? 2 : 0);
        const wchar_t visible = caps && *key >= 'a' && *key <= 'z'
            ? static_cast<wchar_t>(*key - 'a' + 'A') : static_cast<wchar_t>(*key);
        if (state->code == vDoNothing) {
            output.push_back(visible);
        } else if (state->code == vWillProcess || state->code == vRestore ||
                   state->code == vRestoreAndStartNewSession) {
            if (state->backspaceCount > output.size()) {
                throw std::runtime_error("Invalid backspace count");
            }
            output.resize(output.size() - state->backspaceCount);
            for (int index = state->newCharCount - 1; index >= 0; --index) {
                const Uint32 character = state->charData[index];
                output.push_back(static_cast<wchar_t>(character & CHAR_CODE_MASK
                    ? character & 0xFFFF : keyCodeToCharacter(character)));
            }
            if (state->code == vRestore || state->code == vRestoreAndStartNewSession) {
                output.push_back(visible);
            }
        } else {
            throw std::runtime_error("Unexpected engine action");
        }
    }
    return output;
}

int main() {
    if (!OpenKeyKeyboardCase::IsCaseToggleKey(VK_CAPITAL) ||
        OpenKeyKeyboardCase::IsCaseToggleKey(KEY_A) ||
        OpenKeyKeyboardCase::IsCaseToggleKey(KEY_SPACE) ||
        !OpenKeyKeyboardCase::CapsLockEnabled(1) ||
        !OpenKeyKeyboardCase::CapsLockEnabled(static_cast<SHORT>(0x8001)) ||
        OpenKeyKeyboardCase::CapsLockEnabled(0) ||
        OpenKeyKeyboardCase::CapsLockEnabled(static_cast<SHORT>(0x8000))) {
        throw std::runtime_error("Invalid Caps Lock routing/toggle state");
    }
    vKeyHookState initialState;
    if (initialState.code != vDoNothing || initialState.backspaceCount != 0 ||
        initialState.newCharCount != 0 || initialState.extCode != 0 ||
        !initialState.macroKey.empty() || !initialState.macroData.empty()) {
        throw std::runtime_error("Invalid initial hook state");
    }
    for (const auto character : initialState.charData) {
        if (character != 0) {
            throw std::runtime_error("Uninitialized character buffer");
        }
    }
    struct Case { const char* input; const wchar_t* expected; int inputType; int modern; };
    const Case cases[] = {
        { "nias", L"nía", vTelex, 1 }, { "mias", L"mía", vTelex, 1 },
        { "muas", L"múa", vTelex, 1 }, { "gias", L"giá", vTelex, 1 },
        { "quas", L"quá", vTelex, 1 }, { "hoas", L"hoá", vTelex, 1 },
        { "hoas", L"hóa", vTelex, 0 }, { "tieengs", L"tiếng", vTelex, 1 },
        { "vieetj", L"việt", vTelex, 1 }, { "dduowngf", L"đường", vTelex, 1 },
        { "tuowngr", L"tưởng", vTelex, 1 }, { "cuusw", L"cứu", vTelex, 1 },
        { "nia1", L"nía", vVNI, 1 }, { "mua1", L"múa", vVNI, 1 },
        { "gia1", L"giá", vVNI, 1 }, { "qua1", L"quá", vVNI, 1 },
        { "hoa1", L"hoá", vVNI, 1 }, { "hoa1", L"hóa", vVNI, 0 },
        { "tie6ng1", L"tiếng", vVNI, 1 }, { "vie6t5", L"việt", vVNI, 1 }
        , { "laapj", L"l\u1eadp", vTelex, 0 }, { "coongj", L"c\u1ed9ng", vTelex, 0 }
        , { "xax", L"x\u00e3", vTelex, 0 }, { "hoaf", L"h\u00f2a", vTelex, 0 }, { "hoaf", L"ho\u00e0", vTelex, 1 }
    };
    int failures = 0;
    for (const auto& item : cases) {
        const auto actual = type(item.input, item.inputType, item.modern);
        if (actual != item.expected) {
            std::cerr << "FAIL: " << item.input << " type=" << item.inputType
                      << " modern=" << item.modern << " got=" << wideStringToUtf8(actual)
                      << " expected=" << wideStringToUtf8(item.expected) << '\n';
            ++failures;
        }
    }
    std::cout << "Engine regression: " << (sizeof(cases) / sizeof(cases[0]) - failures)
              << "/" << sizeof(cases) / sizeof(cases[0]) << " passed\n";
    struct CapsCase { const char* input; const wchar_t* expected; int inputType; bool initialCaps; };
    const CapsCase capsCases[] = {
        { "|tieengs", L"TIẾNG", vTelex, false },
        { "|tieengs", L"tiếng", vTelex, true },
        { "tieeng|s", L"tiếng", vTelex, false },
        { "tieeng|s", L"TIẾNG", vTelex, true },
        { "ti|eengs", L"tiẾNG", vTelex, false },
        { "ti|eengs", L"TIếng", vTelex, true },
        { "tiee|ng|s", L"tiếNG", vTelex, false },
        { "|tie6ng1", L"TIẾNG", vVNI, false },
        { "|tie6ng1", L"tiếng", vVNI, true },
        { "tie6ng|1", L"tiếng", vVNI, false },
        { "tie6ng|1", L"TIẾNG", vVNI, true },
        { "ti|e6ng1", L"tiẾNG", vVNI, false },
        { "ti|e6ng1", L"TIếng", vVNI, true },
        { "tie6|ng|1", L"tiếNG", vVNI, false },
        { "tieengs |vieetj", L"tiếng VIỆT", vTelex, false },
        { "tieengs |vieetj", L"TIẾNG việt", vTelex, true },
        { "|tieengs", L"TIẾNG", vSimpleTelex1, false },
        { "|tieengs", L"tiếng", vSimpleTelex1, true },
        { "tieeng|s", L"tiếng", vSimpleTelex1, false },
        { "tieeng|s", L"TIẾNG", vSimpleTelex1, true },
        { "|tieengs", L"TIẾNG", vSimpleTelex2, false },
        { "|tieengs", L"tiếng", vSimpleTelex2, true },
        { "tieeng|s", L"tiếng", vSimpleTelex2, false },
        { "tieeng|s", L"TIẾNG", vSimpleTelex2, true }
    };
    int capsFailures = 0;
    for (const auto& item : capsCases) {
        const auto actual = type(item.input, item.inputType, 1, item.initialCaps);
        if (actual != item.expected) {
            std::cerr << "FAIL Caps Lock: " << item.input << " type=" << item.inputType
                      << " initialCaps=" << item.initialCaps << " got=" << wideStringToUtf8(actual)
                      << " expected=" << wideStringToUtf8(item.expected) << '\n';
            ++capsFailures;
        }
    }
    std::cout << "Caps Lock regression: " << (sizeof(capsCases) / sizeof(capsCases[0]) - capsFailures)
              << "/" << sizeof(capsCases) / sizeof(capsCases[0]) << " passed\n";
    failures += capsFailures;
    int caretCases = 0, caretFailures = 0;
    for (const char* raw : {"a", "aw", "aa", "e", "ee", "i", "o", "oo", "ow", "u", "uw", "y",
        "bang", "bawng", "tieeng", "nghieeng", "dduowng", "hoa", "hoan", "qua", "quy", "gia", "gi",
        "mua", "tua", "tuan", "ngoai", "khuyu", "khuya", "ngoa", "cuu", "tuowng"}) {
        for (int modern : {0, 1}) for (bool capital : {false, true}) for (int tone = 1; tone <= 5; ++tone) {
            const auto original = type(raw, vTelex, modern, capital);
            const auto input = std::string(raw) + "sfrxj"[tone - 1];
            const auto expected = type(input.c_str(), vTelex, modern, capital);
            std::wstring edited;
            const bool changed = OpenKeyCaretTone::Edit(original, tone, modern != 0, edited);
            ++caretCases;
            if (!changed || edited != expected) {
                ++caretFailures;
                std::cerr << "FAIL caret parity: " << raw << " tone=" << tone << " modern=" << modern
                    << " got=" << wideStringToUtf8(edited) << " expected=" << wideStringToUtf8(expected) << '\n';
            }
        }
    }
    std::cout << "Caret tone/engine parity: " << caretCases - caretFailures << '/' << caretCases << " passed\n";
    failures += caretFailures;
    struct ShapeCase { const char* raw; char key; };
    const ShapeCase shapes[] = {{"noi", 'o'}, {"noij", 'o'}, {"ten", 'e'}, {"ban", 'a'}, {"bang", 'w'},
        {"bangf", 'w'}, {"tuong", 'w'}, {"tuongs", 'w'}, {"duong", 'd'}, {"duongw", 'd'},
        {"mua", 'w'}, {"mui", 'w'}, {"tuu", 'w'}, {"noi", 'w'}, {"thuong", 'w'}};
    // Bare "thuo" and changes after its existing tone have legacy differences
    // depending on Caps Lock/free-mark history. Caret editing uses independent
    // expected Unicode cases for these, preserving consistent case handling.
    int shapeCases = 0, shapeFailures = 0;
    for (const auto& item : shapes) for (int modern : {0, 1}) for (bool capital : {false, true}) {
        const auto original = type(item.raw, vTelex, modern, capital);
        const auto input = std::string(item.raw) + item.key;
        const auto expected = type(input.c_str(), vTelex, modern, capital);
        std::wstring edited;
        ++shapeCases;
        if (!OpenKeyCaretTone::EditAction(original, OpenKeyCaretTone::EditKey(item.key - 'a' + 'A', vTelex), modern != 0, edited) || edited != expected) {
            ++shapeFailures;
            std::cerr << "FAIL caret shape parity: " << input << " modern=" << modern
                << " got=" << wideStringToUtf8(edited) << " expected=" << wideStringToUtf8(expected) << '\n';
        }
    }
    std::cout << "Caret shape/engine parity: " << shapeCases - shapeFailures << '/' << shapeCases << " passed\n";
    failures += shapeFailures;
    int routingChecks = 0;
    for (const char* input : {"laapj", "coongj", "xax", "hoaf", "aapj"}) {
        OpenKeyCaretRuntime::Runtime runtime;
        const auto window = reinterpret_cast<HWND>(1);
        runtime.profile.Notify(OpenKeyCaretRuntime::Activate, window, 100);
        runtime.intent.Move(window); // A click in the empty editor before typing.
        std::string prefix;
        for (const char* key = input; *key; ++key) {
            const auto vk = static_cast<UINT>(*key - 'a' + 'A');
            if (runtime.Delegate(window, vk, vTelex, true)) { ++failures; std::cerr << "Typing stolen by caret service: " << input << '\n'; }
            ++routingChecks;
            runtime.intent.PassKey(vk, true);
            prefix += *key;
            const auto rendered = type(prefix.c_str(), vTelex, 0);
            // Word/Chromium may advertise text availability and selection
            // changes after every rendered prefix. This is not a user move.
            runtime.profile.Notify(OpenKeyCaretRuntime::Ready, window, 100);
            runtime.profile.Notify(OpenKeyCaretRuntime::Activate, window, 100);
            if (!runtime.profile.Active(window) || rendered.empty()) ++failures;
        }
        runtime.intent.Move(window);
        if (!runtime.Delegate(window, 'Z', vTelex, true)) ++failures;
        ++routingChecks;
    }
    std::cout << "Normal typing/caret handoff: " << routingChecks << " checks completed\n";
    int rangeCases = 0;
    for (int inputMode : {vTelex, vVNI, vSimpleTelex1, vSimpleTelex2}) for (int modern : {0, 1}) for (bool caps : {false, true}) {
        for (const char* input : {"laapj", "coongj", "xax", "hoaf", "cooo", "xaxx", "tieengs", "dduowngf", "aapj", "no65", "tie6ng1"}) {
            const auto expected = type(input, inputMode, modern, caps);
            vInputType = inputMode; vUseModernOrthography = modern;
            auto* state = static_cast<vKeyHookState*>(vKeyInit()); startNewSession();
            std::wstring actual;
            for (const char* key = input; *key; ++key) {
                const UINT vk = *key >= 'a' && *key <= 'z' ? *key - 'a' + 'A' : *key;
                vKeyHandleEvent(Keyboard, KeyDown, static_cast<Uint16>(vk), caps ? 2 : 0);
                const auto edit = OpenKeyTsfTyping::Convert(*state, vk, caps ? 2 : 0);
                if (!edit.changed) actual += static_cast<wchar_t>(caps && *key >= 'a' && *key <= 'z' ? *key - 'a' + 'A' : *key);
                else if (edit.removed <= actual.size()) {
                    actual.resize(actual.size() - edit.removed); actual += edit.inserted;
                } else ++failures;
            }
            if (actual != expected) { ++failures; std::cerr << "FAIL TSF range typing parity: " << input << '\n'; }
            ++rangeCases;
        }
    }
    std::cout << "TSF range typing/legacy engine parity: " << rangeCases << " checks completed\n";
    int bridgeChecks = 0;
    for (const char* input : {"laapj", "coongj", "xax", "hoaf", "cooo", "xaxx"}) {
        const auto expected = type(input, vTelex, 0);
        auto* state = static_cast<vKeyHookState*>(vKeyInit()); startNewSession();
        OpenKeyTsfTyping::Bridge bridge;
        const auto window = reinterpret_cast<HWND>(1);
        std::wstring actual;
        // Queue a complete burst before the editor processes any key. This
        // catches single-slot IPC implementations that lose fast keystrokes.
        for (const char* key = input; *key; ++key) bridge.Forward(window, *key - 'a' + 'A', 0);
        for (const char* key = input; *key; ++key) {
            const auto vk = *key - 'a' + 'A';
            if (bridge.Handle(0, window, vk, state) != 1 || bridge.Handle(0, window, vk, state) != 1) ++failures;
            const auto packet = static_cast<ULONG_PTR>(bridge.Handle(1, window, vk, state));
            if ((packet & 0xc0000000u) != 0x40000000u) ++failures;
            if (packet & 0x200000u) {
                const auto removed = (packet >> 13) & 255;
                std::wstring inserted;
                for (unsigned i = 0; i < (packet & 8191); ++i) {
                    const auto ch = bridge.Handle(2, window, MAKELPARAM(i, (packet >> 22) & 255), state);
                    if (!ch) ++failures; else inserted += static_cast<wchar_t>(ch - 1);
                }
                if (removed > actual.size()) ++failures;
                else { actual.resize(actual.size() - removed); actual += inserted; }
                if (bridge.Handle(2, window, MAKELPARAM(0, (((packet >> 22) & 255) + 1) & 255), state)) ++failures;
            } else actual += static_cast<wchar_t>(*key);
            ++bridgeChecks;
        }
        if (actual != expected || bridge.Handle(1, window, 'A', state)) ++failures;
        bridge.Forward(window, 'L', 0); bridge.Handle(3, window, 0, state);
        if (bridge.Handle(0, window, 'L', state)) ++failures;
        ++bridgeChecks;
    }
    std::cout << "TSF queued engine IPC: " << bridgeChecks << " checks completed\n";
    return failures ? 1 : 0;
}
