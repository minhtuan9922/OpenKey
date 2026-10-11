#include "../Sources/OpenKey/win32/OpenKey/OpenKey.UI/UiLaunchRoute.h"
#include <iostream>
#include <stdexcept>

int main()
{
    int passed = 0;
    const auto check = [&](bool value) { if (!value) throw std::runtime_error("Launch route mismatch"); ++passed; };
    try
    {
        for (const auto page : { L"overview", L"macro", L"convert", L"about" })
        {
            const wchar_t* full[]{ L"C:\\Thư mục thử\\OpenKey.UI.exe", L"--page", page };
            check(OpenKeyUILaunch::Page(3, full) == page);
            check(OpenKeyUILaunch::Page(2, full + 1) == page);
        }
        const wchar_t* unknown[]{ L"--page", L"file:///C:/test" };
        check(OpenKeyUILaunch::Page(2, unknown).empty());
        const wchar_t* wrongCase[]{ L"--page", L"Macro" };
        check(OpenKeyUILaunch::Page(2, wrongCase).empty());
        const wchar_t* missing[]{ L"--page" };
        check(OpenKeyUILaunch::Page(1, missing).empty());
        const wchar_t* duplicate[]{ L"--page", L"macro", L"--page", L"about" };
        check(OpenKeyUILaunch::Page(4, duplicate).empty());
        const wchar_t* invalid[]{ L"--other", L"macro" };
        check(OpenKeyUILaunch::Page(2, invalid).empty());
        const wchar_t* noValue[]{ L"--page", nullptr };
        check(OpenKeyUILaunch::Page(2, noValue).empty());
        check(OpenKeyUILaunch::Page(0, nullptr).empty());
        check(OpenKeyUILaunch::Page(-1, nullptr).empty());
        std::cout << "Fluent launch routes: " << passed << "/" << passed << " passed (pure allowlist; no process launch).\n";
        return 0;
    }
    catch (std::exception const& error) { std::cerr << error.what() << '\n'; return 1; }
}
