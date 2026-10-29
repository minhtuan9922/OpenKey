#include "pch.h"
#include "OverviewPage.xaml.h"
#include "Localization.h"
#include <algorithm>
#include <chrono>

#if __has_include("OverviewPage.g.cpp")
#include "OverviewPage.g.cpp"
#endif

using namespace winrt;
using namespace Microsoft::UI::Xaml;

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
