#include "pch.h"
#include "InputSettingsPage.xaml.h"
#include "Localization.h"

#include <algorithm>
#include <chrono>

#if __has_include("InputSettingsPage.g.cpp")
#include "InputSettingsPage.g.cpp"
#endif

using namespace winrt;
using namespace Microsoft::UI::Xaml;

namespace winrt::OpenKeyUI::implementation
{
    InputSettingsPage::InputSettingsPage()
    {
        InitializeComponent();
        m_preferences = SettingsService::readPreferences();
        m_savedPreferences = m_preferences;
        InputTypeCombo().SelectedIndex(std::clamp(m_preferences.inputType, 0, 3));
        CodeTableCombo().SelectedIndex(std::clamp(m_preferences.codeTable, 0, 4));
        RememberCodeToggle().IsOn(m_preferences.rememberCode != 0);
        m_loadingPreferences = false;
        m_refreshTimer = DispatcherQueue().CreateTimer();
        m_refreshTimer.Interval(std::chrono::milliseconds(500));
        m_refreshTimer.Tick([weak = get_weak()](auto const&, auto const&)
        {
            if (auto page = weak.get()) page->RefreshPreferences();
        });
        Loaded([weak = get_weak()](auto const&, auto const&)
        {
            if (auto page = weak.get())
            {
                page->RefreshPreferences();
                page->m_refreshTimer.Start();
            }
        });
        Unloaded([weak = get_weak()](auto const&, auto const&)
        {
            if (auto page = weak.get()) page->m_refreshTimer.Stop();
        });
    }

    InputSettingsPage::~InputSettingsPage()
    {
        if (m_refreshTimer) m_refreshTimer.Stop();
    }

    void InputSettingsPage::RefreshPreferences()
    {
        // Do not replace a failed edit waiting for retry, or move the selection
        // while the user is interacting with an open drop-down.
        if (m_saveFailed || InputTypeCombo().IsDropDownOpen() || CodeTableCombo().IsDropDownOpen()) return;

        m_preferences = SettingsService::readPreferences();
        m_savedPreferences = m_preferences;
        m_loadingPreferences = true;
        const int inputType = std::clamp(m_preferences.inputType, 0, 3);
        const int codeTable = std::clamp(m_preferences.codeTable, 0, 4);
        const bool rememberCode = m_preferences.rememberCode != 0;
        if (InputTypeCombo().SelectedIndex() != inputType) InputTypeCombo().SelectedIndex(inputType);
        if (CodeTableCombo().SelectedIndex() != codeTable) CodeTableCombo().SelectedIndex(codeTable);
        if (RememberCodeToggle().IsOn() != rememberCode) RememberCodeToggle().IsOn(rememberCode);
        m_loadingPreferences = false;
    }

    void InputSettingsPage::OnInputTypeChanged(IInspectable const&, Controls::SelectionChangedEventArgs const&)
    {
        if (m_loadingPreferences || InputTypeCombo().SelectedIndex() < 0) return;
        m_preferences.inputType = InputTypeCombo().SelectedIndex();
        SavePreferences();
    }

    void InputSettingsPage::OnCodeTableChanged(IInspectable const&, Controls::SelectionChangedEventArgs const&)
    {
        if (m_loadingPreferences || CodeTableCombo().SelectedIndex() < 0) return;
        m_preferences.codeTable = CodeTableCombo().SelectedIndex();
        SavePreferences();
    }

    void InputSettingsPage::OnRememberCodeToggled(IInspectable const&, RoutedEventArgs const&)
    {
        if (m_loadingPreferences) return;
        m_preferences.rememberCode = RememberCodeToggle().IsOn() ? 1 : 0;
        SavePreferences();
    }

    void InputSettingsPage::SavePreferences()
    {
        if (m_loadingPreferences)
        {
            return;
        }

        const bool saved = SettingsService::saveChangedPreferences(m_savedPreferences, m_preferences);
        m_saveFailed = !saved;
        SettingsStatus().IsOpen(true);
        SettingsStatus().Severity(saved
            ? Controls::InfoBarSeverity::Success
            : Controls::InfoBarSeverity::Error);
        SettingsStatus().Title(OpenKeyUILocalization::GetString(saved ? L"SaveSuccessTitle" : L"SaveFailureTitle"));
        SettingsStatus().Message(saved
            ? OpenKeyUILocalization::GetString(L"SpellingSaveSuccessMessage")
            : OpenKeyUILocalization::GetString(L"SaveFailureMessage"));
    }
}
