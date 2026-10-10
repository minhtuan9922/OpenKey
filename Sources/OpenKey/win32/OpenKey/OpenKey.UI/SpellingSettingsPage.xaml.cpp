#include "pch.h"
#include "SpellingSettingsPage.xaml.h"
#include "Localization.h"
#include <chrono>

#if __has_include("SpellingSettingsPage.g.cpp")
#include "SpellingSettingsPage.g.cpp"
#endif

using namespace winrt;
using namespace Microsoft::UI::Xaml;

namespace winrt::OpenKeyUI::implementation
{
    SpellingSettingsPage::SpellingSettingsPage()
    {
        InitializeComponent();
        m_preferences = SettingsService::readPreferences();
        m_savedPreferences = m_preferences;
        CheckSpellingToggle().IsOn(m_preferences.checkSpelling != 0);
        RestoreWrongSpellingToggle().IsOn(m_preferences.restoreIfWrongSpelling != 0);
        ModernOrthographyToggle().IsOn(m_preferences.useModernOrthography != 0);
        QuickTelexToggle().IsOn(m_preferences.quickTelex != 0);
        QuickStartConsonantToggle().IsOn(m_preferences.quickStartConsonant != 0);
        QuickEndConsonantToggle().IsOn(m_preferences.quickEndConsonant != 0);
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

    SpellingSettingsPage::~SpellingSettingsPage()
    {
        if (m_refreshTimer) m_refreshTimer.Stop();
    }

    void SpellingSettingsPage::RefreshPreferences()
    {
        // Preserve pending edits after a save error until a successful retry.
        if (m_saveFailed) return;
        m_preferences = SettingsService::readPreferences();
        m_savedPreferences = m_preferences;
        m_loadingPreferences = true;
        const auto update = [](Controls::ToggleSwitch const& toggle, const int value)
        {
            if (toggle.IsOn() != (value != 0)) toggle.IsOn(value != 0);
        };
        update(CheckSpellingToggle(), m_preferences.checkSpelling);
        update(RestoreWrongSpellingToggle(), m_preferences.restoreIfWrongSpelling);
        update(ModernOrthographyToggle(), m_preferences.useModernOrthography);
        update(QuickTelexToggle(), m_preferences.quickTelex);
        update(QuickStartConsonantToggle(), m_preferences.quickStartConsonant);
        update(QuickEndConsonantToggle(), m_preferences.quickEndConsonant);
        m_loadingPreferences = false;
    }

    void SpellingSettingsPage::OnPreferenceToggled(IInspectable const&, RoutedEventArgs const&)
    {
        if (m_loadingPreferences) return;
        m_preferences.checkSpelling = CheckSpellingToggle().IsOn() ? 1 : 0;
        m_preferences.restoreIfWrongSpelling = RestoreWrongSpellingToggle().IsOn() ? 1 : 0;
        m_preferences.useModernOrthography = ModernOrthographyToggle().IsOn() ? 1 : 0;
        m_preferences.quickTelex = QuickTelexToggle().IsOn() ? 1 : 0;
        m_preferences.quickStartConsonant = QuickStartConsonantToggle().IsOn() ? 1 : 0;
        m_preferences.quickEndConsonant = QuickEndConsonantToggle().IsOn() ? 1 : 0;
        SavePreferences();
    }

    void SpellingSettingsPage::SavePreferences()
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
        SettingsStatus().Title(saved
            ? OpenKeyUILocalization::GetString(L"SaveSuccessTitle")
            : OpenKeyUILocalization::GetString(L"SaveFailureTitle"));
        SettingsStatus().Message(saved
            ? OpenKeyUILocalization::GetString(L"SpellingSaveSuccessMessage")
            : OpenKeyUILocalization::GetString(L"SaveFailureMessage"));
    }
}
