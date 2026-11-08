#pragma once
#include "../engine/Utf8.h"
#include <map>
#include <string>

namespace openkey {
// ASCII transport for Fcitx INI/DBus: hex(UTF8 key):hex(UTF8 value);...
// The same strict parser is shared by the addon and UI. No global engine macro state.
class MacroTable {
public:
    using Entries = std::map<std::string, std::string>;
    static constexpr std::size_t MaxEncoded = 65536;
    static bool validEntry(const std::string& key, const std::string& value, std::size_t maxKeyUnits = 32) {
        if (key.empty() || key.size() > 128 || value.empty() || value.size() > 4096) return false;
        try {
            const auto k = utf8ToUtf16Wide(key), v = utf8ToUtf16Wide(value);
            if (k.size() > maxKeyUnits) return false;
            for (auto ch : k) if (ch <= 32 || (ch >= 127 && ch <= 159)) return false;
            for (auto ch : v) if ((ch < 32 && ch != '\n' && ch != '\t') || (ch >= 127 && ch <= 159)) return false;
        } catch (const std::range_error&) { return false; }
        return true;
    }
    static std::string hex(const std::string& value) {
        static constexpr char digits[] = "0123456789abcdef";
        std::string out;
        for (unsigned char ch : value) { out += digits[ch >> 4]; out += digits[ch & 15]; }
        return out;
    }
    static bool decode(const std::string& value, std::string& out) {
        if (value.size() % 2) return false;
        const auto digit = [](char ch) { return ch >= '0' && ch <= '9' ? ch - '0' : ch >= 'a' && ch <= 'f' ? ch - 'a' + 10 : -1; };
        out.clear();
        for (std::size_t i = 0; i < value.size(); i += 2) {
            const int high = digit(value[i]), low = digit(value[i + 1]);
            if (high < 0 || low < 0) return false;
            out += static_cast<char>((high << 4) | low);
        }
        return true;
    }
    static std::string encode(const Entries& entries) {
        std::string out;
        for (const auto& [key, value] : entries) out += hex(key) + ":" + hex(value) + ";";
        return out;
    }
    static bool parse(const std::string& encoded, Entries& entries, std::size_t maxKeyUnits = 32) {
        if (encoded.size() > MaxEncoded) return false;
        Entries candidate;
        std::size_t start = 0;
        while (start < encoded.size()) {
            const auto end = encoded.find(';', start), colon = encoded.find(':', start);
            if (end == std::string::npos || colon == std::string::npos || colon >= end || candidate.size() >= 256) return false;
            std::string key, value;
            if (!decode(encoded.substr(start, colon - start), key) ||
                !decode(encoded.substr(colon + 1, end - colon - 1), value) ||
                !validEntry(key, value, maxKeyUnits) || !candidate.emplace(key, value).second) return false;
            start = end + 1;
        }
        entries = std::move(candidate);
        return true;
    }
};
}
