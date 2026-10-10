// Header declarations are loaded before store API substitutions.
#include "../Sources/OpenKey/win32/OpenKey/OpenKey/stdafx.h"
#include "../Sources/OpenKey/engine/ConvertTool.h"
#include <functional>
#define main RunStoreTests
#include "SettingsStoreTests.cpp"
#undef main

#define INT_GLOBAL(name) int name = -99
INT_GLOBAL(vLanguage); INT_GLOBAL(vInputType); INT_GLOBAL(vFreeMark); INT_GLOBAL(vCodeTable);
INT_GLOBAL(vCheckSpelling); INT_GLOBAL(vUseModernOrthography); INT_GLOBAL(vQuickTelex);
INT_GLOBAL(vSwitchKeyStatus); INT_GLOBAL(vRestoreIfWrongSpelling); INT_GLOBAL(vFixRecommendBrowser);
INT_GLOBAL(vUseMacro); INT_GLOBAL(vUseMacroInEnglishMode); INT_GLOBAL(vAutoCapsMacro);
INT_GLOBAL(vSendKeyStepByStep); INT_GLOBAL(vUseSmartSwitchKey); INT_GLOBAL(vUpperCaseFirstChar);
INT_GLOBAL(vUseGrayIcon); INT_GLOBAL(vShowOnStartUp); INT_GLOBAL(vRunWithWindows);
INT_GLOBAL(vSupportMetroApp); INT_GLOBAL(vCreateDesktopShortcut); INT_GLOBAL(vRunAsAdmin);
INT_GLOBAL(vCheckNewVersion); INT_GLOBAL(vRememberCode); INT_GLOBAL(vOtherLanguage);
INT_GLOBAL(vTempOffOpenKey); INT_GLOBAL(vFixChromiumBrowser); INT_GLOBAL(vAllowConsonantZFWJ);
INT_GLOBAL(vTempOffSpelling); INT_GLOBAL(vQuickStartConsonant); INT_GLOBAL(vQuickEndConsonant);
bool convertToolDontAlertWhenCompleted = false, convertToolToAllCaps = false;
bool convertToolToAllNonCaps = false, convertToolToCapsFirstLetter = false;
bool convertToolToCapsEachWord = false, convertToolRemoveMark = false;
Uint8 convertToolFromCode = 0, convertToolToCode = 0;
INT_GLOBAL(convertToolHotKey);
int helperWrites = 0;
int OpenKeyHelper::getRegInt(LPCTSTR key, const int& fallback) {
    const auto found = fixture::values.find(key);
    return fixture::openFails || found == fixture::values.end() ? fallback : static_cast<int>(found->second.data);
}
void OpenKeyHelper::setRegInt(LPCTSTR key, const int& value) {
    ++helperWrites;
    fixture::values[key] = { static_cast<DWORD>(value) };
}
#include "../Sources/OpenKey/win32/OpenKey/OpenKey/SettingsService.cpp"

