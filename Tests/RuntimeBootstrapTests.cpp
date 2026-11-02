#include <windows.h>
#include <appmodel.h>
#include <roapi.h>
#include <winstring.h>
#include <inspectable.h>
#include <cstdio>

// Probe the same bootstrap DLL and minimum as the UI, without creating a window.
int wmain(int argc, wchar_t** argv) {
    if (argc != 2) return 2;
    const auto module = LoadLibraryExW(argv[1], nullptr, LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
    if (!module) return 3;
    using Initialize = HRESULT(WINAPI*)(UINT32, PCWSTR, PACKAGE_VERSION, UINT32);
    using Shutdown = void(WINAPI*)();
    const auto initialize = reinterpret_cast<Initialize>(GetProcAddress(module, "MddBootstrapInitialize2"));
    const auto shutdown = reinterpret_cast<Shutdown>(GetProcAddress(module, "MddBootstrapShutdown"));
    if (!initialize || !shutdown) return 4;
    PACKAGE_VERSION minimum{}; minimum.Version = 0x0002000500010000ULL;
    const auto result = initialize(0x00020005, L"", minimum, 0);
    if (FAILED(result)) { std::printf("Bootstrap HRESULT: %08lx\n", result); return 5; }
    RoInitialize(RO_INIT_MULTITHREADED);
    HSTRING name{};
    WindowsCreateString(L"Microsoft.UI.Xaml.Application", 29, &name);
    IInspectable* factory = nullptr;
    const auto activation = RoGetActivationFactory(name, IID_IInspectable, reinterpret_cast<void**>(&factory));
    WindowsDeleteString(name);
    if (factory) factory->Release();
    RoUninitialize();
    shutdown();
    FreeLibrary(module);
    if (FAILED(activation)) { std::printf("XAML activation HRESULT: %08lx\n", activation); return 6; }
    return 0;
}
