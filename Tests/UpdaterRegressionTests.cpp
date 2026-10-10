// Exercise the actual updater helpers, never its download/UI entry point.
#include "../Sources/OpenKey/win32/OpenKey/OpenKeyUpdate/OpenKeyUpdate.cpp"
#include <iostream>

std::string ReadBytes(const std::wstring& path) {
    std::ifstream input(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
}

#include "../Sources/OpenKey/win32/OpenKey/OpenKey/UpdateLaunchService.h"
#include "../Sources/OpenKey/win32/OpenKey/OpenKey/UpdateCheckService.h"
#include "../Sources/OpenKey/win32/OpenKey/OpenKey/UpdateDownloadService.h"
#include "../Sources/OpenKey/win32/OpenKey/OpenKey/AsyncUpdateCheck.h"
#include "../Sources/OpenKey/win32/OpenKey/OpenKey/UpdateService.h"
#include "../Sources/OpenKey/win32/OpenKey/OpenKey.UI/UpdateRequestState.h"
#include <future>
#include <chrono>

struct FakeUpdateHttp {
    int fail = 0, opened = 0, closed = 0, readCount = 0;
    DWORD httpStatus = 200, chunk = 8192;
    std::string body = "{\"latestWinVersion\":{\"versionName\":\"2\",\"versionCode\":2}}";
    size_t position = 0;
    ULONGLONG ticks = 0, step = 0;
    std::vector<int> closeOrder;
    HINTERNET handle(int stage) { if (fail == stage) return nullptr; ++opened; return reinterpret_cast<HINTERNET>(static_cast<INT_PTR>(stage)); }
    HINTERNET open() { return handle(1); }
    bool timeouts(HINTERNET) { return fail != 2; }
    HINTERNET connect(HINTERNET) { return handle(3); }
    HINTERNET request(HINTERNET) { return handle(4); }
    bool configure(HINTERNET) { return fail != 5; }
    bool send(HINTERNET) { return fail != 6; }
    bool receive(HINTERNET) { return fail != 7; }
    bool status(HINTERNET, DWORD& code) { code = httpStatus; return fail != 8; }
    bool read(HINTERNET, char* buffer, DWORD capacity, DWORD& count) {
        ++readCount;
        ticks += step;
        if (fail == 9 || (fail == 10 && readCount == 2)) return false;
        if (fail == 11) { count = capacity + 1; return true; }
        count = static_cast<DWORD>((std::min)(size_t(chunk), body.size() - position));
        std::copy_n(body.data() + position, count, buffer);
        position += count;
        return true;
    }
    void close(HINTERNET handle) { ++closed; closeOrder.push_back(static_cast<int>(reinterpret_cast<INT_PTR>(handle))); }
    ULONGLONG now() { return ticks; }
};

int TestSharedUpdateService() {
    int failures = 0, checks = 0;
    using Result = UpdateCheckService::Result;
    struct VersionCase { uint32_t packed; const char* name; Result expected; };
    for (const auto& test : {
        VersionCase{0x00020002, "2.0.2", Result::UpToDate},
        VersionCase{0x00050002, "2.0.5", Result::UpToDate},
        VersionCase{0x00040002, "2.0.4", Result::UpToDate},
        VersionCase{0x00060002, "2.0.6", Result::Available},
        VersionCase{0x00000102, "2.1.0", Result::Available},
        VersionCase{0x00000003, "3.0.0", Result::Available},
        VersionCase{0x00FFFF01, "1.255.255", Result::UpToDate} }) {
        FakeUpdateHttp api;
        api.body = "{\"latestWinVersion\":{\"versionName\":\"" + std::string(test.name)
            + "\",\"versionCode\":" + std::to_string(test.packed) + "}}";
        const auto outcome = UpdateService::Check(0x00050002, api);
        if (outcome.result != test.expected || outcome.version != test.name || api.opened != api.closed) ++failures;
        ++checks;
    }
    for (int failure = 1; failure <= 11; ++failure) {
        FakeUpdateHttp api; api.fail = failure; api.chunk = 1;
        const auto outcome = UpdateService::Check(0x00050002, api);
        if (outcome.result != (failure == 11 ? Result::InvalidMetadata : Result::DownloadFailed)
            || !outcome.version.empty() || api.opened != api.closed) ++failures;
        ++checks;
    }
    for (const auto& body : { std::string("{}"), std::string(), std::string(65537, 'a'),
        std::string("{\"latestWinVersion\":{\"versionName\":\"2\"},\"other\":{\"versionCode\":3}}"),
        std::string("{\"latestWinVersion\":{\"versionName\":\"2\",\"versionCode\":-1}}") }) {
        FakeUpdateHttp api; api.body = body;
        const auto outcome = UpdateService::Check(0x00050002, api);
        if (outcome.result != Result::InvalidMetadata || !outcome.version.empty()) ++failures;
        ++checks;
    }
    UpdateRequestState state;
    uint64_t ticket = 0, duplicate = 0;
    auto require = [&](bool passed) { if (!passed) ++failures; ++checks; };
    require(!state.IsLoaded() && !state.Busy() && !state.Begin(ticket));
    state.Loaded();
    require(state.IsLoaded() && state.Begin(ticket) && state.Busy() && state.Current(ticket));
    require(!state.Begin(duplicate) && state.Busy());
    require(!state.Finish(ticket + 1) && state.Busy());
    require(state.Finish(ticket) && !state.Busy());
    require(!state.Finish(ticket));
    const auto prior = ticket;
    require(state.Begin(ticket) && ticket != prior && !state.Current(prior));
    state.Unloaded();
    require(!state.IsLoaded() && state.Busy() && !state.Current(ticket) && !state.Begin(duplicate));
    state.Loaded();
    require(state.IsLoaded() && state.Busy() && !state.Current(ticket) && !state.Begin(duplicate));
    require(!state.Finish(ticket) && !state.Busy());
    require(state.Begin(ticket) && state.Current(ticket));
    state.Unloaded();
    require(!state.Finish(ticket) && !state.Busy() && !state.Current(ticket));
    state.Loaded();
    require(state.Begin(ticket) && state.Finish(ticket));
    state.Unloaded();
    require(!state.Current(ticket)); // Late dialog/launcher continuation.
    uint64_t link = 0, newerLink = 0;
    require(!state.BeginStatus(link) && !state.Busy());
    state.Loaded();
    require(state.BeginStatus(link) && state.Current(link) && !state.Busy());
    require(state.BeginStatus(newerLink) && !state.Current(link) && state.Current(newerLink));
    require(state.Begin(ticket) && state.Busy() && !state.Current(newerLink));
    require(state.BeginStatus(link) && state.Busy() && !state.Current(ticket) && state.Current(link));
    require(!state.Begin(duplicate) && state.Current(link)); // Still only one download.
    require(!state.Finish(ticket) && !state.Busy() && state.Current(link)); // Stale update still cleaned up.
    require(state.Begin(ticket) && state.Finish(ticket) && !state.Current(link));
    require(state.BeginStatus(link) && !state.Current(ticket)); // Invalidates dialog continuation too.
    state.Unloaded();
    require(!state.Current(link) && !state.BeginStatus(newerLink));
    state.Loaded();
    require(!state.Current(link) && state.BeginStatus(newerLink) && state.Current(newerLink));
    require(state.Begin(ticket) && state.Busy());
    state.Unloaded();
    state.Loaded();
    require(state.BeginStatus(link) && state.Busy() && !state.Finish(ticket) && !state.Busy()
        && state.Current(link)); // Reloaded page retains latest link result, not old worker result.
    require(state.Begin(ticket) && state.Busy());
    require(!state.Begin(duplicate) && state.Current(ticket)); // Confirmation still owns the workflow.
    require(!state.Begin(duplicate) && state.Busy()); // Launcher pending: no premature Finish.
    require(state.Finish(ticket) && !state.Busy());
    require(state.Begin(ticket) && state.Finish(ticket)); // Cancel/error/success cleanup permits retry.
    require(state.Begin(ticket) && state.Busy());
    require(state.BeginStatus(link) && state.Busy() && !state.Current(ticket));
    require(!state.Begin(duplicate) && state.Current(link));
    require(!state.Finish(ticket) && !state.Busy() && state.Current(link));
    require(state.Begin(ticket));
    state.Unloaded();
    state.Loaded();
    require(state.Busy() && !state.Current(ticket) && !state.Begin(duplicate));
    require(!state.Finish(ticket) && !state.Busy() && state.Begin(ticket) && state.Finish(ticket));
    if (!failures) std::cout << "PASS " << checks << " shared update/lifecycle cases (no network/XAML)\n";
    else std::cerr << "FAIL shared update/lifecycle: " << failures << "\n";
    return failures;
}

int TestBackgroundUpdateChecks() {
    int failures = 0, checks = 0;
    using DownloadResult = UpdateDownloadService::Result;
    auto verify = [&](FakeUpdateHttp api, DownloadResult expected) {
        std::wstring output = L"stale";
        const auto result = UpdateDownloadService::Download(api, output);
        if (result != expected || api.opened != api.closed
            || (expected != DownloadResult::Success && !output.empty())) ++failures;
        if (expected == DownloadResult::Success && std::wstring(api.body.begin(), api.body.end()) != output) ++failures;
        if (!std::is_sorted(api.closeOrder.begin(), api.closeOrder.end(), std::greater<int>())) ++failures;
        ++checks;
    };
    verify(FakeUpdateHttp{}, DownloadResult::Success);
    for (int stage = 1; stage <= 11; ++stage) {
        FakeUpdateHttp api; api.fail = stage; api.chunk = 1;
        verify(api, stage == 11 ? DownloadResult::Invalid : DownloadResult::Failed);
    }
    for (DWORD status : { DWORD(204), DWORD(301), DWORD(302), DWORD(404), DWORD(500) }) {
        FakeUpdateHttp api; api.httpStatus = status; verify(api, DownloadResult::Failed);
    }
    for (size_t length : { size_t(0), size_t(65536), size_t(65537) }) {
        FakeUpdateHttp api; api.body.assign(length, 'a');
        verify(api, length == 65536 ? DownloadResult::Success : DownloadResult::Invalid);
    }
    for (char ch : { char(0), char(0x80), char(0xFF) }) {
        FakeUpdateHttp api; api.body += ch; verify(api, DownloadResult::Invalid);
    }
    for (DWORD chunk : { DWORD(1), DWORD(7), DWORD(8192) }) {
        FakeUpdateHttp api; api.chunk = chunk; verify(api, DownloadResult::Success);
    }
    FakeUpdateHttp slow; slow.step = 10000; slow.chunk = 1;
    verify(slow, DownloadResult::Failed);

    AsyncUpdateCheck pending;
    AsyncUpdateCheck::Outcome output;
    std::function<void()> run;
    const auto work = [] { return AsyncUpdateCheck::Outcome{ UpdateCheckService::Result::Available, "2" }; };
    if (pending.Busy() || pending.Poll(output)) ++failures; ++checks;
    if (pending.Start(work, [](std::function<void()>) { return false; }) || pending.Busy()) ++failures; ++checks;
    if (pending.Start(work, [](std::function<void()>) -> bool { throw 1; }) || pending.Busy()) ++failures; ++checks;
    if (!pending.Start(work, [&](std::function<void()> task) { run = std::move(task); return true; })
        || !pending.Busy() || !pending.Token() || pending.Poll(output)) ++failures; ++checks;
    if (pending.Start(work, [](std::function<void()>) { return true; })) ++failures; ++checks;
    run();
    if (!pending.Poll(output) || output.result != UpdateCheckService::Result::Available || output.version != "2"
        || pending.Busy() || pending.Poll(output)) ++failures; ++checks;
    if (!pending.Start([]() -> AsyncUpdateCheck::Outcome { throw 1; },
        [&](std::function<void()> task) { run = std::move(task); return true; })) ++failures;
    run();
    if (!pending.Poll(output) || output.result != UpdateCheckService::Result::DownloadFailed || !output.version.empty()) ++failures; ++checks;
    pending.Start(work, [&](std::function<void()> task) { run = std::move(task); return true; });
    pending.Abandon(); run();
    if (pending.Busy() || pending.Poll(output)) ++failures; ++checks;
    { AsyncUpdateCheck destroyed; destroyed.Start(work, [&](std::function<void()> task) { run = std::move(task); return true; }); }
    run(); ++checks; // Work remains safe after destroying the coordinator.

    // A real detached worker waits on a gate while the caller can poll/continue.
    auto gate = std::make_shared<std::promise<void>>();
    auto ready = gate->get_future().share();
    auto entered = std::make_shared<std::promise<std::thread::id>>();
    auto workerId = entered->get_future();
    const auto callerId = std::this_thread::get_id();
    if (!pending.Start([ready, entered, work] {
        entered->set_value(std::this_thread::get_id()); ready.wait(); return work();
    })) ++failures;
    const bool started = workerId.wait_for(std::chrono::seconds(5)) == std::future_status::ready;
    if (!started || workerId.get() == callerId || pending.Poll(output)) ++failures;
    gate->set_value();
    bool completed = false;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (std::chrono::steady_clock::now() < deadline && !(completed = pending.Poll(output)))
        std::this_thread::yield();
    if (!completed || output.result != UpdateCheckService::Result::Available || output.version != "2") ++failures;
    ++checks;
    if (!failures) std::cout << "PASS " << checks << " transport/background cases (fake HTTP; no network/UI)\n";
    else std::cerr << "FAIL transport/background: " << failures << "\n";
    return failures;
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
    int failures = TestBackgroundUpdateChecks() + TestSharedUpdateService();
    using CheckResult = UpdateCheckService::Result;
    const std::wstring validMetadata = L"{\"latestVersion\":{\"versionName\":\"9.9\",\"versionCode\":47},\"latestWinVersion\":{\"versionName\":\"2.0.2\",\"versionCode\":131074}}";
    int checkCases = 0;
    for (uint32_t current : { uint32_t(65538), uint32_t(131074), uint32_t(196610), uint32_t(3) }) {
        std::string version = "stale";
        const auto expected = current == 65538 ? CheckResult::Available : CheckResult::UpToDate;
        if (UpdateCheckService::Check(validMetadata, current, version) != expected || version != "2.0.2") ++failures;
        ++checkCases;
    }
    std::string version = "stale";
    if (UpdateCheckService::Check(L"", 0, version) != CheckResult::DownloadFailed || !version.empty()) ++failures;
    ++checkCases;
    for (int depth : { 16, 17 }) {
        std::wstring nested = L"0";
        for (int i = 0; i < depth; ++i) nested = L"{\"child\":" + nested + L"}";
        const auto input = L"{\"unknown\":" + nested + L",\"latestWinVersion\":{\"versionName\":\"2\",\"versionCode\":2}}";
        version = "stale";
        const auto expected = depth == 16 ? CheckResult::Available : CheckResult::InvalidMetadata;
        if (UpdateCheckService::Check(input, 1, version) != expected
            || version != (depth == 16 ? "2" : "")) ++failures;
        ++checkCases;
    }
    version = "stale";
    auto padded = validMetadata;
    padded.resize(65536, L' ');
    if (UpdateCheckService::Check(padded, 65538, version) != CheckResult::Available || version != "2.0.2") ++failures;
    ++checkCases;
    const std::wstring invalidMetadata[] = {
        L" ", L"{}", L"{\"latestVersion\":{\"versionName\":\"2.0.2\",\"versionCode\":131074}}",
        L"{\"latestWinVersion\":{}}", L"{\"latestWinVersion\":{\"versionName\":\"2.0.2\"}}",
        L"{\"latestWinVersion\":{\"versionCode\":131074}}",
        L"{\"latestWinVersion\":{\"versionName\":\"../escape\",\"versionCode\":1}}",
        L"{\"latestWinVersion\":{\"versionName\":\"\",\"versionCode\":1}}",
        L"{\"latestWinVersion\":{\"versionName\":\"2\",\"versionCode\":-1}}",
        L"{\"latestWinVersion\":{\"versionName\":\"2\",\"versionCode\":1.5}}",
        L"{\"latestWinVersion\":{\"versionName\":\"2\",\"versionCode\":01}}",
        L"{\"latestWinVersion\":{\"versionName\":\"2\",\"versionCode\":4294967296}}",
        L"{\"latestWinVersion\":{\"versionName\":\"2\",\"versionCode\":16777216}}",
        L"{\"latestWinVersion\":{\"versionName\":\"2\",\"versionCode\":\"1\"}}",
        L"{\"latestWinVersion\":{\"versionName\":\"2\",\"versionName\":\"3\",\"versionCode\":1}}",
        L"{\"latestWinVersion\":{\"versionName\":\"2\",\"versionCode\":1,\"versionCode\":2}}",
        validMetadata + L"trailing", validMetadata.substr(0, validMetadata.size() - 1),
        L"{\"latestWinVersion\":{},\"latestWinVersion\":{\"versionName\":\"2\",\"versionCode\":1}}",
        L"{\"latestWinVersion\":{\"versionName\":\"2\",\"versionCode\":1,}}",
        std::wstring(65537, L' '),
        L"{\"latestWinVersion\":{\"versionName\":\"" + std::wstring(65, L'a') + L"\",\"versionCode\":1}}",
        L"{\"other\":" + std::wstring(20, L'{') + L"}"
    };
    for (const auto& input : invalidMetadata) {
        version = "stale";
        if (UpdateCheckService::Check(input, 0, version) != CheckResult::InvalidMetadata || !version.empty()) ++failures;
        ++checkCases;
    }
    version = "stale";
    if (UpdateCheckService::Check(L" { \"latestWinVersion\" : { \"versionCode\" : 131074, \"versionName\" : \"v2.0.2-beta_1\" } } \r\n", 65538, version)
        != CheckResult::Available || version != "v2.0.2-beta_1") ++failures;
    ++checkCases;
    if (!failures) std::cout << "PASS " << checkCases << " update-check metadata cases (no network)\n";
    int launchChecks = 0;
    using LaunchResult = UpdateLaunchService::Result;
    const HWND owner = reinterpret_cast<HWND>(123);
    for (bool allowed : { false, true }) for (INT_PTR shellResult = 0; shellResult <= 33; ++shellResult) {
        int paths = 0, starts = 0;
        const auto result = UpdateLaunchService::Start(owner, [=] { return allowed; },
            [&](LPWSTR buffer, DWORD capacity) {
                ++paths; const std::wstring module = L"D:\\tiếng Việt spaces\\OpenKey64.exe";
                if (capacity <= module.size()) return capacity;
                std::copy(module.begin(), module.end(), buffer); return static_cast<DWORD>(module.size());
            }, [&](HWND parent, LPCWSTR file, LPCWSTR directory) {
                ++starts;
                if (parent != owner || std::wstring(file) != L"D:\\tiếng Việt spaces\\OpenKeyUpdate.exe"
                    || std::wstring(directory) != L"D:\\tiếng Việt spaces\\") ++failures;
                return shellResult;
            });
        const auto expected = !allowed ? LaunchResult::ManualUpdateRequired
            : shellResult > 32 ? LaunchResult::Started : LaunchResult::Failed;
        if (result != expected || paths != int(allowed) || starts != int(allowed)) ++failures;
        ++launchChecks;
    }
    for (const auto& module : { std::wstring(), std::wstring(L"relative.exe"),
        std::wstring(L"D:relative\\OpenKey.exe"), std::wstring(L"D:\\folder\\"),
        std::wstring(L"D:\\a\0b.exe", 10), std::wstring(32768, L'x') }) {
        int starts = 0;
        const auto result = UpdateLaunchService::Start(owner, [] { return true; },
            [&](LPWSTR buffer, DWORD capacity) {
                const auto count = (std::min)(module.size(), size_t(capacity));
                std::copy_n(module.begin(), count, buffer); return static_cast<DWORD>(module.size());
            }, [&](HWND, LPCWSTR, LPCWSTR) { ++starts; return INT_PTR(33); });
        if (result != LaunchResult::Failed || starts) ++failures;
        ++launchChecks;
    }
    for (const auto& module : { std::wstring(L"C:\\OpenKey.exe"),
        std::wstring(L"\\\\server\\share\\OpenKey32.exe"),
        L"D:\\" + std::wstring(500, L'a') + L"\\OpenKey.exe" }) {
        const auto expectedDirectory = module.substr(0, module.find_last_of(L'\\') + 1);
        const auto result = UpdateLaunchService::Start(nullptr, [] { return true; },
            [&](LPWSTR buffer, DWORD) { std::copy(module.begin(), module.end(), buffer); return static_cast<DWORD>(module.size()); },
            [&](HWND parent, LPCWSTR file, LPCWSTR directory) {
                if (parent || std::wstring(file) != expectedDirectory + L"OpenKeyUpdate.exe"
                    || std::wstring(directory) != expectedDirectory) ++failures;
                return INT_PTR(33);
            });
        if (result != LaunchResult::Started) ++failures;
        ++launchChecks;
    }
    if (!failures) std::cout << "PASS " << launchChecks << " injected launch cases (no process started)\n";
    struct MetadataCase { std::wstring input; const wchar_t* expected; };
    const MetadataCase metadataCases[] = {
        {L"{\"latestWinVersion\":{\"versionName\":\"2.0.2\",\"versionCode\":131074}}", L"2.0.2"},
        {L"{\"latestVersion\":{\"versionName\":\"9.9.9\"},\"latestWinVersion\":{\"versionName\":\"2.0.2\",\"versionCode\":131074}}", L"2.0.2"},
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
        {L"{\"latestWinVersion\":{\"versionName\":\"..\",\"versionCode\":1}}", nullptr},
        {L"{\"latestWinVersion\":{\"versionName\":\".\",\"versionCode\":1}}", nullptr},
        {L"{\"latestWinVersion\":{\"versionName\":\"2.0.2\"}}", nullptr},
        {L"{\"latestWinVersion\":{\"versionName\":\"2.0.2\",\"versionCode\":0}}", L"2.0.2"},
        {L"{\"latestWinVersion\":{\"versionName\":\"2.0.2\",\"versionCode\":16777216}}", nullptr},
        {L"{\"nested\":{\"latestWinVersion\":{\"versionName\":\"2.0.2\",\"versionCode\":1}}}", nullptr},
        {L"{\"latestWinVersion\":{\"versionName\":\"2.0.2\",\"versionCode\":1}} trailing", nullptr},
        {L"{\"latestWinVersion\":{\"versionName\":\"2.0.2\",\"versionCode\":1},\"latestWinVersion\":{\"versionName\":\"3.0.0\",\"versionCode\":3}}", nullptr},
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
    if (failures == 0) std::cout << "PASS " << ARRAYSIZE(metadataCases) << " metadata cases\n";
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
