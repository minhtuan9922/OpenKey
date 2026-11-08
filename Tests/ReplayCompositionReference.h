#pragma once

#include "Engine.h"
#include <stdexcept>
#include <string>
#include <vector>

namespace openkey::test {

// Frozen alpha.49 replay oracle for differential tests/benchmarks only.
// Production typing must not depend on this implementation.
class ReplayReference {
public:
    static constexpr std::size_t MaxEvents = 256;

    struct Options {
        bool checkSpelling = true;
        bool useModernOrthography = true;
        bool quickTelex = false;
    };

    explicit ReplayReference(int mode = vTelex) : ReplayReference(mode, Options{}) {}
    ReplayReference(int mode, Options options) : mode_(mode), options_(options) {}
    void setOptions(Options options) {
        options_ = options;
        if (!events_.empty()) render();
    }
    bool empty() const { return text_.empty(); }
    bool full() const { return events_.size() >= MaxEvents; }
    const std::string& text() const { return text_; }
    void clear() { events_.clear(); text_.clear(); }

    // ASCII symbols, not physical X11/evdev keycodes. The engine's Linux key
    // constants are mapped explicitly, so this works on Wayland as well.
    bool append(char symbol) {
        if (full()) return false;
        Uint16 code = 0;
        bool caps = symbol >= 'A' && symbol <= 'Z';
        char lower = caps ? static_cast<char>(symbol - 'A' + 'a') : symbol;
        static constexpr Uint16 letters[] = {
            KEY_A, KEY_B, KEY_C, KEY_D, KEY_E, KEY_F, KEY_G, KEY_H,
            KEY_I, KEY_J, KEY_K, KEY_L, KEY_M, KEY_N, KEY_O, KEY_P,
            KEY_Q, KEY_R, KEY_S, KEY_T, KEY_U, KEY_V, KEY_W, KEY_X, KEY_Y, KEY_Z};
        static constexpr Uint16 digits[] = {
            KEY_0, KEY_1, KEY_2, KEY_3, KEY_4, KEY_5, KEY_6, KEY_7, KEY_8, KEY_9};
        if (lower >= 'a' && lower <= 'z') code = letters[lower - 'a'];
        else if (lower >= '0' && lower <= '9') code = digits[lower - '0'];
        else return false;
        events_.push_back({code, symbol, caps});
        render();
        return true;
    }

    bool backspace() {
        if (empty()) return false;
        if (full()) return false;
        events_.push_back({KEY_DELETE, '\b', false});
        render();
        if (empty()) clear();
        return true;
    }

private:
    struct Stroke { Uint16 code; char visible; bool caps; };
    int mode_;
    Options options_;
    std::vector<Stroke> events_;
    std::string text_;

    void render() {
        vInputType = mode_;
        vQuickTelex = options_.quickTelex && mode_ == vTelex;
        vCodeTable = 0;
        vUseMacro = 0;
        vRestoreIfWrongSpelling = 0;
        vFreeMark = vAllowConsonantZFWJ = vQuickStartConsonant = vQuickEndConsonant = vUpperCaseFirstChar = 0;
        vCheckSpelling = options_.checkSpelling;
        vUseModernOrthography = options_.useModernOrthography;
        auto* state = static_cast<vKeyHookState*>(vKeyInit());
        startNewSession();
        std::wstring output;
        for (const auto& stroke : events_) {
            vKeyHandleEvent(Keyboard, KeyDown, stroke.code, stroke.caps ? 2 : 0);
            if (state->code == vDoNothing) {
                if (stroke.code == KEY_DELETE) {
                    if (!output.empty()) output.pop_back();
                } else output.push_back(stroke.visible);
                continue;
            }
            if (state->backspaceCount > output.size()) {
                throw std::runtime_error("Invalid OpenKey replacement range");
            }
            output.resize(output.size() - state->backspaceCount);
            for (int i = state->newCharCount - 1; i >= 0; --i) {
                const auto ch = state->charData[i];
                output.push_back(static_cast<wchar_t>((ch & CHAR_CODE_MASK)
                    ? (ch & CHAR_MASK) : keyCodeToCharacter(ch)));
            }
            if (state->code == vRestore || state->code == vRestoreAndStartNewSession)
                output.push_back(stroke.visible);
        }
        text_ = wideStringToUtf8(output);
    }
};
} // namespace openkey::test
