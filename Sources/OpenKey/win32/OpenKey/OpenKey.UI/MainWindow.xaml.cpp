#include "pch.h"
#include "MainWindow.xaml.h"
#include <chrono>
#include "InputSettingsPage.xaml.h"
#include "OverviewPage.xaml.h"
#include "SpellingSettingsPage.xaml.h"
#include "HotkeySettingsPage.xaml.h"
#include "CompatibilitySettingsPage.xaml.h"
#include "AdvancedSettingsPage.xaml.h"
#include "MacroPage.xaml.h"
#include "ConvertPage.xaml.h"
#include "AboutPage.xaml.h"
#include "UiWindowSizeSettings.h"
#include "UiTheme.h"
#include "AppResources.h"
#include <winrt/Microsoft.UI.Interop.h>
#include <winrt/Windows.UI.h>
#include <winrt/Microsoft.UI.Composition.SystemBackdrops.h>
#include <winrt/Microsoft.UI.Xaml.Media.Animation.h>

#if __has_include("MainWindow.g.cpp")
#include "MainWindow.g.cpp"
#endif

using namespace winrt;
using namespace Microsoft::UI::Xaml;
using namespace Microsoft::UI::Xaml::Media;

namespace winrt::OpenKeyUI::implementation
{
    MainWindow::MainWindow()
    {
        InitializeComponent();
        // Shared resource handle remains valid for the process; do not DestroyIcon.
        if (const auto module = GetModuleHandleW(nullptr))
        {
            const auto icon = static_cast<HICON>(LoadImageW(module, MAKEINTRESOURCEW(IDI_OPENKEY_UI),
                IMAGE_ICON, 0, 0, LR_DEFAULTSIZE | LR_SHARED));
            if (icon)
            {
                try { AppWindow().SetIcon(Microsoft::UI::GetIconIdFromIcon(icon)); }
                catch (hresult_error const&) { /* Optional branding must not prevent startup. */ }
            }
        }
        ApplySavedTheme();
        m_themeTimer = DispatcherQueue().CreateTimer();
        m_themeTimer.Interval(std::chrono::milliseconds(500));
        m_themeTimer.Tick([weak = get_weak()](auto const&, auto const&)
        {
            if (auto window = weak.get())
            {
                if (window->m_appliedTheme != static_cast<int>(OpenKeyUITheme::Read()))
                {
                    window->ApplySavedTheme();
                    window->UpdateTitleBarTheme();
                }
            }
        });
        m_themeTimer.Start();
        // Keep the XAML fallback brush visible if material is unavailable or
        // initialization fails. Built-in Mica handles system material policies.
        try
        {
            if (Microsoft::UI::Composition::SystemBackdrops::MicaController::IsSupported())
            {
                SystemBackdrop(MicaBackdrop{});
                BackdropFallback().Visibility(Visibility::Collapsed);
            }
        }
        catch (hresult_error const&) { SystemBackdrop(nullptr); }
        ConfigureTitleBar();
        RootLayout().ActualThemeChanged([weak = get_weak()](auto const&, auto const&)
        {
            if (auto window = weak.get())
                window->DispatcherQueue().TryEnqueue([weak]
                {
                    if (auto current = weak.get()) current->UpdateTitleBarTheme();
                });
        });
        m_colorsChanged = m_systemSettings.ColorValuesChanged(auto_revoke,
            [weak = get_weak(), dispatcher = DispatcherQueue()](auto const&, auto const&)
        {
            dispatcher.TryEnqueue([weak]
            {
                if (auto window = weak.get())
                {
                    window->ApplySavedTheme();
                    window->UpdateTitleBarTheme();
                }
            });
        });
        // This optional event subscription can fail in desktop environments.
        // Do not let it abort window creation. ActualThemeChanged and
        // ColorValuesChanged above still refresh the caption's system colors.
        try
        {
            m_contrastChanged = m_accessibility.HighContrastChanged(auto_revoke,
                [weak = get_weak(), dispatcher = DispatcherQueue()](auto const&, auto const&)
            {
                dispatcher.TryEnqueue([weak]
                {
                    if (auto window = weak.get()) window->UpdateTitleBarTheme();
                });
            });
        }
        catch (hresult_error const&)
        {
            OutputDebugStringW(L"OpenKey.UI: HighContrastChanged subscription unavailable; using theme/color events.\n");
        }
        ConfigureInitialWindow();
        CaptureNormalWindow();
        AppWindow().Changed([weak = get_weak()](auto const&, auto const& args)
        {
            if (args.DidSizeChange() || args.DidPositionChange())
            {
                if (auto window = weak.get())
                {
                    window->CaptureNormalWindow();
                    window->UpdateTitleBarLayout();
                }
            }
        });
        AppWindow().Closing([weak = get_weak()](auto const&, auto const&)
        {
            if (auto window = weak.get())
            {
                window->SaveWindowState();
                window->m_themeTimer.Stop();
            }
        });
        if (OpenKeyUIWindowSize::ReadMaximized())
        {
            if (const auto presenter = AppWindow().Presenter().try_as<Microsoft::UI::Windowing::OverlappedPresenter>())
                presenter.Maximize();
        }
        NavigateTo(L"OpenKeyUI.OverviewPage");
    }

