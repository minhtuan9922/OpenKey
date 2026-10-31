#pragma once
#include "CaretSettingsProtocol.h"
#include "../../../engine/Engine.h"
#include <deque>

namespace OpenKeyTsfTyping {
    struct Result { unsigned removed = 0; std::wstring inserted; bool changed = false; };
    inline UINT Message(unsigned part) {
        static const UINT values[] = {RegisterWindowMessageW(L"OpenKey.TsfTyping.Test.v1"),
            RegisterWindowMessageW(L"OpenKey.TsfTyping.Key.v1"), RegisterWindowMessageW(L"OpenKey.TsfTyping.Char.v1"),
            RegisterWindowMessageW(L"OpenKey.TsfTyping.Reset.v1"), RegisterWindowMessageW(L"OpenKey.TsfTyping.Boundary.v1")};
        return part < 5 ? values[part] : 0;
    }
    inline bool Key(UINT key) { return (key >= 'A' && key <= 'Z') || (key >= '0' && key <= '9') || key == VK_SPACE || key == VK_BACK; }
    // The same engine result used by the legacy hook, represented as one TSF
    // replacement. No extra placeholder, clipboard paste or injected Backspace.
    inline Result Convert(const vKeyHookState& state, UINT key, unsigned caps) {
        Result result;
        if (state.code == vDoNothing) return result;
        result.changed = true; result.removed = state.backspaceCount;
        const auto append = [&](Uint32 ch) {
            result.inserted += static_cast<wchar_t>((ch & (CHAR_CODE_MASK | PURE_CHARACTER_MASK)) ? ch & 0xffff : keyCodeToCharacter(ch));
        };
        if (state.code == vReplaceMaro) {
            for (const auto ch : state.macroData) append(ch);
            append(key | ((caps == 1 || caps == 2) ? CAPS_MASK : 0));
        } else {
            for (int i = state.newCharCount - 1; i >= 0; --i) append(state.charData[i]);
            if (state.code == vRestore || state.code == vRestoreAndStartNewSession)
                append(key | ((caps == 1 || caps == 2) ? CAPS_MASK : 0));
        }
        return result;
    }
    class Bridge {
        struct Ticket { HWND window; UINT key; unsigned caps; ULONGLONG time; };
        std::deque<Ticket> tickets;
        Result result;
        unsigned serial = 0;
    public:
        void Reset() { tickets.clear(); result = {}; startNewSession(); }
        void Forward(HWND window, UINT key, unsigned caps) {
            if (tickets.size() >= 128 || (!tickets.empty() && tickets.front().window != window)) Reset();
            tickets.push_back({window, key, caps, GetTickCount64()});
        }
        LRESULT Handle(unsigned part, HWND target, LPARAM argument, vKeyHookState* state) {
            if (part == 3) { Reset(); return 1; }
            if (part == 4) { startNewSession(); result = {}; return 1; }
            if (part == 2) {
                const unsigned index = LOWORD(argument), version = HIWORD(argument);
                return version == serial && index < result.inserted.size() ? static_cast<LRESULT>(result.inserted[index]) + 1 : 0;
            }
            if (!Key(static_cast<UINT>(argument))) return 0;
            while (!tickets.empty() && (tickets.front().window != target || tickets.front().key != static_cast<UINT>(argument) ||
                GetTickCount64() - tickets.front().time > 1000)) { tickets.pop_front(); startNewSession(); }
            if (tickets.empty()) return 0;
            if (part == 0) return 1; // Test is repeatable and never advances the engine.
            if (part != 1 || !state) return 0;
            const auto ticket = tickets.front(); tickets.pop_front();
            vKeyHandleEvent(vKeyEvent::Keyboard, vKeyEventState::KeyDown, static_cast<Uint16>(ticket.key), static_cast<Uint8>(ticket.caps), false);
            result = Convert(*state, ticket.key, ticket.caps);
            if (state->code == vRestoreAndStartNewSession) startNewSession();
            if (result.inserted.size() > 8191 || result.removed > 255) { Reset(); return 0; }
            serial = (serial + 1) & 255;
            return 0x40000000u | (serial << 22) | (result.changed ? 0x200000u : 0u) |
                (result.removed << 13) | static_cast<unsigned>(result.inserted.size());
        }
    };
}
