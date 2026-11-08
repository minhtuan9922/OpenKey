#pragma once

#include "Engine.h"
#include "EngineContext.h"
#include <stdexcept>
#include <string>
#include <vector>

namespace openkey {

// Each word owns independent engine state. Append/Backspace process one event;
// history is replayed only when changing options on an unfinished word.
// Table access remains confined to Fcitx's event thread.
class Composition {
public:
    static constexpr std::size_t MaxEvents = 256;

    struct Options {
        bool checkSpelling = true;
        bool useModernOrthography = true;
        bool quickTelex = false;
        bool freeMark = false;
        bool allowConsonantZFWJ = false;
        bool quickStartConsonant = false;
        bool quickEndConsonant = false;
        bool restoreWrongSpelling = false;
    };

    explicit Composition(int mode = vTelex) : Composition(mode, Options{}) {}
    Composition(int mode, Options options) : mode_(mode), options_(options), context_(engineOptions()) {}
    void setOptions(Options options) {
        options_ = options;
        render();
    }
    bool empty() const { return text_.empty(); }
    bool full() const { return events_.size() >= MaxEvents; }
    const std::string& text() const { return text_; }
    std::string finishedText() const {
        auto copy = *this;
        copy.process({KEY_SPACE, 0, false}, true);
        return copy.text();
    }
    void clear() { events_.clear(); text_.clear(); output_.clear(); context_ = EngineContext(engineOptions()); }

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
        process(events_.back());
        return true;
    }

    bool backspace() {
        if (empty()) return false;
        if (full()) return false;
        events_.push_back({KEY_DELETE, '\b', false});
        process(events_.back());
        if (empty()) clear();
        return true;
    }

private:
    struct Stroke { Uint16 code; char visible; bool caps; };
    int mode_;
    Options options_;
    std::vector<Stroke> events_;
    std::string text_;

    std::wstring output_;
    EngineContext::Options engineOptions() const {
        return {mode_, options_.checkSpelling, options_.useModernOrthography, options_.quickTelex,
            options_.freeMark, options_.allowConsonantZFWJ, options_.quickStartConsonant,
            options_.quickEndConsonant, options_.restoreWrongSpelling};
    }
    EngineContext context_;
    void render() {
        context_ = EngineContext(engineOptions());
        output_.clear();
        for (const auto& stroke : events_) process(stroke);
        text_ = wideStringToUtf8(output_);
    }
    void process(const Stroke& stroke, bool boundary = false) {
        const auto* state = &context_.process(stroke.code, stroke.caps);

        if (state->code == vDoNothing) {
            if (boundary) return;
            if (stroke.code == KEY_DELETE) {
                if (!output_.empty()) output_.pop_back();
            } else output_.push_back(stroke.visible);
            text_ = wideStringToUtf8(output_);
            return;
        }
        if (state->backspaceCount > output_.size()) {
            throw std::runtime_error("Invalid OpenKey replacement range");
        }
        output_.resize(output_.size() - state->backspaceCount);
        for (int i = state->newCharCount - 1; i >= 0; --i) {
            const auto ch = state->charData[i];
            output_.push_back(static_cast<wchar_t>((ch & CHAR_CODE_MASK)
                ? (ch & CHAR_MASK) : keyCodeToCharacter(ch)));
        }
        if (!boundary && (state->code == vRestore || state->code == vRestoreAndStartNewSession))
            output_.push_back(stroke.visible);
        text_ = wideStringToUtf8(output_);
    }
};
} // namespace openkey
