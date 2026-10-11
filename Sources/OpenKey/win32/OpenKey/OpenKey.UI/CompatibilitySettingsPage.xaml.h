#pragma once

#include "CompatibilitySettingsPage.g.h"
#include "../OpenKey/SettingsService.h"

namespace winrt::OpenKeyUI::implementation
{
    struct CompatibilitySettingsPage : CompatibilitySettingsPageT<CompatibilitySettingsPage>
    {
        CompatibilitySettingsPage();
        ~CompatibilitySettingsPage();

        void OnPreferenceToggled(
            winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);

    private:
        void RefreshPreferences();
        void SavePreferences();
        void UpdateDependentControls();

        bool m_loadingPreferences = true;
        bool m_saveFailed = false;
        SettingsService::Preferences m_preferences;
        SettingsService::Preferences m_savedPreferences;
        winrt::Microsoft::UI::Dispatching::DispatcherQueueTimer m_refreshTimer{ nullptr };
    };
}

namespace winrt::OpenKeyUI::factory_implementation
{
    struct CompatibilitySettingsPage : CompatibilitySettingsPageT<CompatibilitySettingsPage, implementation::CompatibilitySettingsPage>
    {
    };
}
