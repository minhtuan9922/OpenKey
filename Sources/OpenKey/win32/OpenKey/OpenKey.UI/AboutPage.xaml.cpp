#include "pch.h"
#include "AboutPage.xaml.h"
#include "Localization.h"

#include <winrt/Windows.System.h>
#include <winrt/Windows.Web.Http.h>

#include <cctype>
#include <optional>

#if __has_include("AboutPage.g.cpp")
#include "AboutPage.g.cpp"
#endif

using namespace winrt;
using namespace Microsoft::UI::Xaml;

namespace
{
    constexpr wchar_t OpenKeyVersion[] = L"2.0.5";
    constexpr uint32_t OpenKeyVersionCode = 0x00020005;
    constexpr wchar_t VersionEndpoint[] =
        L"https://raw.githubusercontent.com/tuyenvm/OpenKey/master/version.json";

    struct UpdateInfo
    {
        hstring versionName;
        uint32_t versionCode = 0;
    };

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

    std::optional<UpdateInfo> parseUpdateInfo(const hstring& content)
    {
        const std::string data = to_string(content);
        size_t section = data.find("\"latestWinVersion\"");
        if (section == std::string::npos)
        {
            return std::nullopt;
        }

        size_t nameKey = data.find("\"versionName\"", section);
        size_t nameColon = data.find(':', nameKey);
        size_t nameStart = data.find('"', nameColon + 1);
        size_t nameEnd = data.find('"', nameStart + 1);
        size_t codeKey = data.find("\"versionCode\"", nameEnd);
        size_t codeColon = data.find(':', codeKey);
        size_t codeStart = data.find_first_of("0123456789", codeColon + 1);
        size_t codeEnd = data.find_first_not_of("0123456789", codeStart);
        if (nameKey == std::string::npos || nameColon == std::string::npos ||
            nameStart == std::string::npos || nameEnd == std::string::npos ||
            codeKey == std::string::npos || codeColon == std::string::npos ||
            codeStart == std::string::npos)
        {
            return std::nullopt;
        }

        try
        {
            UpdateInfo result;
            result.versionName = to_hstring(data.substr(nameStart + 1, nameEnd - nameStart - 1));
            result.versionCode = static_cast<uint32_t>(std::stoul(
                data.substr(codeStart, codeEnd == std::string::npos ? std::string::npos : codeEnd - codeStart)));
            return result;
        }
        catch (...)
        {
            return std::nullopt;
        }
    }

}

namespace winrt::OpenKeyUI::implementation
{
    AboutPage::AboutPage()
    {
        InitializeComponent();
        VersionText().Text(OpenKeyUILocalization::Format(L"AboutVersionFormat", OpenKeyVersion));
        BuildDateText().Text(OpenKeyUILocalization::Format(L"AboutBuildDateFormat", buildDate()));
    }

    fire_and_forget AboutPage::OnLinkClicked(IInspectable const& sender, RoutedEventArgs const&)
    {
        auto lifetime = get_strong();
        const auto button = sender.as<Controls::Button>();
        const auto address = unbox_value<hstring>(button.Tag());

        bool opened = false;
        try
        {
            opened = co_await Windows::System::Launcher::LaunchUriAsync(Windows::Foundation::Uri(address));
        }
        catch (hresult_error const&)
        {
        }

        LinkStatus().IsOpen(!opened);
        if (!opened)
        {
            LinkStatus().Title(OpenKeyUILocalization::GetString(L"LinkFailureTitle"));
            LinkStatus().Message(OpenKeyUILocalization::GetString(L"LinkFailureMessage"));
        }
    }

    fire_and_forget AboutPage::OnCheckUpdatesClicked(IInspectable const&, RoutedEventArgs const&)
    {
        auto lifetime = get_strong();
        CheckUpdateButton().IsEnabled(false);
        UpdateProgress().IsActive(true);
        LinkStatus().IsOpen(false);

        std::optional<UpdateInfo> update;
        try
        {
            Windows::Web::Http::HttpClient client;
            const auto content = co_await client.GetStringAsync(Windows::Foundation::Uri(VersionEndpoint));
            update = parseUpdateInfo(content);
        }
        catch (hresult_error const&)
        {
        }

        UpdateProgress().IsActive(false);
        CheckUpdateButton().IsEnabled(true);
        if (!update)
        {
            LinkStatus().Severity(Controls::InfoBarSeverity::Error);
            LinkStatus().Title(OpenKeyUILocalization::GetString(L"UpdateCheckFailureTitle"));
            LinkStatus().Message(OpenKeyUILocalization::GetString(L"UpdateCheckFailureMessage"));
            LinkStatus().IsOpen(true);
            co_return;
        }

        if (update->versionCode <= OpenKeyVersionCode)
        {
            LinkStatus().Severity(Controls::InfoBarSeverity::Success);
            LinkStatus().Title(OpenKeyUILocalization::GetString(L"UpdateCurrentTitle"));
            LinkStatus().Message(OpenKeyUILocalization::GetString(L"UpdateCurrentMessage"));
            LinkStatus().IsOpen(true);
            co_return;
        }

        Controls::ContentDialog dialog;
        dialog.XamlRoot(XamlRoot());
        dialog.Title(box_value(OpenKeyUILocalization::Format(L"UpdateAvailableFormat", update->versionName)));
        dialog.Content(box_value(OpenKeyUILocalization::GetString(L"UpdateConfirmMessage")));
        dialog.PrimaryButtonText(OpenKeyUILocalization::GetString(L"UpdateButtonText"));
        dialog.CloseButtonText(OpenKeyUILocalization::GetString(L"UpdateLaterButtonText"));
        dialog.DefaultButton(Controls::ContentDialogButton::Primary);
        if (co_await dialog.ShowAsync() != Controls::ContentDialogResult::Primary)
        {
            co_return;
        }

        bool opened = false;
        try
        {
            opened = co_await Windows::System::Launcher::LaunchUriAsync(
                Windows::Foundation::Uri(L"https://github.com/tuyenvm/OpenKey/releases"));
        }
        catch (hresult_error const&)
        {
        }
        if (!opened)
        {
            LinkStatus().Severity(Controls::InfoBarSeverity::Error);
            LinkStatus().Title(OpenKeyUILocalization::GetString(L"LinkFailureTitle"));
            LinkStatus().Message(OpenKeyUILocalization::GetString(L"LinkFailureMessage"));
            LinkStatus().IsOpen(true);
            co_return;
        }

    }
}
