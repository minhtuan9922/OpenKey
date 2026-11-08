#pragma once
#include <cstdint>
#include <stdexcept>
#include <string>
#include <type_traits>

namespace openkey {
// The legacy engine operates on UTF-16 units even when wchar_t is 32 bits.
// Preserve that representation, including surrogate pairs, without a global
// codecvt object. Both directions consume the full range, including NUL.
inline std::wstring utf8ToUtf16Wide(const std::string& bytes) {
    std::wstring result;
    result.reserve(bytes.size());
    for (std::size_t i = 0; i < bytes.size();) {
        const auto lead = static_cast<unsigned char>(bytes[i++]);
        std::uint32_t point = lead;
        unsigned trailing = 0;
        std::uint32_t minimum = 0;
        if (lead >= 0xc2 && lead <= 0xdf) { point = lead & 0x1f; trailing = 1; minimum = 0x80; }
        else if (lead >= 0xe0 && lead <= 0xef) { point = lead & 0x0f; trailing = 2; minimum = 0x800; }
        else if (lead >= 0xf0 && lead <= 0xf4) { point = lead & 0x07; trailing = 3; minimum = 0x10000; }
        else if (lead > 0x7f) throw std::range_error("Invalid UTF-8 leading byte");
        if (trailing > bytes.size() - i) throw std::range_error("Truncated UTF-8 sequence");
        for (unsigned j = 0; j < trailing; ++j) {
            const auto next = static_cast<unsigned char>(bytes[i++]);
            if ((next & 0xc0) != 0x80) throw std::range_error("Invalid UTF-8 continuation");
            point = (point << 6) | (next & 0x3f);
        }
        if (point < minimum || point > 0x10ffff || (point >= 0xd800 && point <= 0xdfff))
            throw std::range_error("Invalid Unicode scalar");
        if (point <= 0xffff) result.push_back(static_cast<wchar_t>(point));
        else {
            point -= 0x10000;
            result.push_back(static_cast<wchar_t>(0xd800 + (point >> 10)));
            result.push_back(static_cast<wchar_t>(0xdc00 + (point & 0x3ff)));
        }
    }
    return result;
}

inline std::string utf16WideToUtf8(const std::wstring& units) {
    std::string result;
    auto unit = [&](std::size_t at) {
        return static_cast<std::uint32_t>(static_cast<std::make_unsigned_t<wchar_t>>(units[at]));
    };
    for (std::size_t i = 0; i < units.size(); ++i) {
        auto point = unit(i);
        if (point >= 0xd800 && point <= 0xdbff) {
            if (++i == units.size()) throw std::range_error("Truncated UTF-16 pair");
            const auto low = unit(i);
            if (low < 0xdc00 || low > 0xdfff) throw std::range_error("Invalid UTF-16 pair");
            point = 0x10000 + ((point - 0xd800) << 10) + (low - 0xdc00);
        } else if (point >= 0xdc00 && point <= 0xdfff) throw std::range_error("Unpaired UTF-16 surrogate");
        if (point > 0x10ffff) throw std::range_error("Invalid Unicode scalar");
        if (point < 0x80) result.push_back(static_cast<char>(point));
        else if (point < 0x800) {
            result.push_back(static_cast<char>(0xc0 | (point >> 6)));
            result.push_back(static_cast<char>(0x80 | (point & 0x3f)));
        } else if (point < 0x10000) {
            result.push_back(static_cast<char>(0xe0 | (point >> 12)));
            result.push_back(static_cast<char>(0x80 | ((point >> 6) & 0x3f)));
            result.push_back(static_cast<char>(0x80 | (point & 0x3f)));
        } else {
            result.push_back(static_cast<char>(0xf0 | (point >> 18)));
            result.push_back(static_cast<char>(0x80 | ((point >> 12) & 0x3f)));
            result.push_back(static_cast<char>(0x80 | ((point >> 6) & 0x3f)));
            result.push_back(static_cast<char>(0x80 | (point & 0x3f)));
        }
    }
    return result;
}
}
