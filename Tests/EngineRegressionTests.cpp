// Runs the real engine without installing hooks or changing user settings.
#include "../Sources/OpenKey/engine/Engine.h"
#include "../Sources/OpenKey/win32/OpenKey/OpenKey/KeyboardCaseState.h"
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
    return failures ? 1 : 0;
}
