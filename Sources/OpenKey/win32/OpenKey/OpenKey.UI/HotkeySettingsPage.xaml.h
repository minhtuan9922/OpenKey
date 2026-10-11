#pragma once

#include "HotkeySettingsPage.g.h"
#include "../OpenKey/SettingsService.h"

#include <vector>

namespace winrt::OpenKeyUI::implementation
{
    struct HotkeySettingsPage : HotkeySettingsPageT<HotkeySettingsPage>
    {
        HotkeySettingsPage();
        ~HotkeySettingsPage();

        void OnHotkeyChanged(
            winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);
        void OnKeyChanged(
            winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Microsoft::UI::Xaml::Controls::SelectionChangedEventArgs const& args);

    private:
        void RefreshPreferences();
        void ApplyHotkey(int status);
        void PopulateKeys();
        void SaveHotkey();
        void UpdatePreview();

        bool m_loadingPreferences = true;
        bool m_saveFailed = false;
        SettingsService::Preferences m_preferences;
        SettingsService::Preferences m_savedPreferences;
        std::vector<int> m_keyCodes;
        winrt::Microsoft::UI::Dispatching::DispatcherQueueTimer m_refreshTimer{ nullptr };
    };
}

namespace winrt::OpenKeyUI::factory_implementation
{
    struct HotkeySettingsPage : HotkeySettingsPageT<HotkeySettingsPage, implementation::HotkeySettingsPage>
    {
    };
}
