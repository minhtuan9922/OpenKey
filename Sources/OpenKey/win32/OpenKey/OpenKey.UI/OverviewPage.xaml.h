#pragma once

#include "OverviewPage.g.h"
#include "../OpenKey/SettingsService.h"

namespace winrt::OpenKeyUI::implementation
{
    struct OverviewPage : OverviewPageT<OverviewPage>
    {
        OverviewPage();
        ~OverviewPage();
        winrt::fire_and_forget OnEnableCaretEditing(winrt::Windows::Foundation::IInspectable const&, winrt::Microsoft::UI::Xaml::RoutedEventArgs const&);
        void OnDisableCaretEditing(winrt::Windows::Foundation::IInspectable const&, winrt::Microsoft::UI::Xaml::RoutedEventArgs const&);

        void OnLanguageToggled(
            winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);

        void OnInputTypeChanged(
            winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Microsoft::UI::Xaml::Controls::SelectionChangedEventArgs const& args);
        void OnCodeTableChanged(
            winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Microsoft::UI::Xaml::Controls::SelectionChangedEventArgs const& args);
        void OnRememberCodeToggled(
            winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);

    private:
        void RefreshPreferences();
        void SavePreferences();
        void UpdateLanguageDescription();
        void ShowSaveResult(bool saved);
        void RefreshCaretEditing();
        bool m_caretBusy = false;
        unsigned m_caretTicks = 0;
        bool m_caretFeedback = false;
        int m_caretState = -1;

        bool m_loadingPreferences = true;
        bool m_saveFailed = false;
        SettingsService::Preferences m_preferences;
        SettingsService::Preferences m_savedPreferences;
        winrt::Microsoft::UI::Dispatching::DispatcherQueueTimer m_refreshTimer{ nullptr };
    };
}

namespace winrt::OpenKeyUI::factory_implementation
{
    struct OverviewPage : OverviewPageT<OverviewPage, implementation::OverviewPage>
    {
    };
}
