#include "Engine.h"
#include "Macro.h"

#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

wchar_t character(Uint32 value) {
    if ((value & CHAR_CODE_MASK) || (value & PURE_CHARACTER_MASK)) {
        return static_cast<wchar_t>(value & 0xffff);
    }
    return static_cast<wchar_t>(keyCodeToCharacter(value));
}

Uint16 keyCode(char input) {
    static constexpr Uint16 letters[] = {
        KEY_A, KEY_B, KEY_C, KEY_D, KEY_E, KEY_F, KEY_G,
        KEY_H, KEY_I, KEY_J, KEY_K, KEY_L, KEY_M, KEY_N,
        KEY_O, KEY_P, KEY_Q, KEY_R, KEY_S, KEY_T, KEY_U,
        KEY_V, KEY_W, KEY_X, KEY_Y, KEY_Z,
    };
    if (input >= 'a' && input <= 'z') {
        return letters[input - 'a'];
    }
    if (input >= '0' && input <= '9') {
        static constexpr Uint16 digits[] = {
            KEY_0, KEY_1, KEY_2, KEY_3, KEY_4,
            KEY_5, KEY_6, KEY_7, KEY_8, KEY_9,
        };
        return digits[input - '0'];
    }
    if (input == ' ') {
        return KEY_SPACE;
    }
    throw std::runtime_error("Unsupported test input");
}

void erase(std::wstring& output, std::size_t count) {
    if (count > output.size()) {
        throw std::runtime_error("Engine requested too many backspaces");
    }
    output.resize(output.size() - count);
}

std::wstring type(const std::string& input) {
    auto* hook = static_cast<vKeyHookState*>(vKeyInit());
    startNewSession();
    std::wstring output;

    for (const char token : input) {
        if (token == '<') {
            vKeyHandleEvent(Keyboard, KeyDown, KEY_DELETE);
            if (hook->extCode != 2 || output.empty()) {
                throw std::runtime_error("Invalid Backspace state");
            }
            output.pop_back();
            continue;
        }

        const Uint16 key = keyCode(token);
        vKeyHandleEvent(Keyboard, KeyDown, key);

        if (hook->code == vDoNothing) {
            output.push_back(token);
            continue;
        }

        erase(output, hook->backspaceCount);
        if (hook->code == vReplaceMaro) {
            for (const Uint32 value : hook->macroData) {
                output.push_back(character(value));
            }
            output.push_back(token);
            continue;
        }

        if (hook->code != vWillProcess && hook->code != vRestore &&
            hook->code != vRestoreAndStartNewSession) {
            throw std::runtime_error("Unexpected engine action");
        }
        for (int index = hook->newCharCount - 1; index >= 0; --index) {
            output.push_back(character(hook->charData[index]));
        }
        if (hook->code == vRestore || hook->code == vRestoreAndStartNewSession) {
            output.push_back(token);
        }
    }
    return output;
}

bool expect(const char* label, const std::string& input, const wchar_t* expected) {
    const std::wstring actual = type(input);
    if (actual == expected) {
        return true;
    }
    std::cerr << label << " failed for '" << input << "': got '"
              << wideStringToUtf8(actual) << "', expected '"
              << wideStringToUtf8(expected) << "'\n";
    return false;
}

} // namespace

int main() {
    vLanguage = 1;
    vInputType = vTelex;
    vCodeTable = 0;
    vFreeMark = 1;
    vCheckSpelling = 1;
    vUseModernOrthography = 1;
    vRestoreIfWrongSpelling = 0;
    vUseMacro = 0;

    int failures = 0;
    const std::vector<std::pair<std::string, const wchar_t*>> telexCases{
        {"tieengs", L"tiếng"},
        {"vieetj", L"việt"},
        {"dduowngf", L"đường"},
        {"tuowngr", L"tưởng"},
        {"hoas", L"hoá"},
    };
    for (const auto& [input, expected] : telexCases) {
        if (!expect("Telex", input, expected)) {
            ++failures;
        }
    }

    if (!expect("Backspace and retype", "tieengs<g", L"tiếng")) {
        ++failures;
    }
    if (!expect("Backspace preserves tone", "vieetj<t", L"việt")) {
        ++failures;
    }

    vUseMacro = 1;
    addMacro("ok", "đồng ý");
    if (!expect("Unicode macro", "ok ", L"đồng ý ")) {
        ++failures;
    }
    deleteMacro("ok");

    if (failures != 0) {
        std::cerr << failures << " Linux engine regression test(s) failed\n";
        return 1;
    }
    std::cout << "Linux engine regression tests passed\n";
    return 0;
}
