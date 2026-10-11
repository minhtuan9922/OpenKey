#include "pch.h"
#include "App.xaml.h"
#include "MainWindow.xaml.h"
#include "UiLanguageSettings.h"
#include "UiLaunchRoute.h"
#include <shellapi.h>
#include <winrt/Windows.ApplicationModel.Activation.h>

#include <winrt/Microsoft.Windows.AppLifecycle.h>
#include <winrt/Microsoft.Windows.Globalization.h>

using namespace winrt;
using namespace Microsoft::UI::Xaml;

namespace
{
    winrt::hstring RequestedPage(winrt::Microsoft::Windows::AppLifecycle::AppActivationArguments const& args)
    {
        try
        {
            const auto launch = args.Data().try_as<winrt::Windows::ApplicationModel::Activation::ILaunchActivatedEventArgs>();
            if (!launch) return {};
            const auto command = launch.Arguments();
            if (command.empty()) return {};
            int count = 0;
            auto arguments = CommandLineToArgvW(command.c_str(), &count);
            if (!arguments) return {};
            struct ArgumentsOwner { LPWSTR* value; ~ArgumentsOwner() { LocalFree(value); } } owner{ arguments };
            const auto page = OpenKeyUILaunch::Page(count, arguments);
            return winrt::hstring(page);
        }
        catch (winrt::hresult_error const&) { return {}; }
    }
}

namespace winrt::OpenKeyUI::implementation
{
    App::App()
    {
        // Set before XAML or ResourceLoader resolves any UI strings. The
        // unpackaged override is process-local, so restore our choice each run.
        Microsoft::Windows::Globalization::ApplicationLanguages::PrimaryLanguageOverride(
            OpenKeyUILanguage::Tag(OpenKeyUILanguage::Read()));
#if defined _DEBUG && !defined DISABLE_XAML_GENERATED_BREAK_ON_UNHANDLED_EXCEPTION
        UnhandledException([](IInspectable const&, UnhandledExceptionEventArgs const& args)
        {
            if (IsDebuggerPresent())
            {
                [[maybe_unused]] auto message = args.Message();
                __debugbreak();
            }
        });
#endif
    }

    void App::OnLaunched([[maybe_unused]] LaunchActivatedEventArgs const& args)
    {
        using Microsoft::Windows::AppLifecycle::AppInstance;

        m_appInstance = AppInstance::FindOrRegisterForKey(L"OpenKey.UI.MainWindow");
        if (!m_appInstance.IsCurrent())
        {
            RedirectActivationAsync(m_appInstance);
            return;
        }

        m_dispatcher = Microsoft::UI::Dispatching::DispatcherQueue::GetForCurrentThread();
        m_activationToken = m_appInstance.Activated({ this, &App::OnInstanceActivated });
        m_window = make<MainWindow>();
        get_self<MainWindow>(m_window.as<OpenKeyUI::MainWindow>())->OpenPage(
            RequestedPage(AppInstance::GetCurrent().GetActivatedEventArgs()));
        ActivateMainWindow();
    }

    fire_and_forget App::RedirectActivationAsync(
        Microsoft::Windows::AppLifecycle::AppInstance const& instance)
    {
        auto lifetime = get_strong();
        try
        {
            const auto activation = Microsoft::Windows::AppLifecycle::AppInstance::GetCurrent()
                .GetActivatedEventArgs();
            co_await instance.RedirectActivationToAsync(activation);
        }
        catch (hresult_error const&)
        {
        }
        Application::Current().Exit();
    }

    void App::OnInstanceActivated(
        IInspectable const&,
        Microsoft::Windows::AppLifecycle::AppActivationArguments const& args)
    {
        if (!m_dispatcher)
        {
            return;
        }

        auto lifetime = get_strong();
        const auto page = RequestedPage(args);
        m_dispatcher.TryEnqueue([lifetime, page]()
        {
            if (lifetime->m_window && !page.empty())
                get_self<MainWindow>(lifetime->m_window.as<OpenKeyUI::MainWindow>())->OpenPage(page);
            lifetime->ActivateMainWindow();
        });
    }

    void App::ActivateMainWindow()
    {
        if (!m_window)
        {
            return;
        }

        HWND windowHandle = nullptr;
        if (const auto windowNative = m_window.try_as<::IWindowNative>();
            windowNative && SUCCEEDED(windowNative->get_WindowHandle(&windowHandle)) && windowHandle != nullptr)
        {
            if (IsIconic(windowHandle))
            {
                ShowWindow(windowHandle, SW_RESTORE);
            }
            else
            {
                ShowWindow(windowHandle, SW_SHOW);
            }
        }

        m_window.Activate();
        if (windowHandle != nullptr)
        {
            SetForegroundWindow(windowHandle);
        }
    }
}
