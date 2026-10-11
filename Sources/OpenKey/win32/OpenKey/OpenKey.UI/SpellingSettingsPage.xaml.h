#pragma once

#include "SpellingSettingsPage.g.h"
#include "../OpenKey/SettingsService.h"

namespace winrt::OpenKeyUI::implementation
{
    struct SpellingSettingsPage : SpellingSettingsPageT<SpellingSettingsPage>
    {
        SpellingSettingsPage();
        ~SpellingSettingsPage();

        void OnPreferenceToggled(
            winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);

    private:
        void RefreshPreferences();
        void SavePreferences();

        bool m_loadingPreferences = true;
        bool m_saveFailed = false;
        SettingsService::Preferences m_preferences;
        SettingsService::Preferences m_savedPreferences;
        winrt::Microsoft::UI::Dispatching::DispatcherQueueTimer m_refreshTimer{ nullptr };
    };
}

namespace winrt::OpenKeyUI::factory_implementation
{
    struct SpellingSettingsPage : SpellingSettingsPageT<SpellingSettingsPage, implementation::SpellingSettingsPage>
    {
    };
}
