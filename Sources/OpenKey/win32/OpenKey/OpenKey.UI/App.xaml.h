#pragma once

#include "App.xaml.g.h"
#include <winrt/Microsoft.Windows.AppLifecycle.h>

namespace winrt::OpenKeyUI::implementation
{
    struct App : AppT<App>
    {
        App();
        void OnLaunched(Microsoft::UI::Xaml::LaunchActivatedEventArgs const&);

    private:
        winrt::fire_and_forget RedirectActivationAsync(
            winrt::Microsoft::Windows::AppLifecycle::AppInstance const& instance);
        void OnInstanceActivated(
            winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Microsoft::Windows::AppLifecycle::AppActivationArguments const& args);
        void ActivateMainWindow();

        winrt::Microsoft::UI::Xaml::Window m_window{ nullptr };
        winrt::Microsoft::UI::Dispatching::DispatcherQueue m_dispatcher{ nullptr };
        winrt::Microsoft::Windows::AppLifecycle::AppInstance m_appInstance{ nullptr };
        winrt::event_token m_activationToken{};
    };
}
