#include <windows.h>
#include <cstring>
#include <iostream>
#include <map>
#include <string>
#include "../Sources/OpenKey/win32/OpenKey/OpenKey/SettingsService.h"

// Compile the real store against in-memory API doubles, never the user's registry.
namespace fixture {
struct Value { DWORD data; DWORD type = REG_DWORD; DWORD size = sizeof(DWORD); };
std::map<std::wstring, Value> values;
bool openFails = false, createFails = false, windowExists = true;
std::wstring failedWrite;
int closes = 0, notifications = 0, checks = 0;
void require(bool condition) {
    if (!condition) throw std::runtime_error("Settings store assertion failed");
}
void reset() {
    values.clear(); openFails = createFails = false; windowExists = true;
    failedWrite.clear(); closes = notifications = 0;
}
void validate(HKEY root, LPCWSTR path, REGSAM access, REGSAM expected) {
    require(root == HKEY_CURRENT_USER);
    require(std::wstring(path) == L"SOFTWARE\\TuyenMai\\OpenKey");
    require(access == expected);
}
LSTATUS open(HKEY root, LPCWSTR path, DWORD, REGSAM access, PHKEY key) {
    validate(root, path, access, KEY_READ);
    if (openFails) return ERROR_ACCESS_DENIED;
    *key = reinterpret_cast<HKEY>(1); return ERROR_SUCCESS;
}
LSTATUS create(HKEY root, LPCWSTR path, DWORD, LPWSTR, DWORD, REGSAM access,
    LPSECURITY_ATTRIBUTES, PHKEY key, LPDWORD disposition) {
    validate(root, path, access, KEY_SET_VALUE);
    if (createFails) return ERROR_ACCESS_DENIED;
    *key = reinterpret_cast<HKEY>(1); *disposition = REG_OPENED_EXISTING_KEY; return ERROR_SUCCESS;
}
LSTATUS query(HKEY, LPCWSTR name, LPDWORD, LPDWORD type, LPBYTE data, LPDWORD size) {
    const auto found = values.find(name);
    if (found == values.end()) return ERROR_FILE_NOT_FOUND;
    *type = found->second.type; *size = found->second.size;
    std::memcpy(data, &found->second.data, sizeof(DWORD)); return ERROR_SUCCESS;
}
LSTATUS set(HKEY, LPCWSTR name, DWORD, DWORD type, const BYTE* data, DWORD size) {
    require(type == REG_DWORD && size == sizeof(DWORD));
    if (failedWrite == name) return ERROR_ACCESS_DENIED;
    DWORD value; std::memcpy(&value, data, sizeof(value)); values[name] = { value };
    return ERROR_SUCCESS;
}
LSTATUS close(HKEY key) { require(key == reinterpret_cast<HKEY>(1)); ++closes; return ERROR_SUCCESS; }
HWND find(LPCWSTR name, LPCWSTR title) {
    require(std::wstring(name) == L"OpenKeyVietnameseInputMethod" && title == nullptr);
    return windowExists ? reinterpret_cast<HWND>(2) : nullptr;
}
BOOL post(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
    require(window == reinterpret_cast<HWND>(2) && message == WM_USER + 2020 && !wparam && !lparam);
    ++notifications; return TRUE;
}
}
#define RegOpenKeyExW fixture::open
#define RegCreateKeyExW fixture::create
#define RegQueryValueExW fixture::query
#define RegSetValueExW fixture::set
#define RegCloseKey fixture::close
#define FindWindowW fixture::find
#define PostMessageW fixture::post
#include "../Sources/OpenKey/win32/OpenKey/OpenKey/SettingsServiceStore.cpp"
#include "../Sources/OpenKey/win32/OpenKey/OpenKey.UI/UiLanguageSettings.h"

