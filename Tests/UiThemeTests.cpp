#include <windows.h>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>

namespace themeFixture {
DWORD value = 0, type = REG_DWORD, bytes = sizeof(DWORD);
bool missing = false, openFails = false, queryFails = false, createFails = false, writeFails = false;
int opened = 0, closed = 0, creates = 0, writes = 0, passed = 0;
void require(bool ok) { if (!ok) throw std::runtime_error("Assertion failed"); }
void path(HKEY root, LPCWSTR name) {
    require(root == HKEY_CURRENT_USER && std::wstring(name) == L"SOFTWARE\\TuyenMai\\OpenKey");
}
LSTATUS open(HKEY root, LPCWSTR name, DWORD, REGSAM access, PHKEY key) {
    path(root, name); require(access == KEY_READ);
    if (openFails) return ERROR_ACCESS_DENIED;
    *key = reinterpret_cast<HKEY>(1); ++opened; return ERROR_SUCCESS;
}
LSTATUS query(HKEY, LPCWSTR name, LPDWORD, LPDWORD outType, LPBYTE data, LPDWORD size) {
    require(std::wstring(name) == L"uiTheme");
    if (missing) return ERROR_FILE_NOT_FOUND;
    if (queryFails) return ERROR_ACCESS_DENIED;
    *outType = type;
    if (*size < bytes) { *size = bytes; return ERROR_MORE_DATA; }
    std::memcpy(data, &value, bytes); *size = bytes; return ERROR_SUCCESS;
}
LSTATUS create(HKEY root, LPCWSTR name, DWORD, LPWSTR, DWORD, REGSAM access,
    LPSECURITY_ATTRIBUTES, PHKEY key, LPDWORD disposition) {
    path(root, name); require(access == KEY_SET_VALUE); ++creates;
    if (createFails) return ERROR_ACCESS_DENIED;
    *key = reinterpret_cast<HKEY>(1); *disposition = REG_OPENED_EXISTING_KEY; ++opened; return ERROR_SUCCESS;
}
LSTATUS set(HKEY, LPCWSTR name, DWORD, DWORD dataType, const BYTE* data, DWORD size) {
    require(std::wstring(name) == L"uiTheme" && dataType == REG_DWORD && size == sizeof(DWORD)); ++writes;
    if (writeFails) return ERROR_ACCESS_DENIED;
    std::memcpy(&value, data, size); type = dataType; bytes = size; missing = false; return ERROR_SUCCESS;
}
LSTATUS close(HKEY key) { require(key == reinterpret_cast<HKEY>(1)); ++closed; return ERROR_SUCCESS; }
template<class Action> void test(const char* name, Action action) {
    value = 0; type = REG_DWORD; bytes = sizeof(DWORD);
    missing = openFails = queryFails = createFails = writeFails = false;
    opened = closed = creates = writes = 0;
    try { action(); require(opened == closed); ++passed; }
    catch (...) { std::cerr << "Failed: " << name << '\n'; throw; }
}
}
#define RegOpenKeyExW themeFixture::open
#define RegQueryValueExW themeFixture::query
#define RegCreateKeyExW themeFixture::create
#define RegSetValueExW themeFixture::set
#define RegCloseKey themeFixture::close
#include "../Sources/OpenKey/win32/OpenKey/OpenKey.UI/UiThemeSettings.h"

int main() {
    using namespace themeFixture;
    using namespace OpenKeyUITheme;
    try {
        test("missing defaults to system", [] { missing = true; require(Read() == Theme::System && creates == 0 && writes == 0); });
        test("open denied", [] { openFails = true; require(Read() == Theme::System && opened == 0); });
        test("query denied", [] { queryFails = true; require(Read() == Theme::System); });
        test("system", [] { require(Read() == Theme::System); });
        test("light", [] { value = 1; require(Read() == Theme::Light); });
        test("dark", [] { value = 2; require(Read() == Theme::Dark); });
        test("unknown enum", [] { value = 3; require(Read() == Theme::System); });
        test("maximum DWORD", [] { value = MAXDWORD; require(Read() == Theme::System); });
        test("wrong type", [] { value = 2; type = REG_BINARY; require(Read() == Theme::System); });
        test("short value", [] { value = 2; bytes = 1; require(Read() == Theme::System); });
        test("oversized value", [] { bytes = 8; require(Read() == Theme::System); });
        test("save system", [] { value = 2; require(Save(Theme::System) && Read() == Theme::System && creates == 1 && writes == 1); });
        test("save light", [] { require(Save(Theme::Light) && Read() == Theme::Light && writes == 1); });
        test("save dark", [] { require(Save(Theme::Dark) && Read() == Theme::Dark && writes == 1); });
        test("invalid save no mutation", [] { require(!Save(static_cast<Theme>(3)) && creates == 0 && writes == 0); });
        test("negative enum no mutation", [] { require(!Save(static_cast<Theme>(MAXDWORD)) && creates == 0 && writes == 0); });
        test("create denied", [] { value = 1; createFails = true; require(!Save(Theme::Dark) && value == 1 && writes == 0); });
        test("write denied", [] { value = 1; writeFails = true; require(!Save(Theme::Dark) && value == 1 && writes == 1); });
        test("retry", [] { writeFails = true; require(!Save(Theme::Dark)); writeFails = false; require(Save(Theme::Dark) && Read() == Theme::Dark && writes == 2); });
        test("white foreground dark appearance", [] { require(IsColorLight(255, 255, 255)); });
        test("black foreground light appearance", [] { require(!IsColorLight(0, 0, 0)); });
        test("threshold", [] { require(!IsColorLight(128, 128, 128) && IsColorLight(129, 129, 129)); });
        test("weighted channels", [] { require(IsColorLight(0, 255, 0) && !IsColorLight(255, 0, 0) && !IsColorLight(0, 0, 255)); });
        std::cout << "UI theme regression: " << passed << "/23 passed (fake registry, pure color logic; no UI or hook).\n";
        return 0;
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
