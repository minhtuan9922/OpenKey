#include <windows.h>
#include <cstring>
#include <cwchar>
#include <iostream>
#include <stdexcept>
namespace Fixture {
    int checks = 0, opened = 0, closed = 0, writes = 0;
    bool missing = false, badRead = false, badCreate = false, badWrite = false;
    DWORD value = 0, type = REG_DWORD, bytes = sizeof(DWORD);
    void Check(bool ok, const char* label) { if (!ok) throw std::runtime_error(label); ++checks; }
    void Path(HKEY root, LPCWSTR path) { Check(root == HKEY_CURRENT_USER && !wcscmp(path, L"SOFTWARE\\TuyenMai\\OpenKey"), "storage stays in the current user's application key"); }
    LSTATUS Open(HKEY root, LPCWSTR path, DWORD options, REGSAM access, PHKEY key) {
        Path(root, path); Check(!options && access == KEY_QUERY_VALUE, "read requests query-only access");
        if (missing) return ERROR_FILE_NOT_FOUND; *key = reinterpret_cast<HKEY>(1); ++opened; return ERROR_SUCCESS;
    }
    LSTATUS Query(HKEY key, LPCWSTR name, LPDWORD, LPDWORD outType, LPBYTE data, LPDWORD size) {
        Check(key == reinterpret_cast<HKEY>(1) && !wcscmp(name, L"caretProfileSelected"), "read only the new profile choice");
        if (badRead) return ERROR_ACCESS_DENIED;
        std::memcpy(data, &value, (std::min)(*size, static_cast<DWORD>(sizeof(value)))); *outType = type; *size = bytes; return ERROR_SUCCESS;
    }
    LSTATUS Create(HKEY root, LPCWSTR path, DWORD, LPWSTR, DWORD options, REGSAM access, const LPSECURITY_ATTRIBUTES, PHKEY key, LPDWORD disposition) {
        Path(root, path); Check(options == REG_OPTION_NON_VOLATILE && access == KEY_SET_VALUE, "writes are persistent and value-only");
        if (badCreate) return ERROR_ACCESS_DENIED; *key = reinterpret_cast<HKEY>(1); *disposition = 0; ++opened; return ERROR_SUCCESS;
    }
    LSTATUS Set(HKEY key, LPCWSTR name, DWORD reserved, DWORD kind, const BYTE* data, DWORD size) {
        Check(key == reinterpret_cast<HKEY>(1) && !wcscmp(name, L"caretProfileSelected") && !reserved && kind == REG_DWORD && size == sizeof(DWORD), "never write other preferences or types");
        if (badWrite) return ERROR_ACCESS_DENIED;
        std::memcpy(&value, data, sizeof(value)); type = kind; bytes = size; ++writes; return ERROR_SUCCESS;
    }
    LSTATUS Close(HKEY key) { Check(key == reinterpret_cast<HKEY>(1), "only close our handle"); ++closed; return ERROR_SUCCESS; }
}
#define RegOpenKeyExW Fixture::Open
#define RegQueryValueExW Fixture::Query
#define RegCreateKeyExW Fixture::Create
#define RegSetValueExW Fixture::Set
#define RegCloseKey Fixture::Close
#include "../Sources/OpenKey/win32/OpenKey/OpenKey/CaretSelectionSettings.h"
int main() {
    using namespace Fixture;
    using namespace OpenKeyCaretSettings;
    try {
        missing = true; Check(Read() == Choice::Unknown && !writes, "missing preference does not enable VIE or write a default"); missing = false;
        for (DWORD selected : {0u, 1u}) { value = selected; Check(Read() == static_cast<Choice>(selected), "valid choices round-trip"); }
        value = 2; Check(Read() == Choice::Unknown, "out-of-range data is ignored");
        value = 1; type = REG_SZ; Check(Read() == Choice::Unknown, "wrong registry type is ignored"); type = REG_DWORD;
        bytes = 2; Check(Read() == Choice::Unknown, "truncated DWORD is ignored"); bytes = 8; Check(Read() == Choice::Unknown, "oversized value is ignored"); bytes = sizeof(DWORD);
        badRead = true; Check(Read() == Choice::Unknown, "query failure cannot imply VIE"); badRead = false;
        Check(Save(true) && value == 1 && Save(false) && value == 0, "store both selected and inactive choices");
        badCreate = true; Check(!Save(true) && value == 0, "creation failure preserves existing value"); badCreate = false;
        badWrite = true; Check(!Save(true) && value == 0, "write failure preserves existing value"); badWrite = false;
        Check(opened == closed, "all handles close on success and failure");
        std::cout << checks << " caret profile storage checks passed (fake registry only).\n"; return 0;
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
