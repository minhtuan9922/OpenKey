#pragma once

namespace OpenKeyUILocalization
{
    inline winrt::hstring GetString(wchar_t const* resourceId)
    {
        static const auto loader =
            winrt::Microsoft::Windows::ApplicationModel::Resources::ResourceLoader();
        return loader.GetString(resourceId);
    }

    inline winrt::hstring Format(wchar_t const* resourceId, winrt::hstring const& argument)
    {
        std::wstring value(GetString(resourceId));
        const auto position = value.find(L"{0}");
        if (position != std::wstring::npos)
        {
            value.replace(position, 3, argument.c_str(), argument.size());
        }
        return winrt::hstring(value);
    }
}
