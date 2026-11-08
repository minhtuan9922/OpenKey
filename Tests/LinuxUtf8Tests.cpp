#include "Engine.h"
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {
void require(bool value, const char* label) { if (!value) throw std::runtime_error(label); }
template<class Function> void rejects(Function function) {
    try { function(); } catch (const std::range_error&) { return; }
    throw std::runtime_error("Malformed Unicode accepted");
}
}
int main() {
    try {
        for (const auto& input : std::vector<std::string>{"", "ASCII", u8"Tiếng Việt", u8"a\u0306\u0301", u8"😀𐀀􏿿", std::string("a\0b", 3)})
            require(wideStringToUtf8(utf8ToWideString(input)) == input, "UTF-8 roundtrip");
        require(utf8ToWideString(u8"😀").size() == 2, "Legacy UTF-16 unit representation");
        for (const auto& bad : std::vector<std::string>{"\x80", "\xc0\x80", "\xc1\xbf", "\xc2", "\xc2\x20",
                "\xe0\x80\xaf", "\xe0\xa0", "\xed\xa0\x80", "\xed\xbf\xbf", "\xf0\x80\x80\xaf",
                "\xf0\x90\x80", "\xf4\x90\x80\x80", "\xf5\x80\x80\x80", "\xff", std::string("a\0\xff", 3)})
            rejects([&] { utf8ToWideString(bad); });
        for (const auto& bad : std::vector<std::wstring>{std::wstring(1, wchar_t(0xd800)), std::wstring(1, wchar_t(0xdc00)),
                {wchar_t(0xd800), L'a'}, {wchar_t(0xdc00), wchar_t(0xd800)}, {wchar_t(0xd800), wchar_t(0xd800)}})
            rejects([&] { wideStringToUtf8(bad); });
        if constexpr (sizeof(wchar_t) > 2) {
            rejects([] { wideStringToUtf8(std::wstring(1, wchar_t(0x110000))); });
            require(wideStringToUtf8(std::wstring(1, wchar_t(0x1f600))) == u8"😀", "Native wide scalar");
        }
        // Exhaust every scalar, rather than sampling only Vietnamese BMP text.
        std::size_t scalars = 0;
        for (std::uint32_t point = 0; point <= 0x10ffff; ++point) {
            if (point >= 0xd800 && point <= 0xdfff) continue;
            std::wstring units;
            if (point < 0x10000) units.push_back(static_cast<wchar_t>(point));
            else {
                const auto value = point - 0x10000;
                units.push_back(static_cast<wchar_t>(0xd800 + (value >> 10)));
                units.push_back(static_cast<wchar_t>(0xdc00 + (value & 0x3ff)));
            }
            require(utf8ToWideString(wideStringToUtf8(units)) == units, "Exhaustive scalar roundtrip");
            ++scalars;
        }
        std::cout << "UTF-8 validation and " << scalars << " scalar roundtrips passed\n";
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
