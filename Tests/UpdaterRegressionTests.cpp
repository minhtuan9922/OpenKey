// Exercise the actual updater helpers, never its download/UI entry point.
#include "../Sources/OpenKey/win32/OpenKey/OpenKeyUpdate/OpenKeyUpdate.cpp"
#include <iostream>

std::string ReadBytes(const std::wstring& path) {
    std::ifstream input(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
}

int wmain(int argc, wchar_t** argv) {
    if (argc == 3 && std::wstring(argv[1]) == L"--policy") {
        const bool expectedAllowed = std::wstring(argv[2]) == L"allow";
        return OpenKeyUpdatePolicy::CanUseLegacyUpdater() == expectedAllowed ? 0 : 1;
    }
    if (argc != 2) return 2;
    const std::wstring root = argv[1];
    struct Case { const wchar_t* name; bool expected; bool lockTarget; };
    const Case cases[] = {
        {L"valid", true, false},
        {L"missing", false, false},
        {L"corrupt", false, false},
        {L"locked", false, true},
        {L"existing-stage", false, false},
        {L"Unicode tiếng Việt spaces", true, false},
        {L"wrong-name", false, false},
        {L"wrong-machine", false, false},
        {L"invalid-pe", false, false},
        {L"both-architectures", true, false},
    };
    int failures = 0;
    struct MetadataCase { std::wstring input; const wchar_t* expected; };
    const MetadataCase metadataCases[] = {
        {L"{\"latestWinVersion\":{\"versionName\":\"2.0.2\",\"versionCode\":131074}}", L"2.0.2"},
        {L"{\"latestVersion\":{\"versionName\":\"9.9.9\"},\"latestWinVersion\":{\"versionName\":\"2.0.2\"}}", L"2.0.2"},
        {L"{\"latestWinVersion\" : { \"versionCode\":1, \"versionName\" : \"v2.1-beta_1\" }}", L"v2.1-beta_1"},
        {L"", nullptr},
        {L"{\"latestVersion\":{\"versionName\":\"2.0.2\"}}", nullptr},
        {L"{\"latestWinVersion\":{},\"other\":{\"versionName\":\"2.0.2\"}}", nullptr},
        {L"{\"latestWinVersion\" {\"versionName\":\"2.0.2\"}}", nullptr},
        {L"{\"latestWinVersion\":{\"versionName\":123}}", nullptr},
        {L"{\"latestWinVersion\":{\"versionName\":\"\"}}", nullptr},
        {L"{\"latestWinVersion\":{\"versionName\":\"unterminated}}", nullptr},
        {L"{\"latestWinVersion\":{\"versionName\":\"../escape\"}}", nullptr},
        {L"{\"latestWinVersion\":{\"versionName\":\"2.0?query\"}}", nullptr},
        {L"{\"latestWinVersion\":{\"versionName\":\"" + std::wstring(65, L'a') + L"\"}}", nullptr},
        {L"{\"latestWinVersion\":{\"versionName\":\"1\",\"versionName\":\"2\"}}", nullptr},
        {std::wstring(65537, L' '), nullptr},
        {L"{\"latestWinVersion\":{\"versionName\":\"..\"}}", nullptr},
    };
    for (const auto& test : metadataCases) {
        std::wstring version = L"previous-value";
        const bool result = ReadUpdateVersion(test.input, version);
        const bool passed = test.expected ? result && version == test.expected : !result && version.empty();
        if (!passed) {
            std::cout << "FAIL metadata case " << (&test - metadataCases + 1) << "\n";
            ++failures;
        }
    }
    if (failures == 0) std::cout << "PASS 16 metadata cases\n";
    for (const auto& test : cases) {
        const std::wstring directory = root + L"\\" + test.name;
        const std::wstring target = directory + L"\\" + UpdateExecutableName;
        const std::wstring otherTarget = directory +
            (UpdateExecutableMachine == IMAGE_FILE_MACHINE_AMD64 ? L"\\OpenKey32.exe" : L"\\OpenKey64.exe");
        const auto original = ReadBytes(target);
        if (original != "old-fixture") return 3; // Reject non-fixture destinations.
        HANDLE locked = INVALID_HANDLE_VALUE;
        if (test.lockTarget) {
            locked = CreateFileW(target.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
            if (locked == INVALID_HANDLE_VALUE) return 4;
        }
        const bool result = InstallDownloadedUpdate(directory.c_str(), L"stage-test");
        if (locked != INVALID_HANDLE_VALUE) CloseHandle(locked);
        const auto finalBytes = ReadBytes(target);
        const bool correctBytes = test.expected ?
            finalBytes.size() == 81 && finalBytes.substr(70) == "new-fixture" && IsMatchingUpdateExecutable(target) :
            finalBytes == "old-fixture";
        const bool passed = result == test.expected && correctBytes && ReadBytes(otherTarget) == "other-old-fixture";
        std::cout << (passed ? "PASS " : "FAIL ") << "case " << (&test - cases + 1)
            << "\n";
        if (!passed) ++failures;
    }
    return failures == 0 ? 0 : 1;
}
