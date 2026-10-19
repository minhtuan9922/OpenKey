#include "pch.h"
#include "AboutPage.xaml.h"
#include "Localization.h"
#include "../OpenKey/UpdateService.h"
#include "../OpenKey/SetupUpdateService.h"

#include <winrt/Windows.System.h>

#if __has_include("AboutPage.g.cpp")
#include "AboutPage.g.cpp"
#endif

using namespace winrt;
using namespace Microsoft::UI::Xaml;

namespace
{
    constexpr wchar_t OpenKeyVersion[] = L"3.0.8";
    constexpr uint32_t OpenKeyVersionCode = 0x00080003; // Packed 3.0.8, low byte = major.

    hstring buildDate()
    {
        const int size = MultiByteToWideChar(CP_ACP, 0, __DATE__, -1, nullptr, 0);
        if (size <= 1)
        {
            return OpenKeyUILocalization::GetString(L"UnknownBuildDate");
        }

        std::wstring value(static_cast<size_t>(size), L'\0');
        MultiByteToWideChar(CP_ACP, 0, __DATE__, -1, value.data(), size);
        value.resize(static_cast<size_t>(size - 1));
        return hstring(value);
    }

}

namespace winrt::OpenKeyUI::implementation
{
    AboutPage::AboutPage()
    {
        InitializeComponent();
        VersionText().Text(OpenKeyUILocalization::Format(L"AboutVersionFormat", OpenKeyVersion));
        BuildDateText().Text(OpenKeyUILocalization::Format(L"AboutBuildDateFormat", buildDate()));
        Loaded([this](IInspectable const&, RoutedEventArgs const&) {
            updateRequest.Loaded();
            CheckUpdateButton().IsEnabled(!updateRequest.Busy());
            UpdateProgress().IsActive(updateRequest.Busy());
        });
        Unloaded([this](IInspectable const&, RoutedEventArgs const&) {
            updateRequest.Unloaded();
            UpdateProgress().IsActive(false);
        });
    }

    fire_and_forget AboutPage::OnLinkClicked(IInspectable const& sender, RoutedEventArgs const&)
    {
        auto lifetime = get_strong();
        uint64_t ticket = 0;
        if (!updateRequest.BeginStatus(ticket)) co_return;
        apartment_context uiThread;
        LinkStatus().IsOpen(false);

        bool opened = false;
        try
        {
            const auto button = sender.as<Controls::Button>();
            const auto address = unbox_value<hstring>(button.Tag());
            opened = co_await Windows::System::Launcher::LaunchUriAsync(Windows::Foundation::Uri(address));
        }
        catch (hresult_error const&)
        {
        }
        try { co_await uiThread; }
        catch (hresult_error const&) { co_return; }
        if (!updateRequest.Current(ticket)) co_return;
        LinkStatus().IsOpen(!opened);
        if (!opened)
        {
            LinkStatus().Severity(Controls::InfoBarSeverity::Error);
            LinkStatus().Title(OpenKeyUILocalization::GetString(L"LinkFailureTitle"));
            LinkStatus().Message(OpenKeyUILocalization::GetString(L"LinkFailureMessage"));
        }
    }

    void AboutPage::FinishUpdate(uint64_t ticket) noexcept
    {
        updateRequest.Finish(ticket);
        if (!updateRequest.IsLoaded()) return;
        try {
            UpdateProgress().IsActive(updateRequest.Busy());
            CheckUpdateButton().IsEnabled(!updateRequest.Busy());
        }
        catch (hresult_error const&) { /* The window may already be closing. */ }
    }

