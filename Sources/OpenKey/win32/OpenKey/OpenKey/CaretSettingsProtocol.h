#pragma once
#include <windows.h>
#include "../../../engine/CaretToneEdit.h"

namespace OpenKeyCaretRuntime {
    inline UINT Message() {
        static const UINT value = RegisterWindowMessageW(L"OpenKey.CaretSettings.v2");
        return value;
    }
    enum PresenceEvent { Activate, Ready, Unready, Deactivate, ActiveReady, ActiveUnready, RangeTyping, CaretAccepted, CaretRejected };
    inline UINT PresenceMessage(unsigned kind) {
        static const UINT messages[] = {
            RegisterWindowMessageW(L"OpenKey.CaretTsf.Active.v2"), RegisterWindowMessageW(L"OpenKey.CaretTsf.Ready.v2"),
            RegisterWindowMessageW(L"OpenKey.CaretTsf.Unready.v2"), RegisterWindowMessageW(L"OpenKey.CaretTsf.Inactive.v2"),
            RegisterWindowMessageW(L"OpenKey.CaretTsf.ActiveReady.v2"), RegisterWindowMessageW(L"OpenKey.CaretTsf.ActiveUnready.v2"),
            RegisterWindowMessageW(L"OpenKey.CaretTsf.RangeTyping.v2"), RegisterWindowMessageW(L"OpenKey.CaretTsf.CaretAccepted.v2"),
            RegisterWindowMessageW(L"OpenKey.CaretTsf.CaretRejected.v2")};
        return kind < 9 ? messages[kind] : 0;
    }
    inline ULONG_PTR Encode(bool enabled, int inputType, bool modern, bool caretIntent = false, bool tsfActive = false) {
        if (inputType < 0 || inputType > 3) { enabled = false; inputType = 0; }
        return 0x4f4b0000u | (enabled ? 1u : 0u) | (modern ? 2u : 0u) | (caretIntent ? 4u : 0u) |
            (tsfActive ? 8u : 0u) | (static_cast<ULONG_PTR>(inputType) << 4);
    }
    inline bool Decode(ULONG_PTR packet, bool& enabled, int& inputType, bool& modern, bool* caretIntent = nullptr, bool* tsfActive = nullptr) {
        if ((packet & ~static_cast<ULONG_PTR>(0x3fu)) != 0x4f4b0000u) return false;
        enabled = (packet & 1) != 0; modern = (packet & 2) != 0;
        if (caretIntent) *caretIntent = (packet & 4) != 0;
        if (tsfActive) *tsfActive = (packet & 8) != 0;
        inputType = static_cast<int>((packet >> 4) & 3); return true;
    }
    inline bool Navigation(UINT key) { return key >= VK_PRIOR && key <= VK_DOWN; }
    inline bool Modifier(UINT key) {
        return key == VK_SHIFT || key == VK_LSHIFT || key == VK_RSHIFT || key == VK_CONTROL || key == VK_LCONTROL ||
            key == VK_RCONTROL || key == VK_MENU || key == VK_LMENU || key == VK_RMENU || key == VK_LWIN || key == VK_RWIN ||
            key == VK_CAPITAL || key == VK_NUMLOCK || key == VK_SCROLL;
    }
    struct Intent {
        HWND window = nullptr;
        void Move(HWND target) { window = target; }
        void Clear() { window = nullptr; }
        bool Editing(HWND target) const { return target && target == window; }
        void BeginKey(UINT key, bool down, HWND target) {
            if (Navigation(key)) Move(target);
            if (down && (key == VK_CONTROL || key == VK_LCONTROL || key == VK_RCONTROL || key == VK_MENU ||
                key == VK_LMENU || key == VK_RMENU || key == VK_LWIN || key == VK_RWIN)) Clear();
        }
        void PassKey(UINT key, bool down) { if (down && !Navigation(key) && !Modifier(key)) Clear(); }
    };
    struct Presence {
        HWND window = nullptr;
        DWORD owner = 0;
        bool ready = false;
        bool rangeTyping = false;
        bool Active(HWND target) const { return target && target == window && owner; }
        void Notify(unsigned kind, HWND target, DWORD thread) {
            if (!target || !thread) return;
            if (kind == Activate || kind == ActiveReady || kind == ActiveUnready) {
                if (target != window || thread != owner) { ready = false; rangeTyping = false; }
                window = target; owner = thread;
                if (kind == ActiveReady) ready = true;
                if (kind == ActiveUnready) ready = false;
            } else if (target == window && thread == owner) {
                if (kind == Ready) ready = true;
                if (kind == Unready) ready = false;
                if (kind == RangeTyping) rangeTyping = true;
                if (kind == Deactivate) { window = nullptr; owner = 0; ready = false; rangeTyping = false; }
            }
        }
    };
    struct Runtime {
        Intent intent;
        Presence profile;
        bool Delegate(HWND target, UINT key, int type, bool eligible) const {
            return eligible && intent.Editing(target) && profile.Active(target) && profile.ready &&
                OpenKeyCaretTone::EditKey(key, type) >= 0;
        }
        bool Probe(HWND target, UINT key, int type, bool eligible) const {
            return eligible && intent.Editing(target) && profile.Active(target) && !profile.ready &&
                OpenKeyCaretTone::EditKey(key, type) >= 0;
        }
    };
    inline Runtime& State() { static Runtime value; return value; }
}
