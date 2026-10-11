#include "pch.h"
#include "OverviewPage.xaml.h"
#include "Localization.h"
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
        LanguageToggle().IsOn(m_preferences.language != 0);
        UpdateLanguageDescription();
        m_loadingPreferences = false;
        // Only observe while this page is loaded. Weak handlers avoid retaining
        // a page after navigation, and refresh must never save back to the engine.
        m_refreshTimer = DispatcherQueue().CreateTimer();
        m_refreshTimer.Interval(std::chrono::milliseconds(500));
        m_refreshTimer.Tick([weak = get_weak()](auto const&, auto const&)
        {
            if (auto page = weak.get()) page->RefreshLanguage();
        });
        Loaded([weak = get_weak()](auto const&, auto const&)
        {
            if (auto page = weak.get())
            {
                page->RefreshLanguage();
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

    void OverviewPage::RefreshLanguage()
    {
        // Keep a failed edit visible, consistent with the other settings pages.
        if (m_saveFailed) return;
        m_preferences = SettingsService::readPreferences();
        const bool enabled = m_preferences.language != 0;
        if (LanguageToggle().IsOn() == enabled) return;
        m_loadingPreferences = true;
        LanguageToggle().IsOn(enabled);
        UpdateLanguageDescription();
        m_loadingPreferences = false;
    }

    void OverviewPage::OnLanguageToggled(IInspectable const&, RoutedEventArgs const&)
    {
        if (m_loadingPreferences)
        {
            return;
        }

        // Do not write the stale snapshot from when the page was opened.
        m_preferences = SettingsService::readPreferences();
        auto baseline = m_preferences;
        m_preferences.language = LanguageToggle().IsOn() ? 1 : 0;
        UpdateLanguageDescription();
        const bool saved = SettingsService::saveChangedPreferences(baseline, m_preferences);
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
