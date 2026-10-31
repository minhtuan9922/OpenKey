#pragma once
#include <windows.h>
#include <string>
#include <functional>
#include "../../../engine/CaretToneEdit.h"

namespace OpenKeyCaretTone {
    // The owned WinUI editor advertises that it can handle a tone key itself.
    // The hook then leaves it untouched and resets its old composition buffer.
    constexpr wchar_t OwnedEditorProperty[] = L"OpenKey.CaretToneEdit";
    inline bool Navigation(UINT key) {
        return key == VK_LEFT || key == VK_RIGHT || key == VK_UP || key == VK_DOWN ||
            key == VK_HOME || key == VK_END || key == VK_PRIOR || key == VK_NEXT;
    }
    struct NativeSnapshot {
        HWND editor = nullptr;
        DWORD caret = 0, lineStart = 0;
        std::wstring line;
        Word word;
    };
    inline bool Message(HWND window, UINT message, WPARAM w, LPARAM l, DWORD_PTR& result) {
        return SendMessageTimeoutW(window, message, w, l, SMTO_ABORTIFHUNG | SMTO_BLOCK, 8, &result) != 0;
    }
    inline HWND FocusedEditor(HWND foreground) {
        GUITHREADINFO info{sizeof(info)};
        const DWORD thread = GetWindowThreadProcessId(foreground, nullptr);
        if (!thread || !GetGUIThreadInfo(thread, &info) || !info.hwndFocus ||
            GetAncestor(info.hwndFocus, GA_ROOT) != foreground) return nullptr;
        return info.hwndFocus;
    }
    inline bool ReadNative(HWND editor, NativeSnapshot& output) {
        wchar_t name[64]{};
        if (!editor || !IsWindowEnabled(editor) || !GetClassNameW(editor, name, 64) ||
            (_wcsicmp(name, L"Edit") && _wcsnicmp(name, L"RichEdit", 8)) ||
            (GetWindowLongPtrW(editor, GWL_STYLE) & (ES_PASSWORD | ES_READONLY))) return false;
        DWORD_PTR mask = 0, lineIndex = 0, lineStart = 0, lineLength = 0, copied = 0, ignored = 0;
        DWORD start = 0, end = 0;
        if (!Message(editor, EM_GETPASSWORDCHAR, 0, 0, mask) || mask ||
            !Message(editor, EM_GETSEL, reinterpret_cast<WPARAM>(&start), reinterpret_cast<LPARAM>(&end), ignored) ||
            start != end || start > 0x7fffffff ||
            !Message(editor, EM_LINEFROMCHAR, start, 0, lineIndex) ||
            !Message(editor, EM_LINEINDEX, lineIndex, 0, lineStart) || lineStart > start ||
            !Message(editor, EM_LINELENGTH, start, 0, lineLength) || !lineLength || lineLength > 4096) return false;
        // EM_GETLINE reads only the caret's line, never the complete document.
        std::vector<wchar_t> buffer(static_cast<size_t>(lineLength) + 3);
        *reinterpret_cast<WORD*>(buffer.data()) = static_cast<WORD>(buffer.size() - 1);
        if (!Message(editor, EM_GETLINE, lineIndex, reinterpret_cast<LPARAM>(buffer.data()), copied) || copied > lineLength + 2) return false;
        // RichEdit includes a paragraph terminator in EM_GETLINE, while
        // EM_LINELENGTH and the standard Edit control exclude it.
        while (copied && (buffer[copied - 1] == L'\r' || buffer[copied - 1] == L'\n')) --copied;
        if (copied != lineLength) return false;
        NativeSnapshot next; next.editor = editor; next.caret = start; next.lineStart = static_cast<DWORD>(lineStart);
        next.line.assign(buffer.data(), static_cast<size_t>(copied));
        if (!Around(next.line, start - lineStart, next.word)) return false;
        // EM_GETLINE uses visual lines. Do not mistake part of a word at a
        // soft-wrap boundary for a complete word (hard breaks have extra CPs).
        if (lineIndex && next.word.start == 0) {
            DWORD_PTR previousStart = 0, previousLength = 0;
            if (!Message(editor, EM_LINEINDEX, lineIndex - 1, 0, previousStart) ||
                !Message(editor, EM_LINELENGTH, previousStart, 0, previousLength) ||
                previousStart + previousLength == lineStart) return false;
        }
        if (next.word.start + next.word.text.size() == next.line.size()) {
            DWORD_PTR followingStart = 0;
            if (!Message(editor, EM_LINEINDEX, lineIndex + 1, 0, followingStart) ||
                followingStart == lineStart + copied) return false;
        }
        DWORD verifyStart = 0, verifyEnd = 0;
        if (!Message(editor, EM_GETSEL, reinterpret_cast<WPARAM>(&verifyStart), reinterpret_cast<LPARAM>(&verifyEnd), ignored) ||
            verifyStart != start || verifyEnd != end) return false;
        output = std::move(next); return true;
    }
    inline bool ReplaceNative(const NativeSnapshot& original, const std::wstring& replacement,
        const std::function<bool()>& valid) {
        if (replacement.size() != original.word.text.size() || replacement == original.word.text || !valid()) return false;
        NativeSnapshot current;
        if (!ReadNative(original.editor, current) || current.caret != original.caret ||
            current.lineStart != original.lineStart || current.line != original.line || !valid()) return false;
        size_t changed = 0;
        while (changed < replacement.size() && replacement[changed] == original.word.text[changed]) ++changed;
        size_t limit = replacement.size();
        while (limit > changed && replacement[limit - 1] == original.word.text[limit - 1]) --limit;
        const DWORD begin = original.lineStart + static_cast<DWORD>(original.word.start + changed);
        DWORD_PTR ignored = 0;
        // Replace only the changed span (e.g. both vowels of uo -> horn u/o),
        // retaining other characters' format.
        // One replacement is one undo action; preserve the original caret.
        if (!Message(original.editor, EM_SETSEL, begin, begin + limit - changed, ignored)) return false;
        if (!valid()) { Message(original.editor, EM_SETSEL, original.caret, original.caret, ignored); return false; }
        const auto glyphs = replacement.substr(changed, limit - changed);
        if (!Message(original.editor, EM_REPLACESEL, TRUE, reinterpret_cast<LPARAM>(glyphs.c_str()), ignored)) {
            Message(original.editor, EM_SETSEL, original.caret, original.caret, ignored); return false;
        }
        Message(original.editor, EM_SETSEL, original.caret, original.caret, ignored);
        return true;
    }
    inline bool TryNative(int tone, bool modern) {
        const HWND foreground = GetForegroundWindow(), editor = FocusedEditor(foreground);
        const auto valid = [&] { return foreground == GetForegroundWindow() && FocusedEditor(foreground) == editor; };
        NativeSnapshot snapshot; std::wstring replacement;
        return editor && ReadNative(editor, snapshot) && EditAction(snapshot.word.text, tone, modern, replacement) &&
            ReplaceNative(snapshot, replacement, valid);
    }
    struct Routing {
        bool armed = false;
        bool swallowed[256]{};
        HWND foreground = nullptr;
        void Move(HWND window) { armed = true; foreground = window; }
        void Clear() { armed = false; foreground = nullptr; }
        bool Key(UINT key, bool down, bool eligible, int inputType, bool modern, HWND window,
            const std::function<bool(int, bool)>& edit, bool& changed) {
            changed = false;
            if (key < 256 && swallowed[key]) { if (!down) swallowed[key] = false; return true; }
            if (!eligible || window != foreground) Clear();
            if (Navigation(key)) { if (down) Clear(); else if (eligible) Move(window); return false; }
            if (!down) return false;
            const int tone = EditKey(key, inputType);
            if (armed && eligible && tone >= 0 && edit(tone, modern)) {
                if (key < 256) swallowed[key] = true;
                changed = true; return true;
            }
            // Modifier keys do not insert text; all other unhandled keys end
            // the edit session and continue through the normal typing engine.
            if (key != VK_SHIFT && key != VK_LSHIFT && key != VK_RSHIFT && key != VK_CAPITAL) Clear();
            return false;
        }
    };
}
