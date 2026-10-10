#pragma once

#include "OverviewPage.g.h"
#include "../OpenKey/SettingsService.h"

namespace winrt::OpenKeyUI::implementation
{
    struct OverviewPage : OverviewPageT<OverviewPage>
    {
        OverviewPage();
        ~OverviewPage();

        void OnLanguageToggled(
            winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);

    private:
        void RefreshLanguage();
        void UpdateLanguageDescription();
        void ShowSaveResult(bool saved);

        bool m_loadingPreferences = true;
        bool m_saveFailed = false;
        SettingsService::Preferences m_preferences;
        winrt::Microsoft::UI::Dispatching::DispatcherQueueTimer m_refreshTimer{ nullptr };
    };
}

namespace winrt::OpenKeyUI::factory_implementation
{
    struct OverviewPage : OverviewPageT<OverviewPage, implementation::OverviewPage>
    {
    };
}
