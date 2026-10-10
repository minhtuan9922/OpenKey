#pragma once
#include "UiThemeSettings.h"
#include <winrt/Microsoft.UI.Xaml.h>
#include <winrt/Windows.UI.ViewManagement.h>

namespace OpenKeyUITheme
{
    inline winrt::Microsoft::UI::Xaml::ElementTheme Resolve(Theme theme,
        winrt::Windows::UI::ViewManagement::UISettings const& settings)
    {
        using winrt::Microsoft::UI::Xaml::ElementTheme;
        if (theme == Theme::Light) return ElementTheme::Light;
        if (theme == Theme::Dark) return ElementTheme::Dark;
        // A light system foreground means dark app appearance (Microsoft's
        // documented UISettings approach). Do not rely on Default being Dark.
        const auto foreground = settings.GetColorValue(winrt::Windows::UI::ViewManagement::UIColorType::Foreground);
        return IsColorLight(foreground.R, foreground.G, foreground.B) ? ElementTheme::Dark : ElementTheme::Light;
    }
}
