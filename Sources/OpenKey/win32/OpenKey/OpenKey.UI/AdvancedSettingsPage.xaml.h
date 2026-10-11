#pragma once

#include "AdvancedSettingsPage.g.h"
#include "../OpenKey/SettingsService.h"

namespace winrt::OpenKeyUI::implementation
{
    struct AdvancedSettingsPage : AdvancedSettingsPageT<AdvancedSettingsPage>
    {
        AdvancedSettingsPage();
        ~AdvancedSettingsPage();

        void OnUiLanguageChanged(
            winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Microsoft::UI::Xaml::Controls::SelectionChangedEventArgs const& args);

        void OnUiThemeChanged(
            winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Microsoft::UI::Xaml::Controls::SelectionChangedEventArgs const& args);

        void OnPreferenceToggled(
            winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);

    private:
        void RefreshPreferences();
        void SavePreferences();
        void UpdateDependentControls();

        bool m_loadingPreferences = true;
        bool m_saveFailed = false;
        int m_selectedTheme = 0;
        SettingsService::Preferences m_preferences;
        SettingsService::Preferences m_savedPreferences;
        winrt::Microsoft::UI::Dispatching::DispatcherQueueTimer m_refreshTimer{ nullptr };
    };
}

namespace winrt::OpenKeyUI::factory_implementation
{
    struct AdvancedSettingsPage : AdvancedSettingsPageT<AdvancedSettingsPage, implementation::AdvancedSettingsPage>
    {
    };
}