    void MainWindow::ApplySavedTheme()
    {
        const auto theme = OpenKeyUITheme::Read();
        RootLayout().RequestedTheme(OpenKeyUITheme::Resolve(theme, m_systemSettings));
        m_appliedTheme = static_cast<int>(theme);
    }

    void MainWindow::ConfigureTitleBar()
    {
        if (!Microsoft::UI::Windowing::AppWindowTitleBar::IsCustomizationSupported()) return;
        ExtendsContentIntoTitleBar(true);
        const auto titleBar = AppWindow().TitleBar();
        titleBar.ButtonBackgroundColor(Windows::UI::Colors::Transparent());
        titleBar.ButtonInactiveBackgroundColor(Windows::UI::Colors::Transparent());
        TitleBarDragRegion().Visibility(Visibility::Visible);
        SetTitleBar(TitleBarDragRegion());
        UpdateTitleBarLayout();
        TitleBarDragRegion().Loaded([weak = get_weak()](auto const&, auto const&)
        {
            if (auto window = weak.get())
            {
                window->UpdateTitleBarLayout();
                window->UpdateTitleBarTheme();
            }
        });
        Activated([weak = get_weak()](auto const&, auto const& args)
        {
            if (auto window = weak.get()) window->TitleBarText().Opacity(
                args.WindowActivationState() == WindowActivationState::Deactivated ? 0.6 : 1.0);
        });
    }

    void MainWindow::UpdateTitleBarLayout()
    {
        if (!ExtendsContentIntoTitleBar()) return;
        HWND windowHandle = nullptr;
        const auto native = this->try_as<::IWindowNative>();
        if (!native || FAILED(native->get_WindowHandle(&windowHandle)) || !windowHandle) return;
        const auto dpi = GetDpiForWindow(windowHandle);
        const auto titleBar = AppWindow().TitleBar();
        TitleBarLeftInset().Width(GridLength{OpenKeyUIWindowSize::LogicalMetric(titleBar.LeftInset(), dpi), GridUnitType::Pixel});
        TitleBarRightInset().Width(GridLength{OpenKeyUIWindowSize::LogicalMetric(titleBar.RightInset(), dpi), GridUnitType::Pixel});
        TitleBarDragRegion().Height(titleBar.Height() > 0
            ? OpenKeyUIWindowSize::LogicalMetric(titleBar.Height(), dpi) : 32.0);
    }

