#include "pch.h"
#include "App.xaml.h"
#include "MainWindow.xaml.h"
#include "UiLanguageSettings.h"

#include <winrt/Microsoft.Windows.AppLifecycle.h>
#include <winrt/Microsoft.Windows.Globalization.h>

using namespace winrt;
using namespace Microsoft::UI::Xaml;

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
        Microsoft::Windows::AppLifecycle::AppActivationArguments const&)
    {
        if (!m_dispatcher)
        {
            return;
        }

        auto lifetime = get_strong();
        m_dispatcher.TryEnqueue([lifetime]()
        {
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
