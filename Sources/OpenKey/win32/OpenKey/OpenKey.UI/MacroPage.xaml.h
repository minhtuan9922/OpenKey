#pragma once

#include "MacroPage.g.h"
#include "../OpenKey/MacroService.h"
#include "../OpenKey/SettingsService.h"
#include "UiOperationGate.h"

#include <vector>

namespace winrt::OpenKeyUI::implementation
{
    struct MacroPage : MacroPageT<MacroPage>
    {
        MacroPage();
        ~MacroPage();

        winrt::fire_and_forget OnReloadClicked(
            winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);

        void OnSearchTextChanged(
            winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Microsoft::UI::Xaml::Controls::TextChangedEventArgs const& args);
        winrt::fire_and_forget OnMacroSelectionChanged(
            winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Microsoft::UI::Xaml::Controls::SelectionChangedEventArgs const& args);
        winrt::fire_and_forget OnNewClicked(
            winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);
        void OnSaveClicked(
            winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);
        void OnDeleteClicked(
            winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);
        winrt::fire_and_forget OnImportClicked(
            winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);
        void OnExportClicked(
            winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);
        void OnAutoCapsToggled(
            winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);
        void OnFocusSearchInvoked(
            winrt::Microsoft::UI::Xaml::Input::KeyboardAccelerator const& sender,
            winrt::Microsoft::UI::Xaml::Input::KeyboardAcceleratorInvokedEventArgs const& args);
        void OnDeleteAcceleratorInvoked(
            winrt::Microsoft::UI::Xaml::Input::KeyboardAccelerator const& sender,
            winrt::Microsoft::UI::Xaml::Input::KeyboardAcceleratorInvokedEventArgs const& args);

    private:
        struct BusyScope
        {
            explicit BusyScope(MacroPage& page);
            ~BusyScope();
            void Finish() noexcept;
            explicit operator bool() const noexcept { return static_cast<bool>(m_lease); }
            MacroPage& m_page;
            ::OpenKeyUI::OperationGate::Lease m_lease;
        };
        void UpdateActionAvailability();
        void RestoreEditorSelection();
        winrt::Windows::Foundation::IAsyncOperation<bool> ConfirmDiscardAsync();
        void RefreshPreferences();
        bool ReloadAfterWrite();
        winrt::fire_and_forget DeleteSelectedAsync();
        void RefreshList();
        void ClearEditor();
        void ShowStatus(bool success, winrt::hstring const& title, winrt::hstring const& message);

        bool m_refreshingList = false;
        bool m_loadingPreferences = true;
        bool m_saveFailed = false;
        ::OpenKeyUI::OperationGate m_operations;
        SettingsService::Preferences m_preferences;
        SettingsService::Preferences m_savedPreferences;
        std::wstring m_originalShortcut;
        std::vector<MacroService::Entry> m_entries;
        std::vector<MacroService::Entry> m_visibleEntries;
        winrt::Microsoft::UI::Dispatching::DispatcherQueueTimer m_refreshTimer{ nullptr };
    };
}

namespace winrt::OpenKeyUI::factory_implementation
{
    struct MacroPage : MacroPageT<MacroPage, implementation::MacroPage>
    {
    };
}
