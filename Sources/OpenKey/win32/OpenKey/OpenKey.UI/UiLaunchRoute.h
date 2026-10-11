#pragma once
#include <string_view>

// Only fixed application pages are accepted; never interpret a path or URI.
namespace OpenKeyUILaunch
{
    inline std::wstring_view Page(int count, wchar_t const* const* arguments)
    {
        if (!arguments) return {};
        const int offset = count == 3 ? 1 : 0; // Full command line or arguments only.
        if (count - offset != 2 || !arguments[offset] || !arguments[offset + 1] ||
            std::wstring_view(arguments[offset]) != L"--page") return {};
        const std::wstring_view page(arguments[offset + 1]);
        return page == L"overview" || page == L"macro" || page == L"convert" ||
            page == L"about" ? page : std::wstring_view{};
    }
}
