#pragma once

#include "MainWindow.g.h"
#include "UiWindowSizeSettings.h"
#include <winrt/Windows.UI.ViewManagement.h>

namespace winrt::OpenKeyUI::implementation
{
    struct MainWindow : MainWindowT<MainWindow>
    {
        MainWindow();
        void OpenPage(winrt::hstring const& page);

        void OnNavigationSelectionChanged(
            winrt::Microsoft::UI::Xaml::Controls::NavigationView const& sender,
            winrt::Microsoft::UI::Xaml::Controls::NavigationViewSelectionChangedEventArgs const& args);

    private:
        void ConfigureInitialWindow();
        void ConfigureTitleBar();
        void UpdateTitleBarLayout();
        void UpdateTitleBarTheme();
        void ApplySavedTheme();
        void NavigateTo(wchar_t const* pageName);
        void CaptureNormalWindow();
        void SaveWindowState();
        OpenKeyUIWindowSize::NormalGeometry m_normalGeometry;
        winrt::Windows::UI::ViewManagement::AccessibilitySettings m_accessibility;
        winrt::Windows::UI::ViewManagement::AccessibilitySettings::HighContrastChanged_revoker m_contrastChanged;
        winrt::Windows::UI::ViewManagement::UISettings m_systemSettings;
        winrt::Windows::UI::ViewManagement::UISettings::ColorValuesChanged_revoker m_colorsChanged;
        winrt::Microsoft::UI::Dispatching::DispatcherQueueTimer m_themeTimer{ nullptr };
        int m_appliedTheme = -1;
    };
}

namespace winrt::OpenKeyUI::factory_implementation
{
    struct MainWindow : MainWindowT<MainWindow, implementation::MainWindow>
    {
    };
}
