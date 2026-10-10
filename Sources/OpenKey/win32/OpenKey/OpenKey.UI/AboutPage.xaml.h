#pragma once

#include "AboutPage.g.h"

namespace winrt::OpenKeyUI::implementation
{
    struct AboutPage : AboutPageT<AboutPage>
    {
        AboutPage();

        winrt::fire_and_forget OnLinkClicked(
            winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);
        winrt::fire_and_forget OnCheckUpdatesClicked(
            winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);
    };
}

namespace winrt::OpenKeyUI::factory_implementation
{
    struct AboutPage : AboutPageT<AboutPage, implementation::AboutPage>
    {
    };
}
