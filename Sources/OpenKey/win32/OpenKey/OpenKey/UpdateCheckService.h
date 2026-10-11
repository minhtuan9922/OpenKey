#pragma once
#include <string>
#include <cstdint>

// Deliberately accepts only the release metadata schema (objects, ASCII strings,
// unsigned integers). Unsupported JSON shapes fail closed, not "up to date".
namespace UpdateCheckService {
enum class Result { Available, UpToDate, DownloadFailed, InvalidMetadata };
struct Parser {
    const std::wstring& data;
    size_t position = 0;
    std::wstring version;
    uint32_t code = 0;
    bool windowsSeen = false, nameSeen = false, codeSeen = false;
    explicit Parser(const std::wstring& input) : data(input) {}
    void whitespace() {
        while (position < data.size() && (data[position] == L' ' || data[position] == L'\t'
            || data[position] == L'\r' || data[position] == L'\n')) ++position;
    }
    bool take(wchar_t value) {
        whitespace();
        if (position == data.size() || data[position] != value) return false;
        ++position; return true;
    }
    bool string(std::wstring& value) {
        if (!take(L'"')) return false;
        value.clear();
        while (position < data.size() && data[position] != L'"') {
            const auto ch = data[position++];
            if (ch < 32 || ch > 126 || ch == L'\\') return false;
            value += ch;
        }
        return take(L'"');
    }
    bool number(uint32_t& value) {
        whitespace(); value = 0;
        const auto start = position;
        while (position < data.size() && data[position] >= L'0' && data[position] <= L'9') {
            const uint32_t digit = data[position++] - L'0';
            if (value > (UINT32_MAX - digit) / 10) return false;
            value = value * 10 + digit;
        }
        return position > start && (position - start == 1 || data[start] != L'0');
    }
    bool object(unsigned depth, bool windows = false) {
        if (depth > 16 || !take(L'{')) return false;
        if (take(L'}')) return true;
        for (;;) {
            std::wstring key;
            if (!string(key) || !take(L':')) return false;
            if (depth == 0 && key == L"latestWinVersion") {
                if (windowsSeen) return false;
                windowsSeen = true;
                if (!object(depth + 1, true)) return false;
            } else if (windows && key == L"versionName") {
                if (nameSeen || !string(version) || version.empty() || version.size() > 64) return false;
                if (version == L"." || version == L"..") return false;
                for (auto ch : version) if (!((ch >= L'0' && ch <= L'9') || (ch >= L'a' && ch <= L'z')
                    || (ch >= L'A' && ch <= L'Z') || ch == L'.' || ch == L'-' || ch == L'_')) return false;
                nameSeen = true;
            } else if (windows && key == L"versionCode") {
                if (codeSeen || !number(code) || code > 0xFFFFFF) return false;
                codeSeen = true;
            } else {
                whitespace();
                if (position == data.size()) return false;
                if (data[position] == L'{') { if (!object(depth + 1)) return false; }
                else if (data[position] == L'"') { std::wstring ignored; if (!string(ignored)) return false; }
                else { uint32_t ignored; if (!number(ignored)) return false; }
            }
            if (take(L'}')) return true;
            if (!take(L',')) return false;
        }
    }
};
inline uint32_t OrderedVersion(uint32_t packed) {
    return ((packed & 0xFF) << 16) | (packed & 0xFF00) | ((packed >> 16) & 0xFF);
}
inline Result Check(const std::wstring& data, uint32_t currentVersion, std::string& version) {
    version.clear();
    if (data.empty()) return Result::DownloadFailed;
    if (data.size() > 65536) return Result::InvalidMetadata;
    Parser parser{ data };
    if (!parser.object(0)) return Result::InvalidMetadata;
    parser.whitespace();
    if (parser.position != data.size() || !parser.windowsSeen || !parser.nameSeen || !parser.codeSeen)
        return Result::InvalidMetadata;
    // Every code unit was checked as ASCII above; make the narrowing explicit.
    for (auto ch : parser.version) version += static_cast<char>(ch);
    return OrderedVersion(parser.code) > OrderedVersion(currentVersion) ? Result::Available : Result::UpToDate;
}
}
