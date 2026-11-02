// Read-only prerequisite probe. OS WinRT only; /MT avoids a VC runtime dependency.
#include <windows.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Management.Deployment.h>
#include <winrt/Windows.ApplicationModel.h>
#include <winrt/Windows.System.h>
#include <string_view>

int wmain(int argc, wchar_t** argv) noexcept {
    if (argc != 2) return 2;
    using namespace winrt::Windows;
    const auto architecture = std::wstring_view(argv[1]) == L"x64" ? System::ProcessorArchitecture::X64 : System::ProcessorArchitecture::X86;
    if (std::wstring_view(argv[1]) != L"x64" && std::wstring_view(argv[1]) != L"x86") return 2;
    try {
        winrt::init_apartment(winrt::apartment_type::multi_threaded);
        Management::Deployment::PackageManager manager;
        for (const auto& package : manager.FindPackagesForUser(L"", L"Microsoft.WindowsAppRuntime.2_8wekyb3d8bbwe")) {
            const auto id = package.Id();
            const auto version = id.Version();
            const auto packed = (static_cast<unsigned long long>(version.Major) << 48) |
                (static_cast<unsigned long long>(version.Minor) << 32) |
                (static_cast<unsigned long long>(version.Build) << 16) | version.Revision;
            if (package.IsFramework() && id.Architecture() == architecture &&
                packed >= 0x0002000500010000ULL && package.Status().VerifyIsOK()) return 0;
        }
        return 1;
    } catch (...) { return 2; }
}
