#include "pch.h"
#include "CompatibilitySettingsPage.xaml.h"
#include "Localization.h"
#include "SettingsDependencyState.h"
#include <chrono>

#if __has_include("CompatibilitySettingsPage.g.cpp")
#include "CompatibilitySettingsPage.g.cpp"
#endif

using namespace winrt;
using namespace Microsoft::UI::Xaml;

namespace winrt::OpenKeyUI::implementation
{
    CompatibilitySettingsPage::CompatibilitySettingsPage()
    {
        InitializeComponent();
        m_preferences = SettingsService::readPreferences();
        m_savedPreferences = m_preferences;

        SmartSwitchToggle().IsOn(m_preferences.useSmartSwitchKey != 0);
        OtherLanguagesToggle().IsOn(m_preferences.otherLanguage != 0);
        TemporaryDisableToggle().IsOn(m_preferences.tempOffOpenKey != 0);
        ClipboardToggle().IsOn(m_preferences.sendKeyStepByStep == 0);
        RecommendationFixToggle().IsOn(m_preferences.fixRecommendBrowser != 0);
        ChromiumFixToggle().IsOn(m_preferences.fixChromiumBrowser != 0);
        MetroSupportToggle().IsOn(m_preferences.supportMetroApp != 0);

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

    CompatibilitySettingsPage::~CompatibilitySettingsPage()
    {
        if (m_refreshTimer) m_refreshTimer.Stop();
    }

    void CompatibilitySettingsPage::RefreshPreferences()
    {
        if (m_saveFailed) return;
        m_preferences = SettingsService::readPreferences();
        m_savedPreferences = m_preferences;
        m_loadingPreferences = true;
        const auto update = [](Controls::ToggleSwitch const& toggle, const bool enabled)
        {
            if (toggle.IsOn() != enabled) toggle.IsOn(enabled);
        };
        update(SmartSwitchToggle(), m_preferences.useSmartSwitchKey != 0);
        update(OtherLanguagesToggle(), m_preferences.otherLanguage != 0);
        update(TemporaryDisableToggle(), m_preferences.tempOffOpenKey != 0);
        // The legacy setting is inverted: zero means Clipboard mode enabled.
        update(ClipboardToggle(), m_preferences.sendKeyStepByStep == 0);
        update(RecommendationFixToggle(), m_preferences.fixRecommendBrowser != 0);
        update(ChromiumFixToggle(), m_preferences.fixChromiumBrowser != 0);
        update(MetroSupportToggle(), m_preferences.supportMetroApp != 0);
        UpdateDependentControls();
        m_loadingPreferences = false;
    }

    void CompatibilitySettingsPage::OnPreferenceToggled(IInspectable const&, RoutedEventArgs const&)
    {
        if (m_loadingPreferences) return;
        UpdateDependentControls();
        SavePreferences();
    }

    void CompatibilitySettingsPage::UpdateDependentControls()
    {
        const auto state = OpenKeyUISettingsDependencies::Evaluate(
            RecommendationFixToggle().IsOn(), ChromiumFixToggle().IsOn());
        ChromiumFixToggle().IsEnabled(state.available);
        ChromiumRequirement().Visibility(state.available
            ? Visibility::Collapsed
            : Visibility::Visible);
        // Avoid repeatedly assigning the live region during the 500 ms refresh.
        if (ChromiumDependencyWarning().IsOpen() != state.warnRetainedSelection)
            ChromiumDependencyWarning().IsOpen(state.warnRetainedSelection);
    }

    void CompatibilitySettingsPage::SavePreferences()
    {
        if (m_loadingPreferences)
        {
            return;
        }

        m_preferences.useSmartSwitchKey = SmartSwitchToggle().IsOn() ? 1 : 0;
        m_preferences.otherLanguage = OtherLanguagesToggle().IsOn() ? 1 : 0;
        m_preferences.tempOffOpenKey = TemporaryDisableToggle().IsOn() ? 1 : 0;
        m_preferences.sendKeyStepByStep = ClipboardToggle().IsOn() ? 0 : 1;
        m_preferences.fixRecommendBrowser = RecommendationFixToggle().IsOn() ? 1 : 0;
        m_preferences.fixChromiumBrowser = ChromiumFixToggle().IsOn() ? 1 : 0;
        m_preferences.supportMetroApp = MetroSupportToggle().IsOn() ? 1 : 0;

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
            ? OpenKeyUILocalization::GetString(L"CompatibilitySaveSuccessMessage")
            : OpenKeyUILocalization::GetString(L"SaveFailureMessage"));
    }
}
