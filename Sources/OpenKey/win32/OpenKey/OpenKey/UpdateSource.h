#pragma once
#include <string>

// Release contract: versionName is the exact GitHub tag; asset is OpenKey-Setup.exe.
namespace OpenKeyUpdateSource {
constexpr wchar_t MetadataHost[] = L"raw.githubusercontent.com";
constexpr wchar_t MetadataPath[] = L"/minhtuan9922/OpenKey/master/version.json";
constexpr wchar_t ReleaseBase[] = L"https://github.com/minhtuan9922/OpenKey/releases/download/";
inline bool ValidTag(const std::string& tag) {
    if (tag.empty() || tag.size() > 64 || tag == "." || tag == "..") return false;
    for (const unsigned char ch : tag)
        if (!((ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'z') ||
            (ch >= 'A' && ch <= 'Z') || ch == '.' || ch == '-' || ch == '_')) return false;
    return true;
}
inline std::wstring SetupUrl(const std::string& tag) {
    if (!ValidTag(tag)) return {};
    return std::wstring(ReleaseBase) + std::wstring(tag.begin(), tag.end()) + L"/OpenKey-Setup.exe";
}
}
