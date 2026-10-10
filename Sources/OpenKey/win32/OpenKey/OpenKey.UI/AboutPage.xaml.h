#pragma once

#include "AboutPage.g.h"
#include "UpdateRequestState.h"

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
    private:
        void FinishUpdate(uint64_t ticket) noexcept;
        UpdateRequestState updateRequest;
    };
}

namespace winrt::OpenKeyUI::factory_implementation
{
    struct AboutPage : AboutPageT<AboutPage, implementation::AboutPage>
    {
    };
}