    fire_and_forget AboutPage::OnCheckUpdatesClicked(IInspectable const&, RoutedEventArgs const&)
    {
        auto lifetime = get_strong();
        uint64_t ticket = 0;
        if (!updateRequest.Begin(ticket)) co_return;
        apartment_context uiThread;
        CheckUpdateButton().IsEnabled(false);
        UpdateProgress().IsActive(true);
        LinkStatus().IsOpen(false);

        UpdateService::Outcome update;
        try
        {
            co_await resume_background();
            update = UpdateService::Check(OpenKeyVersionCode);
        }
        catch (...)
        {
            update = UpdateService::Outcome{};
        }
        try
        {
            co_await uiThread;
        }
        catch (hresult_error const&)
        {
            co_return; // UI apartment is gone: do not touch XAML from a worker.
        }

        if (!updateRequest.Current(ticket)) {
            FinishUpdate(ticket);
            co_return;
        }
        UpdateProgress().IsActive(false);
        if (update.result != UpdateCheckService::Result::Available && update.result != UpdateCheckService::Result::UpToDate)
        {
            LinkStatus().Severity(Controls::InfoBarSeverity::Error);
            LinkStatus().Title(OpenKeyUILocalization::GetString(L"UpdateCheckFailureTitle"));
            LinkStatus().Message(OpenKeyUILocalization::GetString(L"UpdateCheckFailureMessage"));
            LinkStatus().IsOpen(true);
            FinishUpdate(ticket);
            co_return;
        }

        if (update.result == UpdateCheckService::Result::UpToDate)
        {
            LinkStatus().Severity(Controls::InfoBarSeverity::Success);
            LinkStatus().Title(OpenKeyUILocalization::GetString(L"UpdateCurrentTitle"));
            LinkStatus().Message(OpenKeyUILocalization::GetString(L"UpdateCurrentMessage"));
            LinkStatus().IsOpen(true);
            FinishUpdate(ticket);
            co_return;
        }

        Controls::ContentDialogResult confirmation = Controls::ContentDialogResult::None;
        try {
            const auto root = XamlRoot();
            if (!root) {
                FinishUpdate(ticket);
                co_return;
            }
            Controls::ContentDialog dialog;
            dialog.XamlRoot(root);
            dialog.RequestedTheme(ActualTheme());
            dialog.Title(box_value(OpenKeyUILocalization::Format(L"UpdateAvailableFormat", to_hstring(update.version))));
            dialog.Content(box_value(OpenKeyUILocalization::GetString(L"UpdateConfirmMessage")));
            dialog.PrimaryButtonText(OpenKeyUILocalization::GetString(L"UpdateButtonText"));
            dialog.CloseButtonText(OpenKeyUILocalization::GetString(L"UpdateLaterButtonText"));
            dialog.DefaultButton(Controls::ContentDialogButton::Primary);
            confirmation = co_await dialog.ShowAsync();
        }
        catch (hresult_error const&) {}
        try { co_await uiThread; }
        catch (hresult_error const&) { co_return; }
        if (!updateRequest.Current(ticket) || confirmation != Controls::ContentDialogResult::Primary)
        {
            FinishUpdate(ticket);
            co_return;
        }

        auto package = std::make_shared<SetupUpdateService::Package>();
        UpdateProgress().IsActive(true);
        DownloadProgress().Value(0);
        DownloadProgress().Visibility(Visibility::Visible);
        LinkStatus().Severity(Controls::InfoBarSeverity::Informational);
        LinkStatus().Title(OpenKeyUILocalization::GetString(L"UpdateDownloadingTitle"));
        LinkStatus().Message(OpenKeyUILocalization::GetString(L"UpdateDownloadingMessage"));
        LinkStatus().IsOpen(true);
        auto progressTimer = DispatcherQueue().CreateTimer();
        progressTimer.Interval(std::chrono::milliseconds(150));
        progressTimer.Tick([weak = get_weak(), package, ticket](auto const&, auto const&) {
            if (auto page = weak.get(); page && page->updateRequest.Current(ticket))
                page->DownloadProgress().Value(package->percent.load());
        });
        progressTimer.Start();
        bool downloaded = false;
        try {
            co_await resume_background();
            downloaded = SetupUpdateService::Download(update.version, *package);
        }
        catch (...) {}
        try { co_await uiThread; }
        catch (hresult_error const&) { co_return; }
        progressTimer.Stop();
        DownloadProgress().Visibility(Visibility::Collapsed);
        if (!updateRequest.Current(ticket)) {
            FinishUpdate(ticket);
            co_return;
        }
        if (downloaded && SetupUpdateService::Launch(*package))
        {
            if (const auto engine = FindWindowW(L"OpenKeyVietnameseInputMethod", nullptr))
                PostMessageW(engine, WM_USER + 2021, 0, 0);
            FinishUpdate(ticket);
            Application::Current().Exit();
            co_return;
        }
        else
        {
            LinkStatus().Severity(Controls::InfoBarSeverity::Error);
            LinkStatus().Title(OpenKeyUILocalization::GetString(L"UpdateInstallFailureTitle"));
            LinkStatus().Message(OpenKeyUILocalization::GetString(L"UpdateInstallFailureMessage"));
            LinkStatus().IsOpen(true);
        }
        FinishUpdate(ticket);
    }
}
