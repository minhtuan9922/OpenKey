#include "CaretTextService.h"
#include <string>
using Microsoft::WRL::ComPtr;
namespace OpenKeyTextService { extern HMODULE module; }
namespace {
    const wchar_t ClassPath[] = L"SOFTWARE\\Classes\\CLSID\\{727F9E3B-0491-4D5C-B33D-64D59BE28A70}";
    HRESULT RegistryString(const std::wstring& path, const wchar_t* name, const std::wstring& value) {
        HKEY key = nullptr;
        LSTATUS error = RegCreateKeyExW(HKEY_LOCAL_MACHINE, path.c_str(), 0, nullptr, 0, KEY_SET_VALUE, nullptr, &key, nullptr);
        if (error == ERROR_SUCCESS) {
            error = RegSetValueExW(key, name, 0, REG_SZ, reinterpret_cast<const BYTE*>(value.c_str()),
                static_cast<DWORD>((value.size() + 1) * sizeof(wchar_t)));
            RegCloseKey(key);
        }
        return HRESULT_FROM_WIN32(error);
    }
    HRESULT Unregister() {
        using namespace OpenKeyTextService;
        ComPtr<ITfInputProcessorProfiles> profiles; ComPtr<ITfCategoryMgr> categories;
        HRESULT result = CoCreateInstance(CLSID_TF_InputProcessorProfiles, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&profiles));
        if (SUCCEEDED(result)) {
            BSTR description = nullptr;
            const bool existed = SUCCEEDED(profiles->GetLanguageProfileDescription(ServiceId, Language, ProfileId, &description));
            SysFreeString(description);
            result = profiles->Unregister(ServiceId);
            if (!existed && FAILED(result)) result = S_OK; // Idempotent uninstall / second architecture.
        }
        if (SUCCEEDED(CoCreateInstance(CLSID_TF_CategoryMgr, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&categories))))
            categories->UnregisterCategory(ServiceId, GUID_TFCAT_TIP_KEYBOARD, ServiceId);
        const LSTATUS removed = RegDeleteTreeW(HKEY_LOCAL_MACHINE, ClassPath);
        if (removed != ERROR_SUCCESS && removed != ERROR_FILE_NOT_FOUND) return HRESULT_FROM_WIN32(removed);
        return result;
    }
}
extern "C" HRESULT WINAPI DllRegisterServer() {
    using namespace OpenKeyTextService;
    const HRESULT initialized = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(initialized)) return initialized;
    HRESULT result = E_FAIL;
    HKEY previous = nullptr;
    const bool existed = RegOpenKeyExW(HKEY_LOCAL_MACHINE, ClassPath, 0, KEY_QUERY_VALUE, &previous) == ERROR_SUCCESS;
    if (previous) RegCloseKey(previous);
    try {
        wchar_t file[32768]{};
        const DWORD length = GetModuleFileNameW(module, file, 32768);
        if (length && length < 32768) {
            result = RegistryString(ClassPath, nullptr, L"OpenKey Caret Editing");
            if (SUCCEEDED(result)) result = RegistryString(std::wstring(ClassPath) + L"\\InprocServer32", nullptr, file);
            if (SUCCEEDED(result)) result = RegistryString(std::wstring(ClassPath) + L"\\InprocServer32", L"ThreadingModel", L"Apartment");
            ComPtr<ITfInputProcessorProfiles> profiles; ComPtr<ITfCategoryMgr> categories;
            if (SUCCEEDED(result)) result = CoCreateInstance(CLSID_TF_InputProcessorProfiles, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&profiles));
            if (SUCCEEDED(result)) result = profiles->Register(ServiceId);
            const wchar_t description[] = L"OpenKey Caret Editing (experimental)";
            if (SUCCEEDED(result)) result = profiles->AddLanguageProfile(ServiceId, Language, ProfileId, description,
                ARRAYSIZE(description) - 1, file, length, 0);
            if (SUCCEEDED(result)) {
                // Keep physical Telex/VNI keys on a US base layout, even though
                // the selected TSF profile is grouped under Vietnamese.
                HKL base = LoadKeyboardLayoutW(L"00000409", KLF_NOTELLSHELL);
                result = base ? profiles->SubstituteKeyboardLayout(ServiceId, Language, ProfileId, base) : E_FAIL;
            }
            if (SUCCEEDED(result)) result = CoCreateInstance(CLSID_TF_CategoryMgr, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&categories));
            if (SUCCEEDED(result)) result = categories->RegisterCategory(ServiceId, GUID_TFCAT_TIP_KEYBOARD, ServiceId);
            // Register and enable this optional profile for the installing user,
            // but do not activate it or change the default input language.
            if (SUCCEEDED(result)) result = profiles->EnableLanguageProfile(ServiceId, Language, ProfileId, TRUE);
            if (FAILED(result) && !existed) Unregister();
        }
    } catch (...) { if (!existed) Unregister(); result = E_OUTOFMEMORY; }
    CoUninitialize(); return result;
}
extern "C" HRESULT WINAPI DllUnregisterServer() {
    const HRESULT initialized = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(initialized)) return initialized;
    const HRESULT result = Unregister(); CoUninitialize(); return result;
}