struct Mapping { const wchar_t* key; int SettingsService::Preferences::* member; };
#define FIELD(key, member) { L##key, &SettingsService::Preferences::member }
const Mapping mapping[] = {
    FIELD("vLanguage", language), FIELD("vInputType", inputType), FIELD("vCodeTable", codeTable),
    FIELD("vCheckSpelling", checkSpelling), FIELD("vRestoreIfWrongSpelling", restoreIfWrongSpelling),
    FIELD("vRememberCode", rememberCode), FIELD("vUseModernOrthography", useModernOrthography),
    FIELD("vQuickTelex", quickTelex), FIELD("vQuickStartConsonant", quickStartConsonant),
    FIELD("vQuickEndConsonant", quickEndConsonant), FIELD("vAutoCapsMacro", autoCapsMacro),
    FIELD("vUseMacro", useMacro), FIELD("vUseMacroInEnglishMode", useMacroInEnglishMode),
    FIELD("vSwitchKeyStatus", switchKeyStatus), FIELD("vFixRecommendBrowser", fixRecommendBrowser),
    FIELD("vSendKeyStepByStep", sendKeyStepByStep), FIELD("vUseSmartSwitchKey", useSmartSwitchKey),
    FIELD("vSupportMetroApp", supportMetroApp), FIELD("vOtherLanguage", otherLanguage),
    FIELD("vTempOffOpenKey", tempOffOpenKey), FIELD("vFixChromiumBrowser", fixChromiumBrowser),
    FIELD("vUpperCaseFirstChar", upperCaseFirstChar), FIELD("vAllowConsonantZFWJ", allowConsonantZFWJ),
    FIELD("vTempOffSpelling", tempOffSpelling), FIELD("vUseGrayIcon", useGrayIcon),
    FIELD("vRunWithWindows", runWithWindows), FIELD("vShowOnStartUp", showOnStartUp),
    FIELD("vCheckNewVersion", checkNewVersion),
    FIELD("convertToolDontAlertWhenCompleted", convertToolDontAlertWhenCompleted),
    FIELD("convertToolToAllCaps", convertToolToAllCaps), FIELD("convertToolToAllNonCaps", convertToolToAllNonCaps),
    FIELD("convertToolToCapsFirstLetter", convertToolToCapsFirstLetter), FIELD("convertToolToCapsEachWord", convertToolToCapsEachWord),
    FIELD("convertToolRemoveMark", convertToolRemoveMark), FIELD("convertToolFromCode", convertToolFromCode),
    FIELD("convertToolToCode", convertToolToCode), FIELD("convertToolHotKey", convertToolHotKey)
};
int main() {
    using namespace fixture;
    try {
        const SettingsService::Preferences defaults;
        for (const auto& field : mapping) {
            reset();
            require(SettingsService::readPreferences().*field.member == defaults.*field.member);
            values[field.key] = { 17, REG_SZ };
            require(SettingsService::readPreferences().*field.member == defaults.*field.member);
            values[field.key] = { 17, REG_DWORD, 2 };
            require(SettingsService::readPreferences().*field.member == defaults.*field.member);
            for (int value : { 0, 1, 37, static_cast<int>(0xFE0000FEu) }) {
                reset(); auto preferences = defaults; preferences.*field.member = value;
                require(SettingsService::savePreferences(preferences));
                require(values.at(field.key).data == static_cast<DWORD>(value));
                require(values.size() == sizeof(mapping) / sizeof(mapping[0]));
                require(SettingsService::readPreferences().*field.member == value);
                require(closes == 2 && notifications == 1);
            }
            ++checks;
        }
        reset(); openFails = true;
        const auto unreadable = SettingsService::readPreferences();
        for (const auto& field : mapping) require(unreadable.*field.member == defaults.*field.member);
        require(closes == 0); ++checks;
        reset(); createFails = true;
        require(!SettingsService::savePreferences(defaults) && values.empty() && !closes && !notifications); ++checks;
        reset(); failedWrite = L"vLanguage";
        require(!SettingsService::savePreferences(defaults) && closes == 1 && !notifications);
        require(values.count(L"convertToolHotKey") == 1); ++checks;
        reset(); failedWrite = L"convertToolHotKey";
        require(!SettingsService::savePreferences(defaults) && closes == 1 && !notifications); ++checks;
        reset(); windowExists = false;
        require(SettingsService::savePreferences(defaults) && closes == 1 && !notifications); ++checks;
        reset(); values[L"macroData"] = { 123, REG_BINARY }; values[L"unrelated"] = { 456 };
        require(SettingsService::savePreferences(defaults));
        require(values.at(L"macroData").data == 123 && values.at(L"unrelated").data == 456); ++checks;
        // A stale page must only write its edited field, preserving every
        // unrelated value changed externally (hotkey/tray/another page).
        for (const auto& edited : mapping) {
            reset();
            auto baseline = defaults;
            auto desired = defaults;
            desired.*edited.member = 37;
            for (const auto& field : mapping) values[field.key] = { 99 };
            values[L"macroData"] = { 123, REG_BINARY };
            require(SettingsService::saveChangedPreferences(baseline, desired));
            for (const auto& field : mapping) {
                require(values.at(field.key).data == (field.member == edited.member ? 37u : 99u));
                require(baseline.*field.member == desired.*field.member);
            }
            require(values.at(L"macroData").data == 123 && closes == 1 && notifications == 1);
            ++checks;
        }
        reset(); createFails = true;
        auto baseline = defaults;
        auto desired = defaults;
        require(SettingsService::saveChangedPreferences(baseline, desired));
        require(values.empty() && closes == 0 && notifications == 0); ++checks;
        desired.language = 0;
        require(!SettingsService::saveChangedPreferences(baseline, desired));
        require(baseline.language == defaults.language && !notifications); ++checks;
        reset(); failedWrite = L"vLanguage";
        desired.codeTable = 4;
        require(!SettingsService::saveChangedPreferences(baseline, desired));
        require(baseline.language == defaults.language && baseline.codeTable == defaults.codeTable);
        require(values.at(L"vCodeTable").data == 4 && closes == 1 && !notifications);
        failedWrite.clear();
        require(SettingsService::saveChangedPreferences(baseline, desired));
        require(values.at(L"vLanguage").data == 0 && baseline.codeTable == 4 && notifications == 1); ++checks;
        reset(); failedWrite = L"vLanguage";
        baseline = desired = defaults; desired.inputType = 2;
        require(SettingsService::saveChangedPreferences(baseline, desired));
        require(values.size() == 1 && values.at(L"vInputType").data == 2 && notifications == 1); ++checks;
        reset(); windowExists = false;
        baseline = desired = defaults; desired.language = 0;
        require(SettingsService::saveChangedPreferences(baseline, desired));
        require(values.size() == 1 && closes == 1 && notifications == 0); ++checks;
        std::cout << "Settings store: " << checks << "/" << checks << " passed (in-memory APIs only)\n";
        int uiChecks = 0;
        using OpenKeyUILanguage::Language;
        reset(); require(OpenKeyUILanguage::Read() == Language::Vietnamese && closes == 1); ++uiChecks;
        reset(); openFails = true;
        require(OpenKeyUILanguage::Read() == Language::Vietnamese && closes == 0); ++uiChecks;
        reset(); values[L"uiDisplayLanguage"] = { 7 };
        require(OpenKeyUILanguage::Read() == Language::Vietnamese); ++uiChecks;
        reset(); values[L"uiDisplayLanguage"] = { 1, REG_SZ };
        require(OpenKeyUILanguage::Read() == Language::Vietnamese); ++uiChecks;
        reset(); values[L"uiDisplayLanguage"] = { 1, REG_DWORD, 2 };
        require(OpenKeyUILanguage::Read() == Language::Vietnamese); ++uiChecks;
        reset(); values[L"vLanguage"] = { 42 }; values[L"macroData"] = { 123, REG_BINARY };
        require(OpenKeyUILanguage::Save(Language::English));
        require(OpenKeyUILanguage::Read() == Language::English && closes == 2 && !notifications);
        require(values.at(L"vLanguage").data == 42 && values.at(L"macroData").data == 123); ++uiChecks;
        require(OpenKeyUILanguage::Save(Language::Vietnamese));
        require(OpenKeyUILanguage::Read() == Language::Vietnamese && !notifications); ++uiChecks;
        reset(); failedWrite = L"uiDisplayLanguage";
        require(!OpenKeyUILanguage::Save(Language::English) && closes == 1 && values.empty() && !notifications); ++uiChecks;
        reset(); createFails = true;
        require(!OpenKeyUILanguage::Save(Language::English) && !closes && values.empty()); ++uiChecks;
        reset(); require(!OpenKeyUILanguage::Save(static_cast<Language>(2)) && !closes && values.empty()); ++uiChecks;
        require(std::wstring(OpenKeyUILanguage::Tag(Language::Vietnamese)) == L"vi-VN");
        require(std::wstring(OpenKeyUILanguage::Tag(Language::English)) == L"en-US"); ++uiChecks;
        reset(); require(OpenKeyUILanguage::Save(Language::English));
        require(SettingsService::savePreferences(defaults));
        require(OpenKeyUILanguage::Read() == Language::English); ++uiChecks;
        std::cout << "UI language settings: " << uiChecks << "/" << uiChecks << " passed (in-memory APIs only)\n";
        return 0;
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