    void MainWindow::UpdateTitleBarTheme()
    {
        if (!ExtendsContentIntoTitleBar()) return;
        const auto titleBar = AppWindow().TitleBar();
        const auto foreground = TitleBarText().Foreground().try_as<SolidColorBrush>();
        const auto hover = CaptionHoverPalette().Background().try_as<SolidColorBrush>();
        const auto pressed = CaptionPressedPalette().Background().try_as<SolidColorBrush>();
        if (!foreground || !hover || !pressed) return;
        auto textColor = foreground.Color();
        auto hoverColor = hover.Color();
        auto pressedColor = pressed.Color();
        if (m_accessibility.HighContrast())
        {
            const auto systemColor = [](int index)
            {
                const auto color = GetSysColor(index);
                return Windows::UI::Color{255, GetRValue(color), GetGValue(color), GetBValue(color)};
            };
            textColor = systemColor(COLOR_WINDOWTEXT);
            hoverColor = pressedColor = systemColor(COLOR_WINDOW);
        }
        titleBar.ButtonForegroundColor(textColor);
        titleBar.ButtonInactiveForegroundColor(textColor);
        titleBar.ButtonBackgroundColor(Windows::UI::Colors::Transparent());
        titleBar.ButtonInactiveBackgroundColor(Windows::UI::Colors::Transparent());
        titleBar.ButtonHoverForegroundColor(textColor);
        titleBar.ButtonPressedForegroundColor(textColor);
        titleBar.ButtonHoverBackgroundColor(hoverColor);
        titleBar.ButtonPressedBackgroundColor(pressedColor);
    }

    void MainWindow::ConfigureInitialWindow()
    {
        HWND windowHandle = nullptr;
        check_hresult(this->try_as<::IWindowNative>()->get_WindowHandle(&windowHandle));

        const auto desired = OpenKeyUIWindowSize::Read();
        OpenKeyUIWindowSize::Position position{};
        const bool hasPosition = OpenKeyUIWindowSize::ReadPosition(position);

        const auto displayArea = hasPosition
            ? Microsoft::UI::Windowing::DisplayArea::GetFromPoint({position.x, position.y},
                Microsoft::UI::Windowing::DisplayAreaFallback::Nearest)
            : Microsoft::UI::Windowing::DisplayArea::GetFromWindowId(
                AppWindow().Id(), Microsoft::UI::Windowing::DisplayAreaFallback::Nearest);
        if (!displayArea)
        {
            const UINT windowDpi = GetDpiForWindow(windowHandle);
            AppWindow().Resize({
                OpenKeyUIWindowSize::Pixels(desired.width, windowDpi),
                OpenKeyUIWindowSize::Pixels(desired.height, windowDpi) });
            return;
        }

        const auto workArea = displayArea.WorkArea();
        if (hasPosition)
        {
            // Before activation, place wholly on the target display so HWND DPI
            // follows that display before calculating our final logical size.
            AppWindow().MoveAndResize(workArea);
        }
        const UINT windowDpi = GetDpiForWindow(windowHandle);
        const int windowWidth = OpenKeyUIWindowSize::Fit(desired.width, workArea.Width, windowDpi);
        const int windowHeight = OpenKeyUIWindowSize::Fit(desired.height, workArea.Height, windowDpi);

        AppWindow().MoveAndResize({
            hasPosition ? OpenKeyUIWindowSize::ClampPosition(position.x, workArea.X, workArea.Width, windowWidth)
                : workArea.X + ((workArea.Width - windowWidth) / 2),
            hasPosition ? OpenKeyUIWindowSize::ClampPosition(position.y, workArea.Y, workArea.Height, windowHeight)
                : workArea.Y + ((workArea.Height - windowHeight) / 2),
            windowWidth,
            windowHeight });
    }

