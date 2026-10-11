#pragma once

#include <array>
#include <limits>
#include <string>
#include "HtmlNamedEntities.h"

// CF_HTML is UTF-8; offsets count bytes, not UTF-16 characters.
// Fail closed on unsupported headers/fragments instead of damaging rich text.
namespace HtmlClipboard {
inline void appendUtf8(unsigned int value, std::string& output) {
    if (value < 0x80) output += static_cast<char>(value);
    else if (value < 0x800) {
        output += static_cast<char>(0xC0 | (value >> 6));
        output += static_cast<char>(0x80 | (value & 0x3F));
    } else if (value < 0x10000) {
        output += static_cast<char>(0xE0 | (value >> 12));
        output += static_cast<char>(0x80 | ((value >> 6) & 0x3F));
        output += static_cast<char>(0x80 | (value & 0x3F));
    } else {
        output += static_cast<char>(0xF0 | (value >> 18));
        output += static_cast<char>(0x80 | ((value >> 12) & 0x3F));
        output += static_cast<char>(0x80 | ((value >> 6) & 0x3F));
        output += static_cast<char>(0x80 | (value & 0x3F));
    }
}

// Numeric references use bounded HTML recovery. Named references use longest-match
// data-state rules, including the 106 historical no-semicolon aliases.
// A preview limit is resolved against the complete text node, never a sliced
// prefix (which could incorrectly match &not inside the longer &notin;).
inline bool decodeText(const std::string& source, std::string& output,
    size_t limit = std::string::npos) {
    output.clear();
    if (limit == std::string::npos) limit = source.size();
    if (limit > source.size()) return false;
    for (size_t cursor = 0; cursor < limit; ++cursor) {
        if (source[cursor] != '&') {
            // HTML normalizes literal CR/CRLF before parsing references. A CR
            // produced by a numeric reference must instead survive unchanged.
            if (source[cursor] == '\r') {
                output += '\n';
                if (cursor + 1 < source.size() && source[cursor + 1] == '\n') {
                    if (cursor + 1 == limit) return false;
                    ++cursor;
                }
            } else output += source[cursor];
            continue;
        }
        if (cursor + 1 < source.size() && source[cursor + 1] != '#') {
            const NamedEntities::Entry* match = nullptr;
            size_t matchEnd = cursor + 1;
            for (size_t length = 1; length <= NamedEntities::maximumNameLength &&
                cursor + length < source.size(); ++length) {
                const char character = source[cursor + length];
                if (!((character >= 'a' && character <= 'z') ||
                    (character >= 'A' && character <= 'Z') ||
                    (character >= '0' && character <= '9'))) break;
                const std::string_view name(source.data() + cursor + 1, length);
                const auto* candidate = NamedEntities::find(name);
                if (!candidate) continue;
                const size_t end = cursor + length + 1;
                if (end < source.size() && source[end] == ';') {
                    match = candidate; matchEnd = end + 1;
                    break;
                }
                if (NamedEntities::allowsMissingSemicolon(name)) {
                    match = candidate; matchEnd = end;
                }
            }
            // Unknown names and bare ampersands are literal data, not a failed
            // conversion. Reconsume the suffix normally (without decoding twice).
            if (!match) { output += '&'; continue; }
            if (matchEnd > limit) return false;
            appendUtf8(match->first, output);
            if (match->second) appendUtf8(match->second, output);
            cursor = matchEnd - 1;
            continue;
        }
        if (cursor + 1 >= source.size() || source[cursor + 1] != '#') {
            output += '&'; continue;
        }
        size_t end = cursor + 2;
        unsigned int base = 10;
        if (end < source.size() && (source[end] == 'x' || source[end] == 'X')) {
            base = 16;
            ++end;
        }
        const size_t digitStart = end;
        unsigned int value = 0;
        for (; end < source.size(); ++end) {
            const char character = source[end];
            unsigned int digit;
            if (character >= '0' && character <= '9') digit = character - '0';
            else if (character >= 'a' && character <= 'f') digit = character - 'a' + 10;
            else if (character >= 'A' && character <= 'F') digit = character - 'A' + 10;
            else break;
            if (digit >= base) break;
            // Saturate above Unicode's maximum but consume every input digit,
            // including arbitrarily long numbers, without integer overflow.
            if (value > (0x10FFFFu - digit) / base) value = 0x110000u;
            else
                value = value * base + digit;
        }
        if (end == digitStart) { output += '&'; continue; }
        if (end < source.size() && source[end] == ';') ++end;
        if (end > limit) return false;
        if (!value || value > 0x10FFFFu || (value >= 0xD800u && value <= 0xDFFFu))
            value = 0xFFFDu;
        // HTML's historical Windows-1252 replacements; five unmapped C1
        // controls, other controls and noncharacters survive unchanged.
        static constexpr std::array<unsigned int, 32> c1 = {
            0x20AC, 0x81, 0x201A, 0x192, 0x201E, 0x2026, 0x2020, 0x2021,
            0x2C6, 0x2030, 0x160, 0x2039, 0x152, 0x8D, 0x17D, 0x8F,
            0x90, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014,
            0x2DC, 0x2122, 0x161, 0x203A, 0x153, 0x9D, 0x17E, 0x178
        };
        if (value >= 0x80u && value <= 0x9Fu) value = c1[value - 0x80u];
        appendUtf8(value, output);
        cursor = end - 1;
    }
    return true;
}

inline std::string escapeText(const std::string& source) {
    std::string output;
    for (const char character : source) {
        if (character == '&') output += "&amp;";
        else if (character == '<') output += "&lt;";
        else if (character == '>') output += "&gt;";
        else if (character == '\r') output += "&#13;";
        else output += character;
    }
    return output;
}

template<typename Transform>
bool rewrite(const std::string& source, std::string& output, Transform transform) {
    struct Field { size_t position = 0, width = 0, value = 0; bool found = false, absent = false; };
    const std::array<std::string, 6> names = { "StartHTML", "EndHTML", "StartFragment", "EndFragment", "StartSelection", "EndSelection" };
    std::array<Field, 6> fields{};
    size_t line = 0, headerEnd = 0;
    if (source.compare(0, 12, "Version:0.9\r") != 0 &&
        source.compare(0, 12, "Version:1.0\r") != 0 &&
        source.compare(0, 12, "Version:0.9\n") != 0 &&
        source.compare(0, 12, "Version:1.0\n") != 0) return false;
    while (line < source.size()) {
        if (fields[0].found && ((!fields[0].absent && line == fields[0].value) ||
            (fields[0].absent && source.compare(line, 20, "<!--StartFragment-->") == 0))) {
            headerEnd = line; break;
        }
        const auto end = source.find_first_of("\r\n", line);
        if (end == std::string::npos) return false;
        const auto colon = source.find(':', line);
        if (colon == std::string::npos || colon >= end) return false;
        const auto key = source.substr(line, colon - line);
        for (size_t index = 0; index < names.size(); ++index) {
            if (key != names[index]) continue;
            auto& field = fields[index];
            if (field.found) return false;
            field.position = colon + 1;
            while (field.position < end && source[field.position] == ' ') ++field.position;
            field.width = end - field.position;
            if (!field.width) return false;
            if (source.compare(field.position, field.width, "-1") == 0) {
                if (index > 1) return false;
                field.absent = field.found = true;
                continue;
            }
            for (size_t cursor = field.position; cursor < end; ++cursor) {
                const char digit = source[cursor];
                if (digit < '0' || digit > '9' ||
                    field.value > ((std::numeric_limits<size_t>::max)() - (digit - '0')) / 10) return false;
                field.value = field.value * 10 + (digit - '0');
            }
            field.found = true;
        }
        line = end + 1;
        if (source[end] == '\r' && line < source.size() && source[line] == '\n') ++line;
    }
    for (size_t index = 0; index < 4; ++index) if (!fields[index].found) return false;
    const auto start = fields[2].value, finish = fields[3].value;
    if (!headerEnd || headerEnd > start || start > finish || finish > source.size() ||
        fields[0].absent != fields[1].absent) return false;
    if (!fields[0].absent && (finish > fields[1].value || fields[1].value != source.size())) return false;
    if (fields[0].absent && (start != headerEnd + 20 || source.substr(finish) != "<!--EndFragment-->")) return false;
    if (fields[4].found != fields[5].found || (fields[4].found &&
        (fields[4].value < start || fields[4].value > fields[5].value || fields[5].value > finish))) return false;
    const auto fragment = source.substr(start, finish - start);
    std::string converted;
    std::array<size_t, 2> selections{};
    std::array<bool, 2> mapped{};
    // Transform must own its state by value so a copy is an isolated preview.
    auto transformText = [&](const std::string& text, std::string& replacement, bool preview,
        size_t limit = std::string::npos) {
        std::string decoded, transformed;
        if (!decodeText(text, decoded, limit)) return false;
        if (preview) {
            auto snapshot = transform;
            if (!snapshot(decoded, transformed)) return false;
        } else if (!transform(decoded, transformed)) return false;
        replacement = escapeText(transformed);
        return true;
    };
    // Preview prefixes from the state before this text node. Neither selection
    // endpoint is allowed to advance the actual conversion state.
    auto mapSelections = [&](size_t begin, size_t end, bool text) {
        for (size_t index = 0; index < selections.size(); ++index) {
            const auto& field = fields[4 + index];
            if (!field.found || mapped[index] || field.value < start + begin || field.value > start + end) continue;
            const auto offset = field.value - start - begin;
            size_t convertedOffset = offset;
            if (text) {
                std::string prefix;
                if (!transformText(fragment.substr(begin, end - begin), prefix, true, offset)) return false;
                convertedOffset = prefix.size();
            } else if (offset < end - begin &&
                (static_cast<unsigned char>(fragment[begin + offset]) & 0xC0) == 0x80) return false;
            selections[index] = start + converted.size() + convertedOffset;
            mapped[index] = true;
        }
        return true;
    };
    size_t cursor = 0;
    while (cursor < fragment.size()) {
        if (fragment[cursor] == '<') {
            // Preserve tags/attributes verbatim, including quoted '>' characters.
            const auto nameStart = cursor + ((cursor + 1 < fragment.size() && fragment[cursor + 1] == '/') ? 2 : 1);
            const auto nameEnd = fragment.find_first_of(" \t\r\n>/", nameStart);
            auto name = fragment.substr(nameStart, nameEnd - nameStart);
            for (auto& character : name) if (character >= 'A' && character <= 'Z') character += 'a' - 'A';
            if (name == "script" || name == "style" || name == "textarea" || name == "title") return false;
            size_t end = cursor + 1;
            if (fragment.compare(cursor, 4, "<!--") == 0) {
                end = fragment.find("-->", cursor + 4);
                if (end == std::string::npos) return false;
                end += 3;
            } else {
                char quote = 0;
                for (; end < fragment.size(); ++end) {
                    const auto character = fragment[end];
                    if (quote) { if (character == quote) quote = 0; }
                    else if (character == '\'' || character == '"') quote = character;
                    else if (character == '>') { ++end; break; }
                }
                if (quote || end > fragment.size() || fragment[end - 1] != '>') return false;
            }
            if (!mapSelections(cursor, end, false)) return false;
            converted.append(fragment, cursor, end - cursor);
            // Logical paragraph/line boundaries affect casing but never inject
            // characters into the serialized HTML. Inline tags are transparent.
            const std::array<const char*, 25> boundaries = { "br", "hr", "p", "div", "li", "ul", "ol", "h1", "h2", "h3", "h4", "h5", "h6", "tr", "td", "th", "table", "section", "article", "header", "footer", "blockquote", "pre", "dl", "dt" };
            for (const auto* boundary : boundaries) if (name == boundary) {
                std::string discarded;
                if (!transform("\n", discarded)) return false;
                break;
            }
            cursor = end;
        } else {
            const auto next = fragment.find('<', cursor);
            const auto end = next == std::string::npos ? fragment.size() : next;
            const auto text = fragment.substr(cursor, end - cursor);
            std::string replacement;
            if (!mapSelections(cursor, end, true)) return false;
            if (!transformText(text, replacement, false)) return false;
            converted += replacement;
            cursor = end;
        }
    }
    output = source.substr(0, start) + converted + source.substr(finish);
    if (fragment.empty()) {
        for (size_t index = 0; index < selections.size(); ++index) {
            selections[index] = start;
            mapped[index] = true;
        }
    }
    if (fields[4].found) {
        for (size_t index = 0; index < selections.size(); ++index) {
            if (!mapped[index]) return false;
            fields[4 + index].value = selections[index];
        }
    }
    if (!fields[1].absent) fields[1].value = output.size();
    fields[3].value = start + converted.size();
    for (const auto& field : fields) {
        if (!field.found || field.absent) continue;
        const auto value = std::to_string(field.value);
        if (value.size() > field.width) return false;
        output.replace(field.position, field.width, std::string(field.width - value.size(), '0') + value);
    }
    return true;
}
}
