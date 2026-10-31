#include "pch.h"
#include "OverviewPage.xaml.h"
#include "Localization.h"
#include "CaretEditingUI.h"
#include <algorithm>
#include <chrono>

#if __has_include("OverviewPage.g.cpp")
#include "OverviewPage.g.cpp"
#endif

using namespace winrt;
using namespace winrt::Microsoft::UI::Xaml;

namespace winrt::OpenKeyUI::implementation
{
    OverviewPage::OverviewPage()
    {
        InitializeComponent();
        m_preferences = SettingsService::readPreferences();
        m_savedPreferences = m_preferences;
        LanguageToggle().IsOn(m_preferences.language != 0);
        InputTypeCombo().SelectedIndex(std::clamp(m_preferences.inputType, 0, 3));
        CodeTableCombo().SelectedIndex(std::clamp(m_preferences.codeTable, 0, 4));
        RememberCodeToggle().IsOn(m_preferences.rememberCode != 0);
        UpdateLanguageDescription();
        m_loadingPreferences = false;
        RefreshCaretEditing();
        // Only observe while this page is loaded. Weak handlers avoid retaining
        // a page after navigation, and refresh must never save back to the engine.
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

    OverviewPage::~OverviewPage()
    {
        if (m_refreshTimer) m_refreshTimer.Stop();
    }

    void OverviewPage::RefreshPreferences()
    {
        if (++m_caretTicks % 4 == 0 && !m_caretBusy) RefreshCaretEditing();
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
        if (LanguageToggle().IsOn() != (m_preferences.language != 0))
            LanguageToggle().IsOn(m_preferences.language != 0);
        UpdateLanguageDescription();
        m_loadingPreferences = false;
    }

    void OverviewPage::RefreshCaretEditing()
    {
        const auto state = OpenKeyUICaret::Read();
        const int current = (state.installed ? 1 : 0) | (state.enabled ? 2 : 0) | (state.active ? 4 : 0);
        if (m_caretState != current) m_caretFeedback = false;
        m_caretState = current;
        CaretEnableButton().IsEnabled(!m_caretBusy && !state.active);
        CaretDisableButton().IsEnabled(!m_caretBusy && state.enabled);
        if (!m_caretBusy && !m_caretFeedback) CaretStatus().Message(OpenKeyUILocalization::GetString(state.active ? L"CaretActive" : state.installed ? L"CaretReady" : L"CaretNotInstalled"));
    }

    winrt::fire_and_forget OverviewPage::OnEnableCaretEditing(IInspectable const&, RoutedEventArgs const&)
    {
        if (m_caretBusy) co_return;
        auto lifetime = get_strong(); const auto dispatcher = DispatcherQueue();
        m_caretBusy = true; m_caretFeedback = false; RefreshCaretEditing();
        CaretStatus().Severity(Controls::InfoBarSeverity::Informational);
        CaretStatus().Message(OpenKeyUILocalization::GetString(L"CaretInstalling"));
        const auto helper = OpenKeyUICaret::Helper(); DWORD error = ERROR_SUCCESS;
        const bool installed = OpenKeyUICaret::Read().installed;
        try {
            co_await winrt::resume_background();
            if (!helper.empty()) {
                if (OpenKeyUICaret::RunHelper(helper, false) != ERROR_SUCCESS) error = OpenKeyUICaret::RunHelper(helper, true);
            } else if (!installed) error = ERROR_FILE_NOT_FOUND;
            co_await wil::resume_foreground(dispatcher);
            HRESULT activated = E_FAIL;
            if (!error) activated = OpenKeyUICaret::Activate(true);
            m_caretBusy = false; RefreshCaretEditing();
            m_caretFeedback = true;
            const bool success = !error && activated == S_OK;
            CaretStatus().Severity(success ? Controls::InfoBarSeverity::Success : error == ERROR_CANCELLED ? Controls::InfoBarSeverity::Informational : Controls::InfoBarSeverity::Warning);
            CaretStatus().Message(OpenKeyUILocalization::GetString(success ? L"CaretActivated" : error == ERROR_CANCELLED ? L"CaretCanceled" : error == ERROR_FILE_NOT_FOUND ? L"CaretMissingPayload" : L"CaretActivationFailed"));
        } catch (...) {
            m_caretBusy = false;
            if (dispatcher.HasThreadAccess()) {
                RefreshCaretEditing(); CaretStatus().Severity(Controls::InfoBarSeverity::Warning);
                m_caretFeedback = true;
                CaretStatus().Message(OpenKeyUILocalization::GetString(L"CaretActivationFailed"));
            }
        }
    }

    void OverviewPage::OnDisableCaretEditing(IInspectable const&, RoutedEventArgs const&)
    {
        if (m_caretBusy) return;
        const HRESULT result = OpenKeyUICaret::Activate(false);
        RefreshCaretEditing();
        m_caretFeedback = true;
        CaretStatus().Severity(result == S_OK ? Controls::InfoBarSeverity::Success : Controls::InfoBarSeverity::Warning);
        CaretStatus().Message(OpenKeyUILocalization::GetString(result == S_OK ? L"CaretDisabled" : L"CaretActivationFailed"));
    }

    void OverviewPage::OnInputTypeChanged(IInspectable const&, Controls::SelectionChangedEventArgs const&)
    {
        if (m_loadingPreferences || InputTypeCombo().SelectedIndex() < 0) return;
        m_preferences.inputType = InputTypeCombo().SelectedIndex();
        SavePreferences();
    }

    void OverviewPage::OnCodeTableChanged(IInspectable const&, Controls::SelectionChangedEventArgs const&)
    {
        if (m_loadingPreferences || CodeTableCombo().SelectedIndex() < 0) return;
        m_preferences.codeTable = CodeTableCombo().SelectedIndex();
        SavePreferences();
    }

    void OverviewPage::OnRememberCodeToggled(IInspectable const&, RoutedEventArgs const&)
    {
        if (m_loadingPreferences) return;
        m_preferences.rememberCode = RememberCodeToggle().IsOn() ? 1 : 0;
        SavePreferences();
    }

    void OverviewPage::OnLanguageToggled(IInspectable const&, RoutedEventArgs const&)
    {
        if (m_loadingPreferences) return;
        m_preferences.language = LanguageToggle().IsOn() ? 1 : 0;
        UpdateLanguageDescription();
        SavePreferences();
    }

    void OverviewPage::SavePreferences()
    {
        const bool saved = SettingsService::saveChangedPreferences(m_savedPreferences, m_preferences);
        m_saveFailed = !saved;
        ShowSaveResult(saved);
    }

    void OverviewPage::UpdateLanguageDescription()
    {
        LanguageDescription().Text(LanguageToggle().IsOn()
            ? OpenKeyUILocalization::GetString(L"VietnameseInputDescription")
            : OpenKeyUILocalization::GetString(L"EnglishInputDescription"));
    }

    void OverviewPage::ShowSaveResult(const bool saved)
    {
        SettingsStatus().Severity(saved
            ? Controls::InfoBarSeverity::Success
            : Controls::InfoBarSeverity::Error);
        SettingsStatus().Title(OpenKeyUILocalization::GetString(saved ? L"SaveSuccessTitle" : L"SaveFailureTitle"));
        SettingsStatus().Message(saved
            ? OpenKeyUILocalization::GetString(L"OverviewSaveSuccessMessage")
            : OpenKeyUILocalization::GetString(L"SaveFailureMessage"));
    }
}
