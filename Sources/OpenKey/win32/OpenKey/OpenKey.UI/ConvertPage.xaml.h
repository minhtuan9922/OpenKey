#pragma once

#include "ConvertPage.g.h"
#include "../OpenKey/ConvertService.h"
#include "../OpenKey/SettingsService.h"

#include <vector>

namespace winrt::OpenKeyUI::implementation
{
    struct ConvertPage : ConvertPageT<ConvertPage>
    {
        ConvertPage();
        ~ConvertPage();

        void OnCodeTableChanged(
            winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Microsoft::UI::Xaml::Controls::SelectionChangedEventArgs const& args);
        void OnSwapClicked(
            winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);
        void OnOptionChanged(
            winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);
        void OnInputTextChanged(
            winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Microsoft::UI::Xaml::Controls::TextChangedEventArgs const& args);
        void OnConvertClicked(
            winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);
        void OnCopyClicked(
            winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);
        void OnClearClicked(
            winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);
        void OnQuickConvertClicked(
            winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);
        void OnQuickHotkeyChanged(
            winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);
        void OnQuickKeyChanged(
            winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Microsoft::UI::Xaml::Controls::SelectionChangedEventArgs const& args);
        void OnNotifyToggled(
            winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);

    private:
        void RefreshPreferences();
        void ApplyPreferences();
        void SavePreferences();
        void SaveQuickHotkey();
        void UpdateActionState();
        void UpdateHotkeyPreview();
        ConvertService::Options CurrentOptions();
        void ShowStatus(bool success, winrt::hstring const& title, winrt::hstring const& message);
        void ArmCaretTone(bool armed);
        void EditCaretTone(winrt::Microsoft::UI::Xaml::Input::KeyRoutedEventArgs const& args);

        bool m_loadingPreferences = true;
        bool m_saveFailed = false;
        bool m_caretToneArmed = false;
        bool m_composingText = false;
        HWND m_caretToneWindow = nullptr;
        SettingsService::Preferences m_preferences;
        SettingsService::Preferences m_savedPreferences;
        std::vector<int> m_quickKeyCodes;
        winrt::Microsoft::UI::Dispatching::DispatcherQueueTimer m_refreshTimer{ nullptr };
    };
}

namespace winrt::OpenKeyUI::factory_implementation
{
    struct ConvertPage : ConvertPageT<ConvertPage, implementation::ConvertPage>
    {
    };
}
