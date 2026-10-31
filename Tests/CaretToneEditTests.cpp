#define NOMINMAX
#include "../Sources/OpenKey/win32/OpenKey/OpenKey/CaretToneEdit.h"
#include <iostream>
#include <stdexcept>
using namespace OpenKeyCaretTone;
namespace {
    int checks = 0;
    void Check(bool value, const char* message) { if (!value) throw std::runtime_error(message); ++checks; }
    void Pure() {
        struct Case { const wchar_t* source; int tone; bool modern; const wchar_t* expected; };
        const Case cases[] = {
            {L"b\u0103ng", 2, true, L"b\u1eb1ng"}, {L"b\u1eb1ng", 1, true, L"b\u1eafng"},
            {L"b\u1eb1ng", 0, true, L"b\u0103ng"}, {L"b\u1eb1ng", 2, true, L"b\u0103ng"},
            {L"B\u0102NG", 2, true, L"B\u1eb0NG"}, {L"Vi\u1ec7t", 1, true, L"Vi\u1ebft"},
            {L"\u0111\u01b0\u1eddng", 0, true, L"\u0111\u01b0\u01a1ng"},
            {L"hoa", 1, true, L"ho\u00e1"}, {L"hoa", 1, false, L"h\u00f3a"},
            {L"qua", 1, true, L"qu\u00e1"}, {L"gia", 1, false, L"gi\u00e1"},
            {L"gi", 2, true, L"g\u00ec"}, {L"quy", 3, true, L"qu\u1ef7"},
            {L"nghieng", 1, true, L"nghi\u00e9ng"}, {L"ti\u00eang", 1, true, L"ti\u1ebfng"},
            {L"mua", 1, true, L"m\u00faa"}, {L"t\u01b0\u01a1ng", 3, false, L"t\u01b0\u1edfng"},
            {L"khuya", 2, true, L"khu\u1ef3a"}, {L"ngoai", 2, true, L"ngo\u00e0i"}
        };
        for (const auto& item : cases) {
            std::wstring actual;
            Check(Edit(item.source, item.tone, item.modern, actual) && actual == item.expected, "tone edit case");
        }
        for (int row = 0; row < 12; ++row) for (int capital = 0; capital < 2; ++capital)
            for (int from = 0; from < 6; ++from) for (int to = 0; to < 6; ++to) {
                std::wstring source(1, Row(row, capital != 0)[from]), result;
                const bool changed = Edit(source, to, true, result);
                const int expected = from == to ? 0 : to;
                Check(changed == (from != expected) && (!changed || result == std::wstring(1, Row(row, capital != 0)[expected])),
                    "all Unicode vowels preserve shape and case while changing tones");
            }
        for (const auto* invalid : {L"", L"hello", L"test", L"abc", L"cong_nghe", L"bang2", L"ba\u0306ng", L"b\u00e0\u00e0n"}) {
            std::wstring result; Check(!Edit(invalid, 2, true, result), "invalid word is left untouched");
        }
        std::wstring result;
        Check(!Edit(L"b\u0103ng", 0, true, result), "clear tone on unmarked word passes through");
        Check(!Edit(L"t\u1ed1t", 2, true, result), "invalid tone for stop consonant passes through");
        const std::wstring sentence = L"\u0111\u00e2y l\u00e0 n\u1ed9i dung b\u0103ng ti\u1ebfng Vi\u1ec7t";
        const size_t start = sentence.find(L"b\u0103ng");
        for (size_t offset = 0; offset <= 4; ++offset) {
            Word word;
            Check(Around(sentence, start + offset, word) && word.text == L"b\u0103ng" && word.caret == offset && word.start == start,
                "read entire word at every caret position including its edges");
        }
        Word word;
        for (const auto* invalid : {L"bang_abc", L"bang123", L"123bang", L"bang\u0301", L"\u4e00bang"})
            Check(!Around(invalid, 2, word), "do not edit inside identifiers, numbers or decomposed/unknown text");
        Check(!Around(std::wstring(32, L'a'), 16, word), "reject overlong word");
        Check(!Around(L"   ", 1, word) && !Around(L"a", 2, word), "empty and invalid caret");
        Check(ToneKey('F', 0) == 2 && ToneKey('2', 1) == 2 && ToneKey('Z', 3) == 0 && ToneKey('0', 1) == 0 &&
            ToneKey('A', 0) == -1 && ToneKey('F', -1) == -1, "Telex and VNI tone bindings");
    }
    void Route() {
        Routing route; bool changed = false; int calls = 0;
        const auto window = reinterpret_cast<HWND>(1), other = reinterpret_cast<HWND>(2);
        const auto apply = [&](int tone, bool) { ++calls; return tone == 2; };
        Check(!route.Key('F', true, true, 0, true, window, apply, changed) && calls == 0, "normal typing never reads the field");
        route.Move(window);
        Check(route.Key('F', true, true, 0, true, window, apply, changed) && changed && calls == 1, "tone after mouse move is handled");
        Check(route.Key('F', true, true, 0, true, window, apply, changed) && !changed && calls == 1, "held key cannot toggle the tone repeatedly");
        Check(route.Key('F', false, false, 0, true, other, apply, changed), "consume matching keyup even after focus change");
        Check(!route.Key('F', true, true, 0, true, other, apply, changed) && calls == 1, "foreground change cancels edit context");
        route.Move(window);
        Check(!route.Key('F', true, false, 0, true, window, apply, changed) && calls == 1, "shortcut, English or legacy encoding is untouched");
        Check(!route.Key(VK_LEFT, false, true, 0, true, window, apply, changed) && route.armed, "navigation keyup arms after caret movement");
        Check(!route.Key('B', true, true, 0, true, window, apply, changed) && !route.armed, "ordinary text disarms editing");
        route.Move(window);
        Check(!route.Key('S', true, true, 0, true, window, apply, changed) && !changed && !route.armed, "unsupported/no-op edit passes through");
    }
    void Shapes() {
        struct Case { const wchar_t* source; char key; int type; const wchar_t* expected; };
        const Case cases[] = {
            {L"noi", 'O', 0, L"n\u00f4i"}, {L"n\u00f4i", 'J', 0, L"n\u1ed9i"},
            {L"n\u1ed9i", 'O', 0, L"n\u1ecdi"}, {L"n\u1ecdi", 'O', 0, L"n\u1ed9i"},
            {L"n\u1ed9i", 'Z', 0, L"n\u00f4i"}, {L"n\u00f4i", 'O', 0, L"noi"},
            {L"NOI", 'O', 3, L"N\u00d4I"}, {L"N\u00d4I", 'J', 3, L"N\u1ed8I"},
            {L"t\u00ean", 'E', 0, L"ten"}, {L"ten", 'E', 0, L"t\u00ean"},
            {L"b\u1ea1n", 'A', 0, L"b\u1eadn"}, {L"b\u1eadn", 'A', 0, L"b\u1ea1n"},
            {L"b\u1eb1ng", 'W', 0, L"b\u00e0ng"}, {L"b\u00e0ng", 'W', 0, L"b\u1eb1ng"},
            {L"noi", 'W', 0, L"n\u01a1i"}, {L"n\u01a1i", 'W', 0, L"noi"},
            {L"mua", 'W', 0, L"m\u01b0a"}, {L"m\u01b0a", 'W', 0, L"mua"},
            {L"tuong", 'W', 0, L"t\u01b0\u01a1ng"}, {L"t\u01b0\u01a1ng", 'W', 0, L"tuong"},
            {L"thu\u1ecf", 'W', 0, L"thu\u1edf"},
            {L"THUO", 'W', 0, L"THU\u01a0"}, {L"THU\u1ece", 'W', 0, L"THU\u1ede"},
            {L"thuoc", 'W', 0, L"th\u01b0\u01a1c"},
            {L"t\u00faong", 'W', 0, L"t\u01b0\u1edbng"},
            {L"duong", 'D', 0, L"\u0111uong"}, {L"\u0111\u01b0\u1eddng", 'D', 0, L"d\u01b0\u1eddng"},
            {L"D", 'D', 0, L"\u0110"}, {L"\u0110", 'D', 0, L"D"},
            {L"noi", '6', 1, L"n\u00f4i"}, {L"n\u00f4i", '5', 1, L"n\u1ed9i"},
            {L"n\u1ed9i", '6', 1, L"n\u1ecdi"}, {L"n\u01a1i", '7', 1, L"noi"},
            {L"b\u00e0ng", '8', 1, L"b\u1eb1ng"}, {L"b\u1eb1ng", '8', 1, L"b\u00e0ng"},
            {L"tuong", '7', 1, L"t\u01b0\u01a1ng"}, {L"duong", '9', 1, L"\u0111uong"},
            {L"quoc", 'W', 0, L"qu\u01a1c"}
        };
        for (const auto& item : cases) {
            std::wstring edited;
            if (!EditAction(item.source, EditKey(item.key, item.type), true, edited) || edited != item.expected) {
                std::cerr << "Shape edit failed: key=" << item.key << " type=" << item.type << '\n';
                Check(false, "shape/tone transition");
            }
            ++checks;
        }
        for (const auto* word : {L"hello", L"noi2", L"no\u0302i", L"no_i"}) {
            std::wstring result;
            Check(!EditAction(word, HatO, true, result), "shape editing rejects identifiers and decomposed or unsupported words");
        }
        std::wstring unchanged;
        Check(!EditAction(L"noi", HatE, true, unchanged) && !EditAction(L"ban", HornOnly, true, unchanged) &&
            !EditAction(L"noi", StrokeD, true, unchanged), "unmatched shape keys pass through");
        Check(EditKey('A', 0) == HatA && EditKey('W', 2) == HornOrBreve && EditKey('6', 1) == HatAny &&
            EditKey('7', 1) == HornOnly && EditKey('8', 1) == Breve && EditKey('9', 1) == StrokeD &&
            EditKey('6', 0) == -1 && EditKey('O', 1) == -1, "Telex/VNI shape key selection");
    }
    void NativeShapes(HWND editor, HWND window) {
        const std::wstring original = L"noi dung ti\u1ebfng Vi\u1ec7t";
        for (size_t caret = 0; caret <= 3; ++caret) for (int type : {0, 1}) {
            SetWindowTextW(editor, original.c_str()); SendMessageW(editor, EM_SETSEL, caret, caret);
            Routing route; route.Move(window); bool changed = false;
            const auto edit = [&](int action, bool modern) {
                NativeSnapshot snapshot; std::wstring replacement;
                return ReadNative(editor, snapshot) && EditAction(snapshot.word.text, action, modern, replacement) &&
                    ReplaceNative(snapshot, replacement, [] { return true; });
            };
            const UINT hat = type == 1 ? '6' : 'O', dot = type == 1 ? '5' : 'J';
            Check(route.Key(hat, true, true, type, true, window, edit, changed) && changed, "first shape key is consumed");
            Check(route.Key(hat, true, true, type, true, window, edit, changed) && !changed, "holding shape key does not remove the hat again");
            Check(route.Key(hat, false, true, type, true, window, edit, changed), "shape keyup consumed");
            Check(route.Key(dot, true, true, type, true, window, edit, changed) && changed, "tone key continues the same edit session");
            route.Key(dot, false, true, type, true, window, edit, changed);
            wchar_t text[128]{}; GetWindowTextW(editor, text, 128);
            Check(std::wstring(text) == L"n\u1ed9i dung ti\u1ebfng Vi\u1ec7t", "noi + oj or 65 edits only the requested word in the sentence");
            DWORD begin = 0, end = 0; SendMessageW(editor, EM_GETSEL, reinterpret_cast<WPARAM>(&begin), reinterpret_cast<LPARAM>(&end));
            Check(begin == caret && end == caret, "multi-key shape/tone edit retains original caret");
        }
        SetWindowTextW(editor, L"t\u00faong"); SendMessageW(editor, EM_SETSEL, 3, 3);
        NativeSnapshot snapshot; std::wstring replacement;
        Check(ReadNative(editor, snapshot) && EditAction(snapshot.word.text, HornOrBreve, true, replacement) &&
            ReplaceNative(snapshot, replacement, [] { return true; }), "replace multiple vowels and move their tone in one native action");
        wchar_t text[64]{}; GetWindowTextW(editor, text, 64);
        Check(std::wstring(text) == L"t\u01b0\u1edbng", "horn pair and tone are both applied");
        SendMessageW(editor, EM_UNDO, 0, 0); GetWindowTextW(editor, text, 64);
        Check(std::wstring(text) == L"t\u00faong", "single undo restores the entire horn-pair edit");
    }
    void Native(const wchar_t* name) {
        HWND parent = CreateWindowExW(0, L"STATIC", L"Owned caret-tone fixture", WS_OVERLAPPEDWINDOW, 0, 0, 500, 300,
            nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
        HWND editor = CreateWindowExW(0, name, L"", WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_AUTOVSCROLL,
            0, 0, 400, 200, parent, nullptr, GetModuleHandleW(nullptr), nullptr);
        Check(parent && editor, "create owned hidden native edit fixture");
        NativeShapes(editor, parent);
        const std::wstring sentence = L"\u0111\u00e2y l\u00e0 n\u1ed9i dung b\u0103ng ti\u1ebfng Vi\u1ec7t";
        const auto begin = sentence.find(L"b\u0103ng");
        for (size_t offset = 0; offset <= 4; ++offset) {
            SetWindowTextW(editor, sentence.c_str()); SendMessageW(editor, EM_SETSEL, begin + offset, begin + offset);
            NativeSnapshot snapshot; std::wstring edited;
            const bool readable = ReadNative(editor, snapshot);
            if (!readable) {
                const auto line = SendMessageW(editor, EM_LINEFROMCHAR, begin + offset, 0);
                const auto lineStart = SendMessageW(editor, EM_LINEINDEX, line, 0);
                const auto lineLength = SendMessageW(editor, EM_LINELENGTH, begin + offset, 0);
                wchar_t buffer[4098]{}; *reinterpret_cast<WORD*>(buffer) = 4097;
                const auto copied = SendMessageW(editor, EM_GETLINE, line, reinterpret_cast<LPARAM>(buffer));
                std::cerr << "Native read failed: class=" << (name[0] == L'E' ? "Edit" : "RichEdit") << " offset=" << offset
                    << " line=" << line << " start=" << lineStart << " length=" << lineLength << " copied=" << copied << '\n';
            }
            Check(readable, "read real native caret word");
            Check(Edit(snapshot.word.text, 2, true, edited), "transform real native caret word");
            Check(ReplaceNative(snapshot, edited, [] { return true; }), "replace native word");
            wchar_t actual[128]{}; GetWindowTextW(editor, actual, 128);
            auto expected = sentence; expected.replace(begin, 4, L"b\u1eb1ng");
            Check(actual == expected, "sample sentence changes only bang to bang with grave");
            DWORD start = 0, end = 0; SendMessageW(editor, EM_GETSEL, reinterpret_cast<WPARAM>(&start), reinterpret_cast<LPARAM>(&end));
            Check(start == begin + offset && start == end, "caret position is preserved inside or beside word");
            SendMessageW(editor, EM_UNDO, 0, 0); GetWindowTextW(editor, actual, 128);
            Check(actual == sentence, "single undo restores the original sentence");
        }
        SetWindowTextW(editor, L"b\u0103ng"); SendMessageW(editor, EM_SETSEL, 4, 4);
        NativeSnapshot snapshot;
        Check(ReadNative(editor, snapshot), "fresh context");
        SendMessageW(editor, EM_SETSEL, 2, 2);
        Check(!ReplaceNative(snapshot, L"b\u1eb1ng", [] { return true; }), "stale caret never replaced");
        SendMessageW(editor, EM_SETSEL, 4, 4);
        SendMessageW(editor, EM_SETREADONLY, TRUE, 0);
        Check(!ReplaceNative(snapshot, L"b\u1eb1ng", [] { return true; }), "readonly enabled after capture prevents editing");
        SendMessageW(editor, EM_SETREADONLY, FALSE, 0);
        SetWindowTextW(editor, L"kh\u00e1c"); SendMessageW(editor, EM_SETSEL, 4, 4);
        Check(!ReplaceNative(snapshot, L"b\u1eb1ng", [] { return true; }), "stale text never replaced");
        Check(!ReplaceNative(snapshot, L"b\u1eb1ng", [] { return false; }), "cancelled operation never replaced");
        SendMessageW(editor, EM_SETSEL, 1, 3); Check(!ReadNative(editor, snapshot), "selection skipped");
        SendMessageW(editor, EM_SETSEL, 4, 4); SendMessageW(editor, EM_SETREADONLY, TRUE, 0);
        Check(!ReadNative(editor, snapshot), "readonly skipped"); SendMessageW(editor, EM_SETREADONLY, FALSE, 0);
        SendMessageW(editor, EM_SETPASSWORDCHAR, L'*', 0); Check(!ReadNative(editor, snapshot), "password mask skipped");
        SendMessageW(editor, EM_SETPASSWORDCHAR, 0, 0);
        SetWindowTextW(editor, (L"line one\r\nb\u0103ng\r\nline three")); SendMessageW(editor, EM_SETSEL, 12, 12);
        Check(ReadNative(editor, snapshot) && snapshot.word.text == L"b\u0103ng", "multiline caret uses the correct line");
        SetWindowPos(editor, nullptr, 0, 0, 20, 200, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
        SetWindowTextW(editor, L"b\u0103ngb\u0103ng"); SendMessageW(editor, EM_SETSEL, 2, 2);
        if (SendMessageW(editor, EM_GETLINECOUNT, 0, 0) <= 1)
            std::cerr << "No wrapping in " << (name[0] == L'E' ? "Edit" : "RichEdit") << '\n';
        Check(SendMessageW(editor, EM_GETLINECOUNT, 0, 0) > 1, "fixture has an actual soft-wrapped word");
        Check(!ReadNative(editor, snapshot), "soft wrapping cannot expose a fragment as a complete word");
        SetWindowPos(editor, nullptr, 0, 0, 400, 200, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
        SetWindowTextW(editor, std::wstring(4097, L'a').c_str()); SendMessageW(editor, EM_SETSEL, 1, 1);
        Check(!ReadNative(editor, snapshot), "line read remains bounded");
        EnableWindow(editor, FALSE); Check(!ReadNative(editor, snapshot), "disabled skipped");
        DestroyWindow(parent);
    }
}
int main() {
    try {
        Pure(); Shapes(); Route(); Native(L"Edit");
        HMODULE rich = LoadLibraryW(L"Msftedit.dll"); Check(rich != nullptr, "load native RichEdit");
        Native(L"RICHEDIT50W"); FreeLibrary(rich);
        std::cout << checks << " caret tone editing checks passed (pure rules/routing and owned hidden Edit/RichEdit controls).\n";
        return 0;
    } catch (const std::exception& e) { std::cerr << "FAIL: " << e.what() << '\n'; return 1; }
}
