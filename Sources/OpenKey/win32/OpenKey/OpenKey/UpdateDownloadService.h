#pragma once
#include <windows.h>
#include <winhttp.h>
#include <string>
#pragma comment(lib, "winhttp.lib")

namespace UpdateDownloadService {
enum class Result { Success, Failed, Invalid };
struct NativeApi {
    HINTERNET open() { return WinHttpOpen(L"OpenKey update check", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
        WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0); }
    bool timeouts(HINTERNET session) { return WinHttpSetTimeouts(session, 3000, 5000, 5000, 5000) != FALSE; }
    HINTERNET connect(HINTERNET session) { return WinHttpConnect(session, L"raw.githubusercontent.com", INTERNET_DEFAULT_HTTPS_PORT, 0); }
    HINTERNET request(HINTERNET connection) { return WinHttpOpenRequest(connection, L"GET",
        L"/tuyenvm/OpenKey/master/version.json", nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE); }
    bool configure(HINTERNET request) {
        DWORD policy = WINHTTP_OPTION_REDIRECT_POLICY_NEVER;
        return WinHttpSetOption(request, WINHTTP_OPTION_REDIRECT_POLICY, &policy, sizeof(policy)) != FALSE;
    }
    bool send(HINTERNET request) { return WinHttpSendRequest(request, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
        WINHTTP_NO_REQUEST_DATA, 0, 0, 0) != FALSE; }
    bool receive(HINTERNET request) { return WinHttpReceiveResponse(request, nullptr) != FALSE; }
    bool status(HINTERNET request, DWORD& code) {
        DWORD size = sizeof(code);
        return WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
            WINHTTP_HEADER_NAME_BY_INDEX, &code, &size, WINHTTP_NO_HEADER_INDEX) != FALSE;
    }
    bool read(HINTERNET request, char* buffer, DWORD capacity, DWORD& count) {
        return WinHttpReadData(request, buffer, capacity, &count) != FALSE;
    }
    void close(HINTERNET handle) { WinHttpCloseHandle(handle); }
    ULONGLONG now() { return GetTickCount64(); }
};
template<class Api> Result Download(Api& api, std::wstring& output) {
    output.clear();
    // No shared temporary file; limits apply before growing the payload.
    struct Handle {
        Api& api; HINTERNET value;
        ~Handle() { if (value) api.close(value); }
    };
    const auto started = api.now();
    const auto expired = [&] { return api.now() - started >= 20000; };
    Handle session{ api, api.open() };
    if (!session.value || !api.timeouts(session.value) || expired()) return Result::Failed;
    Handle connection{ api, api.connect(session.value) };
    if (!connection.value || expired()) return Result::Failed;
    Handle request{ api, api.request(connection.value) };
    if (!request.value || !api.configure(request.value) || expired()
        || !api.send(request.value) || expired() || !api.receive(request.value) || expired()) return Result::Failed;
    DWORD status = 0;
    if (!api.status(request.value, status) || status != 200 || expired()) return Result::Failed;
    std::wstring payload;
    char buffer[8192];
    for (;;) {
        DWORD count = 0;
        if (expired() || !api.read(request.value, buffer, sizeof(buffer), count) || expired()) return Result::Failed;
        if (count > sizeof(buffer) || count > 65536 - payload.size()) return Result::Invalid;
        if (count == 0) break;
        for (DWORD i = 0; i < count; ++i) {
            const auto ch = static_cast<unsigned char>(buffer[i]);
            if (ch > 127 || ch == 0) return Result::Invalid;
            payload += static_cast<wchar_t>(ch);
        }
    }
    if (payload.empty()) return Result::Invalid;
    output.swap(payload);
    return Result::Success;
}
inline Result Download(std::wstring& output) { NativeApi api; return Download(api, output); }
}
