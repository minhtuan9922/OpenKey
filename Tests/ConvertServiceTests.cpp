#include <windows.h>
#include <stdexcept>
#include "../Sources/OpenKey/engine/ConvertTool.h"
#define main RunEngineRegression
#include "EngineRegressionTests.cpp"
#undef main

// Any accidental clipboard or clipboard-memory call fails instead of touching Windows state.
template<typename Result, typename... Arguments> Result forbiddenClipboard(Arguments...) {
    throw std::runtime_error("Clipboard API must not be called by pure conversion tests");
}
#define OpenClipboard forbiddenClipboard<BOOL>
#define CloseClipboard forbiddenClipboard<BOOL>
#define RegisterClipboardFormatW forbiddenClipboard<UINT>
#define GetClipboardData forbiddenClipboard<HANDLE>
#define IsClipboardFormatAvailable forbiddenClipboard<BOOL>
#define EmptyClipboard forbiddenClipboard<BOOL>
#define SetClipboardData forbiddenClipboard<HANDLE>
#define GlobalAlloc forbiddenClipboard<HGLOBAL>
#define GlobalLock forbiddenClipboard<LPVOID>
#define GlobalUnlock forbiddenClipboard<BOOL>
#define GlobalFree forbiddenClipboard<HGLOBAL>
#define GlobalSize forbiddenClipboard<SIZE_T>
#define GetActiveWindow forbiddenClipboard<HWND>
#include "../Sources/OpenKey/win32/OpenKey/OpenKey/ConvertService.cpp"

using Options = ConvertService::Options;
int passed = 0;
void equal(const std::wstring& actual, const std::wstring& expected, const char* name) {
    if (actual != expected) {
        std::cerr << "FAIL " << name << ": actual=" << wideStringToUtf8(actual)
            << " expected=" << wideStringToUtf8(expected) << '\n';
        throw std::runtime_error("Conversion mismatch");
    }
    ++passed;
}
std::wstring legacy(const std::wstring& source, const Options& options) {
    convertToolFromCode = static_cast<Uint8>(options.fromCode);
    convertToolToCode = static_cast<Uint8>(options.toCode);
    convertToolToAllCaps = options.toAllCaps;
    convertToolToAllNonCaps = options.toAllNonCaps;
    convertToolToCapsFirstLetter = options.toCapsFirstLetter;
    convertToolToCapsEachWord = options.toCapsEachWord;
    convertToolRemoveMark = options.removeMark;
    auto result = utf8ToWideString(convertUtil(wideStringToUtf8(source)));
    // convertUtil returns its explicit string terminator; service returns text without it.
    if (!result.empty() && result.back() == L'\0') result.pop_back();
    return result;
}
int main() {
    try {
        if (RunEngineRegression()) return 1;
        const std::wstring corpus = L"Tiếng Việt: đường, tưởng, cứu.\nẮ ằ ẵ ặ ấ ầ ẩ ẫ ậ ế ề ể ễ ệ ố ồ ổỗ ộ ớ ờ ở ỡ ợ ứ ừử ữ ự đ Đ! ascii 123 😀\n";
        // Encode each source through the legacy engine, then compare all 25 mappings.
        // Trailing newline avoids the known legacy final-character packed-output branch.
        for (int from = 0; from < 5; ++from) {
            Options encode; encode.toCode = from;
            const auto source = legacy(corpus, encode);
            for (int to = 0; to < 5; ++to) {
                for (int mode = 0; mode < 6; ++mode) {
                    Options options; options.fromCode = from; options.toCode = to;
                    options.toAllCaps = mode == 1; options.toAllNonCaps = mode == 2;
                    options.toCapsFirstLetter = mode == 3; options.toCapsEachWord = mode == 4;
                    options.removeMark = mode == 5;
                    equal(ConvertService::convert(source, options), legacy(source, options), "legacy 5x5x6 matrix");
                }
            }
        }
        Options options;
        equal(ConvertService::convert(L"", options), L"", "empty");
        equal(ConvertService::convert(L"abc 123 😀", options), L"abc 123 😀", "plain surrogate pair");
        options.toAllCaps = true;
        equal(ConvertService::convert(L"tiếng việt", options), L"TIẾNG VIỆT", "all caps golden");
        options = {}; options.toAllNonCaps = true;
        equal(ConvertService::convert(L"TIẾNG VIỆT", options), L"tiếng việt", "lowercase golden");
        options = {}; options.toCapsFirstLetter = true;
        equal(ConvertService::convert(L"tiếng việt. đường mới!\nxin chào", options), L"Tiếng việt. Đường mới!\nXin chào", "sentence golden");
        options = {}; options.toCapsEachWord = true;
        equal(ConvertService::convert(L"tiếng việt đường mới", options), L"Tiếng Việt Đường Mới", "words golden");
        options = {}; options.removeMark = true;
        equal(ConvertService::convert(L"tiếng việt đường", options), L"tieng viet duong", "remove marks golden");
        for (int code : { -1, 5, 999 }) {
            options = {}; options.fromCode = code;
            equal(ConvertService::convert(L"abc", options), L"", "invalid source table");
            options = {}; options.toCode = code;
            equal(ConvertService::convert(L"abc", options), L"", "invalid destination table");
        }
        const std::wstring lower = L"tiếng việt đường tưởng cứu\n";
        for (int code = 0; code < 5; ++code) {
            options = {}; options.toCode = code;
            const auto encoded = ConvertService::convert(lower, options);
            options.fromCode = code; options.toCode = 0;
            equal(ConvertService::convert(encoded, options), lower, "five table roundtrip");
        }
        for (int code = 0; code < 5; ++code) {
            options = {}; options.toCode = code;
            const auto encoded = ConvertService::convert(L"việt ệ", options);
            options.fromCode = code; options.toCode = 0;
            equal(ConvertService::convert(encoded, options), L"việt ệ", "final marked character roundtrip");
        }
        options = {};
        const std::wstring embeddedNull(L"a\0b", 3);
        equal(ConvertService::convert(embeddedNull, options), embeddedNull, "length-aware NUL");
        std::wstring longText;
        for (int index = 0; index < 5000; ++index) longText += L"tiếng việt\n";
        equal(ConvertService::convert(longText, options), longText, "long text");
        std::cout << "Convert service: " << passed << "/" << passed << " passed (pure text; clipboard API traps)\n";
        return 0;
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
