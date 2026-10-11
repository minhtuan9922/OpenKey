#include "pch.h"
#include "AdvancedSettingsPage.xaml.h"
#include "Localization.h"
#include "SettingsDependencyState.h"
#include <chrono>
#include "UiLanguageSettings.h"
#include "UiTheme.h"

#if __has_include("AdvancedSettingsPage.g.cpp")
#include "AdvancedSettingsPage.g.cpp"
#endif

using namespace winrt;
using namespace Microsoft::UI::Xaml;

namespace winrt::OpenKeyUI::implementation
{
    AdvancedSettingsPage::AdvancedSettingsPage()
    {
        InitializeComponent();
        m_preferences = SettingsService::readPreferences();
        m_savedPreferences = m_preferences;
        UiLanguageCombo().SelectedIndex(OpenKeyUILanguage::Read() == OpenKeyUILanguage::Language::English ? 1 : 0);
        m_selectedTheme = static_cast<int>(OpenKeyUITheme::Read());
        UiThemeCombo().SelectedIndex(m_selectedTheme);

        UpperCaseFirstCharToggle().IsOn(m_preferences.upperCaseFirstChar != 0);
        AllowConsonantToggle().IsOn(m_preferences.allowConsonantZFWJ != 0);
        TemporarySpellingToggle().IsOn(m_preferences.tempOffSpelling != 0);
        GrayIconToggle().IsOn(m_preferences.useGrayIcon != 0);
        RunWithWindowsToggle().IsOn(m_preferences.runWithWindows != 0);
        ShowOnStartupToggle().IsOn(m_preferences.showOnStartUp != 0);
        CheckUpdatesToggle().IsOn(m_preferences.checkNewVersion != 0);

        UpdateDependentControls();
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

    AdvancedSettingsPage::~AdvancedSettingsPage()
    {
        if (m_refreshTimer) m_refreshTimer.Stop();
    }

    void AdvancedSettingsPage::OnPreferenceToggled(IInspectable const&, RoutedEventArgs const&)
    {
        if (m_loadingPreferences) return;
        UpdateDependentControls();
        SavePreferences();
    }

    void AdvancedSettingsPage::OnUiLanguageChanged(IInspectable const&, Controls::SelectionChangedEventArgs const&)
    {
        if (m_loadingPreferences || UiLanguageCombo().SelectedIndex() < 0) return;
        const bool saved = OpenKeyUILanguage::Save(UiLanguageCombo().SelectedIndex() == 1
            ? OpenKeyUILanguage::Language::English : OpenKeyUILanguage::Language::Vietnamese);
        SettingsStatus().IsOpen(true);
        SettingsStatus().Severity(saved ? Controls::InfoBarSeverity::Success : Controls::InfoBarSeverity::Error);
        SettingsStatus().Title(OpenKeyUILocalization::GetString(saved ? L"UiLanguageSavedTitle" : L"SaveFailureTitle"));
        SettingsStatus().Message(OpenKeyUILocalization::GetString(saved ? L"UiLanguageRestartMessage" : L"SaveFailureMessage"));
        if (!saved)
        {
            m_loadingPreferences = true;
            UiLanguageCombo().SelectedIndex(OpenKeyUILanguage::Read() == OpenKeyUILanguage::Language::English ? 1 : 0);
            m_loadingPreferences = false;
        }
    }

    void AdvancedSettingsPage::OnUiThemeChanged(
        Windows::Foundation::IInspectable const&,
        Controls::SelectionChangedEventArgs const&)
    {
        const auto index = UiThemeCombo().SelectedIndex();
        if (m_loadingPreferences || index < 0 || index > 2) return;
        const auto root = XamlRoot() ? XamlRoot().Content().try_as<FrameworkElement>() : nullptr;
        const bool saved = root && OpenKeyUITheme::Save(static_cast<OpenKeyUITheme::Theme>(index));
        if (saved)
        {
            root.RequestedTheme(OpenKeyUITheme::Resolve(static_cast<OpenKeyUITheme::Theme>(index),
                Windows::UI::ViewManagement::UISettings{}));
            m_selectedTheme = index;
        }
        else
        {
            m_loadingPreferences = true;
            UiThemeCombo().SelectedIndex(m_selectedTheme);
            m_loadingPreferences = false;
        }
        SettingsStatus().IsOpen(true);
        SettingsStatus().Severity(saved ? Controls::InfoBarSeverity::Success : Controls::InfoBarSeverity::Error);
        SettingsStatus().Title(OpenKeyUILocalization::GetString(saved ? L"UiThemeSavedTitle" : L"SaveFailureTitle"));
        SettingsStatus().Message(OpenKeyUILocalization::GetString(saved ? L"UiThemeAppliedMessage" : L"SaveFailureMessage"));
    }

    void AdvancedSettingsPage::RefreshPreferences()
    {
        if (!UiThemeCombo().IsDropDownOpen())
        {
            const auto theme = static_cast<int>(OpenKeyUITheme::Read());
            if (m_selectedTheme != theme)
            {
                m_loadingPreferences = true;
                UiThemeCombo().SelectedIndex(theme);
                m_selectedTheme = theme;
                m_loadingPreferences = false;
            }
        }
        const auto latest = SettingsService::readPreferences();
        if (m_saveFailed)
        {
            // Keep pending advanced edits, but the external spelling switch
            // must still control availability of the dependent options.
            if (m_preferences.checkSpelling != latest.checkSpelling)
            {
                m_preferences.checkSpelling = latest.checkSpelling;
                m_savedPreferences.checkSpelling = latest.checkSpelling;
                UpdateDependentControls();
            }
            return;
        }
        m_preferences = latest;
        m_savedPreferences = latest;
        m_loadingPreferences = true;
        const auto update = [](Controls::ToggleSwitch const& toggle, const int value)
        {
            if (toggle.IsOn() != (value != 0)) toggle.IsOn(value != 0);
        };
        update(UpperCaseFirstCharToggle(), m_preferences.upperCaseFirstChar);
        update(AllowConsonantToggle(), m_preferences.allowConsonantZFWJ);
        update(TemporarySpellingToggle(), m_preferences.tempOffSpelling);
        update(GrayIconToggle(), m_preferences.useGrayIcon);
        update(RunWithWindowsToggle(), m_preferences.runWithWindows);
        update(ShowOnStartupToggle(), m_preferences.showOnStartUp);
        update(CheckUpdatesToggle(), m_preferences.checkNewVersion);
        // Display language is a separate saved setting, applied on reopening.
        // Do not change its ComboBox or the pending restart message here.
        UpdateDependentControls();
        m_loadingPreferences = false;
    }

    void AdvancedSettingsPage::UpdateDependentControls()
    {
        const auto state = OpenKeyUISettingsDependencies::Evaluate(
            m_preferences.checkSpelling != 0,
            AllowConsonantToggle().IsOn() || TemporarySpellingToggle().IsOn());
        AllowConsonantToggle().IsEnabled(state.available);
        TemporarySpellingToggle().IsEnabled(state.available);
        SpellingRequirement().Visibility(state.available
            ? Visibility::Collapsed
            : Visibility::Visible);
        if (SpellingDependencyWarning().IsOpen() != state.warnRetainedSelection)
            SpellingDependencyWarning().IsOpen(state.warnRetainedSelection);
    }

    void AdvancedSettingsPage::SavePreferences()
    {
        if (m_loadingPreferences)
        {
            return;
        }

        m_preferences.upperCaseFirstChar = UpperCaseFirstCharToggle().IsOn() ? 1 : 0;
        m_preferences.allowConsonantZFWJ = AllowConsonantToggle().IsOn() ? 1 : 0;
        m_preferences.tempOffSpelling = TemporarySpellingToggle().IsOn() ? 1 : 0;
        m_preferences.useGrayIcon = GrayIconToggle().IsOn() ? 1 : 0;
        m_preferences.runWithWindows = RunWithWindowsToggle().IsOn() ? 1 : 0;
        m_preferences.showOnStartUp = ShowOnStartupToggle().IsOn() ? 1 : 0;
        m_preferences.checkNewVersion = CheckUpdatesToggle().IsOn() ? 1 : 0;

        const bool saved = SettingsService::saveChangedPreferences(m_savedPreferences, m_preferences);
        m_saveFailed = !saved;
        SettingsStatus().IsOpen(true);
        SettingsStatus().Severity(saved
            ? Controls::InfoBarSeverity::Success
            : Controls::InfoBarSeverity::Error);
        SettingsStatus().Title(saved
            ? OpenKeyUILocalization::GetString(L"SaveSuccessTitle")
            : OpenKeyUILocalization::GetString(L"SaveFailureTitle"));
        SettingsStatus().Message(saved
            ? OpenKeyUILocalization::GetString(L"AdvancedSaveSuccessMessage")
            : OpenKeyUILocalization::GetString(L"SaveFailureMessage"));
    }
}
