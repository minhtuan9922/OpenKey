// Runs the real engine without installing hooks or changing user settings.
#include "../Sources/OpenKey/engine/Engine.h"
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

std::wstring type(const char* input, const int inputType, const int modern) {
    vInputType = inputType;
    vUseModernOrthography = modern;
    auto* state = static_cast<vKeyHookState*>(vKeyInit());
    startNewSession();
    std::wstring output;
    for (const char* key = input; *key; ++key) {
        const Uint16 code = *key >= 'a' && *key <= 'z' ? *key - 'a' + KEY_A : *key;
        vKeyHandleEvent(Keyboard, KeyDown, code);
        if (state->code == vDoNothing) {
            output.push_back(static_cast<wchar_t>(*key));
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
                output.push_back(static_cast<wchar_t>(*key));
            }
        } else {
            throw std::runtime_error("Unexpected engine action");
        }
    }
    return output;
}

int main() {
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
    return failures ? 1 : 0;
}