struct EngineMapping { std::function<int()> actual; int fallback; int (*normalize)(int); };
int identity(int value) { return value; }
int boolean(int value) { return value != 0; }
int byte(int value) { return static_cast<Uint8>(value); }
#define MAP(global, member) { [] { return static_cast<int>(global); }, defaults.member, identity }
#define BOOL_MAP(global) { [] { return static_cast<int>(global); }, defaults.global, boolean }
int main() {
    using namespace fixture;
    if (RunStoreTests()) return 1;
    try {
        const SettingsService::Preferences defaults;
        const EngineMapping engine[] = {
            MAP(vLanguage, language), MAP(vInputType, inputType), MAP(vCodeTable, codeTable),
            MAP(vCheckSpelling, checkSpelling), MAP(vRestoreIfWrongSpelling, restoreIfWrongSpelling),
            MAP(vRememberCode, rememberCode), MAP(vUseModernOrthography, useModernOrthography),
            MAP(vQuickTelex, quickTelex), MAP(vQuickStartConsonant, quickStartConsonant),
            MAP(vQuickEndConsonant, quickEndConsonant), MAP(vAutoCapsMacro, autoCapsMacro),
            { [] { return vSwitchKeyStatus; }, 0x7A000206, identity }, // Preserve legacy load fallback.
            MAP(vFixRecommendBrowser, fixRecommendBrowser), MAP(vSendKeyStepByStep, sendKeyStepByStep),
            MAP(vUseSmartSwitchKey, useSmartSwitchKey), MAP(vSupportMetroApp, supportMetroApp),
            MAP(vOtherLanguage, otherLanguage), MAP(vTempOffOpenKey, tempOffOpenKey),
            MAP(vFixChromiumBrowser, fixChromiumBrowser), MAP(vUpperCaseFirstChar, upperCaseFirstChar),
            MAP(vAllowConsonantZFWJ, allowConsonantZFWJ), MAP(vTempOffSpelling, tempOffSpelling),
            MAP(vUseGrayIcon, useGrayIcon), MAP(vRunWithWindows, runWithWindows),
            MAP(vShowOnStartUp, showOnStartUp), MAP(vCheckNewVersion, checkNewVersion),
            BOOL_MAP(convertToolDontAlertWhenCompleted), BOOL_MAP(convertToolToAllCaps),
            BOOL_MAP(convertToolToAllNonCaps), BOOL_MAP(convertToolToCapsFirstLetter),
            BOOL_MAP(convertToolToCapsEachWord), BOOL_MAP(convertToolRemoveMark),
            { [] { return static_cast<int>(convertToolFromCode); }, defaults.convertToolFromCode, byte },
            { [] { return static_cast<int>(convertToolToCode); }, defaults.convertToolToCode, byte },
            { [] { return convertToolHotKey; }, defaults.convertToolHotKey,
                [](int value) { return value ? value : SettingsService::EmptyHotKey; } }
        };
        constexpr size_t count = sizeof(mapping) / sizeof(mapping[0]);
        static_assert(count == sizeof(engine) / sizeof(engine[0]), "Mapping test tables must match");
        int passed = 0;
        auto verify = [&](size_t selected, int value) {
            for (size_t index = 0; index < count; ++index) {
                const int expected = index == selected ? engine[index].normalize(value) : engine[index].fallback;
                if (engine[index].actual() != expected) {
                    std::wcerr << L"Mapping mismatch: " << mapping[index].key << '\n';
                    require(false);
                }
            }
            require(vFreeMark == 0 && helperWrites == 0 && notifications == 0);
        };
        reset(); SettingsService::load(); verify(count, 0); ++passed;
        for (size_t index = 0; index < count; ++index) {
            for (int value : { 0, 1, 37, 257, static_cast<int>(0xFE0000FEu) }) {
                reset(); values[mapping[index].key] = { static_cast<DWORD>(value) };
                SettingsService::load(); verify(index, value);
                // Loading an empty store again must overwrite stale globals.
                reset(); SettingsService::load(); verify(count, 0);
            }
            ++passed;
        }
        struct Extra { const wchar_t* key; int* global; int fallback; };
        const Extra extras[] = {
            { L"vUseMacro", &vUseMacro, 1 }, { L"vUseMacroInEnglishMode", &vUseMacroInEnglishMode, 0 },
            { L"vRunAsAdmin", &vRunAsAdmin, 0 }, { L"vCreateDesktopShortcut", &vCreateDesktopShortcut, 0 }
        };
        for (const auto& extra : extras) {
            reset(); values[extra.key] = { 37 }; SettingsService::load(); require(*extra.global == 37);
            reset(); SettingsService::load(); require(*extra.global == extra.fallback); ++passed;
        }
        reset(); openFails = true; SettingsService::load(); verify(count, 0); ++passed;
        std::cout << "Settings engine mapping: " << passed << "/" << passed
            << " passed (no registry/hook/runtime message dispatch)\n";

        // Pin the legacy reset contract, including settings intentionally not reset.
        reset();
        for (const auto& field : mapping) values[field.key] = { 37 };
        for (const auto& extra : extras) values[extra.key] = { 37 };
        values[L"macroData"] = { 123, REG_BINARY };
        values[L"unrelated"] = { 456 };
        values[L"vFreeMark"] = { 77 };
        SettingsService::load();
        vFreeMark = 37;
        helperWrites = 0;
        const auto beforeReset = values;
        SettingsService::restoreDefaults();
        int resetPassed = 0;
        int expectedWrites = 0;
        for (size_t index = 0; index < count; ++index) {
            const std::wstring key = mapping[index].key;
            if (key == L"vQuickStartConsonant" || key == L"vQuickEndConsonant" || key == L"vCheckNewVersion") continue;
            int expected = defaults.*mapping[index].member;
            if (key == L"vSupportMetroApp") expected = 1; // Reset differs from first-load default.
            require(engine[index].actual() == expected);
            require(values.at(key).data == static_cast<DWORD>(expected));
            require(values.at(key).type == REG_DWORD && values.at(key).size == sizeof(DWORD));
            ++resetPassed; ++expectedWrites;
        }
        require(vUseMacro == 0 && values.at(L"vUseMacro").data == 0); ++resetPassed; ++expectedWrites;
        require(vUseMacroInEnglishMode == 0 && values.at(L"vUseMacroInEnglishMode").data == 0); ++resetPassed; ++expectedWrites;
        require(helperWrites == expectedWrites && values.size() == beforeReset.size() && !notifications);
        require(vFreeMark == 0); ++resetPassed;
        for (const wchar_t* key : { L"vQuickStartConsonant", L"vQuickEndConsonant", L"vCheckNewVersion",
            L"vRunAsAdmin", L"vCreateDesktopShortcut", L"macroData", L"unrelated", L"vFreeMark" }) {
            const auto& actual = values.at(key); const auto& old = beforeReset.at(key);
            require(actual.data == old.data && actual.type == old.type && actual.size == old.size);
        }
        require(vQuickStartConsonant == 37 && vQuickEndConsonant == 37 && vCheckNewVersion == 37);
        require(vRunAsAdmin == 37 && vCreateDesktopShortcut == 37); ++resetPassed;

        const auto afterReset = values;
        helperWrites = 0;
        SettingsService::restoreDefaults();
        require(helperWrites == expectedWrites && values.size() == afterReset.size());
        for (const auto& value : afterReset) {
            const auto& actual = values.at(value.first);
            require(actual.data == value.second.data && actual.type == value.second.type && actual.size == value.second.size);
        }
        ++resetPassed;
        helperWrites = 0;
        SettingsService::load();
        for (size_t index = 0; index < count; ++index) {
            require(engine[index].actual() == engine[index].normalize(static_cast<int>(afterReset.at(mapping[index].key).data)));
        }
        require(vUseMacro == 0 && vUseMacroInEnglishMode == 0 && vFreeMark == 0 && !helperWrites && !notifications);
        ++resetPassed;
        std::cout << "Settings restore defaults: " << resetPassed << "/" << resetPassed
            << " passed (legacy behavior; in-memory persistence only)\n";
        return 0;
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
