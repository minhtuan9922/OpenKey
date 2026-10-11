#pragma once
#include <windows.h>
#include <winhttp.h>
#include <bcrypt.h>
#include <shellapi.h>
#include <atomic>
#include <sstream>
#include <string>
#include "UpdateSource.h"
#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "bcrypt.lib")
#pragma comment(lib, "shell32.lib")

namespace SetupUpdateService {
struct InternetHandle {
    HINTERNET value;
    ~InternetHandle() { if (value) WinHttpCloseHandle(value); }
};
struct FileHandle {
    HANDLE value;
    ~FileHandle() { if (value != INVALID_HANDLE_VALUE) CloseHandle(value); }
};
struct Package {
    std::wstring directory, path;
    std::string expectedHash;
    std::atomic<int> percent{0};
    bool verified = false, launched = false;
    ~Package() {
        // Setup still needs its executable after ShellExecute returns.
        if (!launched && !directory.empty()) {
            DeleteFileW(path.c_str());
            RemoveDirectoryW(directory.c_str());
        }
    }
};
inline bool ExpectedHash(const std::string& text, std::string& hash) {
    hash.clear();
    std::istringstream lines(text);
    std::string line;
    while (std::getline(lines, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.size() != 83 || line.substr(64) != "  OpenKey-Setup.exe") continue;
        if (!hash.empty()) { hash.clear(); return false; }
        hash = line.substr(0, 64);
        for (char& ch : hash) {
            if (ch >= 'A' && ch <= 'F') ch += 'a' - 'A';
            if (!((ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'f'))) { hash.clear(); return false; }
        }
    }
    return !hash.empty();
}
inline bool AllowedUrl(const std::wstring& url, std::wstring& host, std::wstring& target) {
    URL_COMPONENTS parts{}; parts.dwStructSize = sizeof(parts);
    parts.dwHostNameLength = parts.dwUrlPathLength = parts.dwExtraInfoLength = DWORD(-1);
    parts.dwUserNameLength = parts.dwPasswordLength = DWORD(-1);
    if (!WinHttpCrackUrl(url.c_str(), 0, 0, &parts) || parts.nScheme != INTERNET_SCHEME_HTTPS ||
        parts.nPort != INTERNET_DEFAULT_HTTPS_PORT || parts.dwUserNameLength || parts.dwPasswordLength) return false;
    host.assign(parts.lpszHostName, parts.dwHostNameLength);
    if (host != L"github.com" && host != L"release-assets.githubusercontent.com" &&
        host != L"objects.githubusercontent.com") return false;
    target.assign(parts.lpszUrlPath, parts.dwUrlPathLength);
    if (parts.dwExtraInfoLength) target.append(parts.lpszExtraInfo, parts.dwExtraInfoLength);
    return true;
}
template<class Read, class Write, class Now>
bool Transfer(DWORD limit, bool hasLength, DWORD length, Read read, Write write, Now now,
    ULONGLONG started, Package& package, bool executable) {
    DWORD total = 0;
    char buffer[65536];
    for (;;) {
        DWORD count = 0;
        if (now() - started > 600000 || !read(buffer, sizeof(buffer), count) || now() - started > 600000) return false;
        if (!count) return total > 0 && (!hasLength || total == length);
        if (count > sizeof(buffer) || count > limit - total || (hasLength && count > length - total)) return false;
        if (!write(buffer, count)) return false;
        total += count;
        if (executable && length) package.percent.store(static_cast<int>(100ULL * total / length));
    }
}
// Explicit HTTPS redirects, bounded size/time, and exact Content-Length for the executable.
inline bool Fetch(std::wstring url, HANDLE file, std::string& text, Package& package) {
    InternetHandle session{WinHttpOpen(L"OpenKey setup update", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
        WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0)};
    if (!session.value || !WinHttpSetTimeouts(session.value, 5000, 10000, 15000, 15000)) return false;
    const auto started = GetTickCount64();
    const bool executable = file != INVALID_HANDLE_VALUE;
    const DWORD limit = executable ? 512 * 1024 * 1024 : 65536;
    text.clear();
    for (int redirect = 0; redirect <= 5; ++redirect) {
        std::wstring host, target;
        if (!AllowedUrl(url, host, target)) return false;
        InternetHandle connection{WinHttpConnect(session.value, host.c_str(), INTERNET_DEFAULT_HTTPS_PORT, 0)};
        if (!connection.value) return false;
        InternetHandle request{WinHttpOpenRequest(connection.value, L"GET", target.c_str(), nullptr,
            WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE)};
        DWORD policy = WINHTTP_OPTION_REDIRECT_POLICY_NEVER;
        if (!request.value || !WinHttpSetOption(request.value, WINHTTP_OPTION_REDIRECT_POLICY, &policy, sizeof(policy)) ||
            !WinHttpSendRequest(request.value, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) ||
            !WinHttpReceiveResponse(request.value, nullptr)) return false;
        DWORD status = 0, size = sizeof(status);
        if (!WinHttpQueryHeaders(request.value, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
            WINHTTP_HEADER_NAME_BY_INDEX, &status, &size, WINHTTP_NO_HEADER_INDEX)) return false;
        if (status == 301 || status == 302 || status == 303 || status == 307 || status == 308) {
            wchar_t location[8192]; size = sizeof(location);
            if (!WinHttpQueryHeaders(request.value, WINHTTP_QUERY_LOCATION, WINHTTP_HEADER_NAME_BY_INDEX,
                location, &size, WINHTTP_NO_HEADER_INDEX)) return false;
            url = location;
            continue;
        }
        if (status != 200) return false;
        DWORD length = 0; size = sizeof(length);
        const bool hasLength = WinHttpQueryHeaders(request.value, WINHTTP_QUERY_CONTENT_LENGTH | WINHTTP_QUERY_FLAG_NUMBER,
            WINHTTP_HEADER_NAME_BY_INDEX, &length, &size, WINHTTP_NO_HEADER_INDEX) != FALSE;
        if ((executable && (!hasLength || !length)) || (hasLength && length > limit)) return false;
        return Transfer(limit, hasLength, length,
            [&](char* buffer, DWORD capacity, DWORD& count) { return WinHttpReadData(request.value, buffer, capacity, &count) != FALSE; },
            [&](const char* buffer, DWORD count) {
                if (!executable) { text.append(buffer, count); return true; }
                DWORD written = 0;
                return WriteFile(file, buffer, count, &written, nullptr) && written == count;
            }, [] { return GetTickCount64(); }, started, package, executable);
    }
    return false;
}
inline bool VerifyFile(const std::wstring& path, const std::string& expected) {
    FileHandle file{CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr)};
    if (file.value == INVALID_HANDLE_VALUE) return false;
    struct Hash {
        BCRYPT_ALG_HANDLE algorithm = nullptr;
        BCRYPT_HASH_HANDLE value = nullptr;
        ~Hash() { if (value) BCryptDestroyHash(value); if (algorithm) BCryptCloseAlgorithmProvider(algorithm, 0); }
    } hash;
    if (BCryptOpenAlgorithmProvider(&hash.algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) < 0 ||
        BCryptCreateHash(hash.algorithm, &hash.value, nullptr, 0, nullptr, 0, 0) < 0) return false;
    unsigned char buffer[65536], digest[32];
    for (;;) {
        DWORD count = 0;
        if (!ReadFile(file.value, buffer, sizeof(buffer), &count, nullptr)) return false;
        if (!count) break;
        if (BCryptHashData(hash.value, buffer, count, 0) < 0) return false;
    }
    if (BCryptFinishHash(hash.value, digest, sizeof(digest), 0) < 0) return false;
    std::string actual;
    constexpr char hex[] = "0123456789abcdef";
    for (const auto byte : digest) { actual += hex[byte >> 4]; actual += hex[byte & 15]; }
    return actual == expected;
}
inline bool Download(const std::string& version, Package& package) {
    const auto url = OpenKeyUpdateSource::SetupUrl(version);
    if (url.empty() || !package.directory.empty()) return false;
    std::string checksum, expected;
    if (!Fetch(url.substr(0, url.find_last_of(L'/') + 1) + L"SHA256SUMS.txt", INVALID_HANDLE_VALUE, checksum, package) ||
        !ExpectedHash(checksum, expected)) return false;
    wchar_t temporary[32768];
    const DWORD length = GetTempPathW(ARRAYSIZE(temporary), temporary);
    if (!length || length >= ARRAYSIZE(temporary)) return false;
    unsigned char random[16];
    if (BCryptGenRandom(nullptr, random, sizeof(random), BCRYPT_USE_SYSTEM_PREFERRED_RNG) < 0) return false;
    std::wstring directory = std::wstring(temporary) + L"OpenKey-update-";
    constexpr wchar_t hex[] = L"0123456789abcdef";
    for (auto byte : random) { directory += hex[byte >> 4]; directory += hex[byte & 15]; }
    if (!CreateDirectoryW(directory.c_str(), nullptr)) return false;
    package.directory = directory;
    package.path = directory + L"\\OpenKey-Setup.exe";
    {
        FileHandle file{CreateFileW(package.path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr)};
        if (file.value == INVALID_HANDLE_VALUE || !Fetch(url, file.value, checksum, package) || !FlushFileBuffers(file.value)) return false;
    }
    package.verified = VerifyFile(package.path, expected);
    if (package.verified) package.expectedHash = expected;
    return package.verified;
}
template<class Execute> bool Launch(Package& package, HWND owner, Execute execute) {
    if (!package.verified || package.launched || package.path.empty() || !VerifyFile(package.path, package.expectedHash)) return false;
    package.launched = execute(owner, package.path.c_str(), package.directory.c_str()) > 32;
    return package.launched;
}
inline bool Launch(Package& package, HWND owner = nullptr) {
    return Launch(package, owner, [](HWND parent, LPCWSTR path, LPCWSTR directory) {
        return reinterpret_cast<INT_PTR>(ShellExecuteW(parent, L"open", path, nullptr, directory, SW_SHOWNORMAL));
    });
}
}