    void MainWindow::CaptureNormalWindow()
    {
        HWND windowHandle = nullptr;
        const auto native = this->try_as<::IWindowNative>();
        if (!native || FAILED(native->get_WindowHandle(&windowHandle)) || !windowHandle) return;
        RECT bounds{};
        if (!GetWindowRect(windowHandle, &bounds)) return;
        const auto width = static_cast<std::int64_t>(bounds.right) - bounds.left;
        const auto height = static_cast<std::int64_t>(bounds.bottom) - bounds.top;
        if (width <= 0 || height <= 0 || width > INT32_MAX || height > INT32_MAX) return;
        OpenKeyUIWindowSize::CaptureNormalGeometry(m_normalGeometry, OpenKeyUIWindowSize::LogicalSize(
            static_cast<int>(width), static_cast<int>(height), GetDpiForWindow(windowHandle)),
            {bounds.left, bounds.top}, IsIconic(windowHandle) != FALSE, IsZoomed(windowHandle) != FALSE);
    }

    void MainWindow::SaveWindowState()
    {
        HWND windowHandle = nullptr;
        const auto native = this->try_as<::IWindowNative>();
        if (!native || FAILED(native->get_WindowHandle(&windowHandle)) || !windowHandle || IsIconic(windowHandle)) return;
        CaptureNormalWindow();
        // Save cached normal geometry, never the maximized or minimized rectangle.
        // Optional UI preferences: errors must not prevent closing or reload the engine.
        if (m_normalGeometry.valid)
        {
            OpenKeyUIWindowSize::Save(m_normalGeometry.size);
            OpenKeyUIWindowSize::SavePosition(m_normalGeometry.position);
        }
        OpenKeyUIWindowSize::SaveMaximized(IsZoomed(windowHandle) != FALSE);
    }

    void MainWindow::OpenPage(hstring const& page)
    {
        const auto select = [&](auto const& items)
        {
            for (auto const& item : items)
            {
                if (auto navigation = item.template try_as<Controls::NavigationViewItem>())
                {
                    if (navigation.Tag() && unbox_value<hstring>(navigation.Tag()) == page)
                    {
                        RootNavigation().SelectedItem(navigation);
                        return true;
                    }
                }
            }
            return false;
        };
        if (!select(RootNavigation().MenuItems())) select(RootNavigation().FooterMenuItems());
    }

    void MainWindow::NavigateTo(wchar_t const* pageName)
    {
        const Windows::UI::Xaml::Interop::TypeName pageType{
            pageName, Windows::UI::Xaml::Interop::TypeKind::Custom};
        if (ContentFrame().CurrentSourcePageType().Name == pageType.Name) return;
        if (m_systemSettings.AnimationsEnabled()) ContentFrame().Navigate(pageType);
        else ContentFrame().Navigate(pageType, nullptr, Media::Animation::SuppressNavigationTransitionInfo{});
    }

    void MainWindow::OnNavigationSelectionChanged(
        Controls::NavigationView const&,
        Controls::NavigationViewSelectionChangedEventArgs const& args)
    {
        const auto selectedItem = args.SelectedItemContainer();
        if (!selectedItem || !selectedItem.Tag())
        {
            return;
        }

        const auto tag = unbox_value<hstring>(selectedItem.Tag());
        if (tag == L"overview")
        {
            NavigateTo(L"OpenKeyUI.OverviewPage");
        }
        else if (tag == L"input")
        {
            NavigateTo(L"OpenKeyUI.InputSettingsPage");
        }
        else if (tag == L"spelling")
        {
            NavigateTo(L"OpenKeyUI.SpellingSettingsPage");
        }
        else if (tag == L"hotkeys")
        {
            NavigateTo(L"OpenKeyUI.HotkeySettingsPage");
        }
        else if (tag == L"compatibility")
        {
            NavigateTo(L"OpenKeyUI.CompatibilitySettingsPage");
        }
        else if (tag == L"advanced")
        {
            NavigateTo(L"OpenKeyUI.AdvancedSettingsPage");
        }
        else if (tag == L"macro")
        {
            NavigateTo(L"OpenKeyUI.MacroPage");
        }
        else if (tag == L"convert")
        {
            NavigateTo(L"OpenKeyUI.ConvertPage");
        }
        else if (tag == L"about")
        {
            NavigateTo(L"OpenKeyUI.AboutPage");
        }
    }
}
