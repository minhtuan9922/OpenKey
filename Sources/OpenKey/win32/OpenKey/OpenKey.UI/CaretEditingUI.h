#pragma once
#include <shellapi.h>
#include "../OpenKey.TextService/CaretTextService.h"
#include "../OpenKey.TextService/CaretDeployment.h"
#include "../OpenKey/CaretSelectionSettings.h"
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "uuid.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "advapi32.lib")

namespace OpenKeyUICaret {
    using Microsoft::WRL::ComPtr;
    inline TF_INPUTPROCESSORPROFILE previous{};
    inline bool havePrevious = false;
    struct State { bool installed = false, enabled = false, active = false; };
    inline State Read() {
        State result;
        const auto path = OpenKeyCaretDeployment::RegisteredPath(OpenKeyCaretDeployment::Is64BitWindows());
        result.installed = !path.empty() && GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES;
        ComPtr<ITfInputProcessorProfiles> profiles;
        if (SUCCEEDED(CoCreateInstance(CLSID_TF_InputProcessorProfiles, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&profiles)))) {
            BOOL enabled = FALSE;
            if (SUCCEEDED(profiles->IsEnabledLanguageProfile(OpenKeyTextService::ServiceId, OpenKeyTextService::Language, OpenKeyTextService::ProfileId, &enabled))) result.enabled = enabled != FALSE;
        }
        ComPtr<ITfInputProcessorProfileMgr> manager;
        TF_INPUTPROCESSORPROFILE active{};
        if (SUCCEEDED(CoCreateInstance(CLSID_TF_InputProcessorProfiles, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&manager))) &&
            manager->GetActiveProfile(GUID_TFCAT_TIP_KEYBOARD, &active) == S_OK)
            result.active = active.dwProfileType == TF_PROFILETYPE_INPUTPROCESSOR && active.clsid == OpenKeyTextService::ServiceId && active.guidProfile == OpenKeyTextService::ProfileId;
        return result;
    }
    inline std::wstring Helper() {
        wchar_t file[32768]{}; if (!GetModuleFileNameW(nullptr, file, 32768)) return {};
        std::wstring directory(file); directory.resize(directory.find_last_of(L"\\/"));
        for (const auto& root : {directory, directory.substr(0, directory.find_last_of(L"\\/"))}) {
            const auto candidate = root + L"\\CaretEditing\\OpenKey.CaretSetup.exe";
            if (GetFileAttributesW(candidate.c_str()) != INVALID_FILE_ATTRIBUTES) return candidate;
        }
        return {};
    }
    inline DWORD RunHelper(const std::wstring& helper, bool elevate) {
        const HRESULT initialized = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
        struct Apartment { HRESULT initialized; ~Apartment() { if (SUCCEEDED(initialized)) CoUninitialize(); } } apartment{initialized};
        if (FAILED(initialized) && initialized != RPC_E_CHANGED_MODE) return static_cast<DWORD>(initialized);
        SHELLEXECUTEINFOW info{}; info.cbSize = sizeof(info);
        info.fMask = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_NOASYNC | SEE_MASK_FLAG_NO_UI;
        info.lpVerb = elevate ? L"runas" : L"open"; info.lpFile = helper.c_str();
        info.lpParameters = elevate ? L"--install" : L"--status"; info.nShow = SW_HIDE;
        if (!ShellExecuteExW(&info)) return GetLastError();
        if (!info.hProcess) return ERROR_PROCESS_ABORTED;
        DWORD result = ERROR_TIMEOUT;
        if (WaitForSingleObject(info.hProcess, 180000) == WAIT_OBJECT_0) GetExitCodeProcess(info.hProcess, &result);
        CloseHandle(info.hProcess); return result;
    }
    inline HRESULT Activate(bool enable) {
        ComPtr<ITfInputProcessorProfiles> profiles; ComPtr<ITfInputProcessorProfileMgr> manager;
        HRESULT result = CoCreateInstance(CLSID_TF_InputProcessorProfiles, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&profiles));
        if (FAILED(result) || FAILED(result = profiles.As(&manager))) return result;
        if (enable) {
            TF_INPUTPROCESSORPROFILE active{};
            if (manager->GetActiveProfile(GUID_TFCAT_TIP_KEYBOARD, &active) == S_OK &&
                !(active.dwProfileType == TF_PROFILETYPE_INPUTPROCESSOR && active.clsid == OpenKeyTextService::ServiceId)) {
                previous = active; havePrevious = true;
            }
            result = profiles->EnableLanguageProfile(OpenKeyTextService::ServiceId, OpenKeyTextService::Language, OpenKeyTextService::ProfileId, TRUE);
            if (result != S_OK) return FAILED(result) ? result : E_FAIL;
            result = profiles->ChangeCurrentLanguage(OpenKeyTextService::Language);
            if (FAILED(result)) return result;
            result = manager->ActivateProfile(TF_PROFILETYPE_INPUTPROCESSOR, OpenKeyTextService::Language,
                OpenKeyTextService::ServiceId, OpenKeyTextService::ProfileId, nullptr, TF_IPPMF_FORSESSION | TF_IPPMF_ENABLEPROFILE);
            if (result == S_OK && !OpenKeyCaretSettings::Save(true)) return HRESULT_FROM_WIN32(ERROR_WRITE_FAULT);
            return result;
        }
        if (Read().active) {
            TF_INPUTPROCESSORPROFILE restore = previous;
            if (!havePrevious) {
                HKL layout = nullptr; SystemParametersInfoW(SPI_GETDEFAULTINPUTLANG, 0, &layout, 0);
                if (!layout) layout = LoadKeyboardLayoutW(L"00000409", KLF_NOTELLSHELL);
                restore = {}; restore.dwProfileType = TF_PROFILETYPE_KEYBOARDLAYOUT;
                restore.langid = LOWORD(reinterpret_cast<ULONG_PTR>(layout)); restore.hkl = layout;
            }
            result = profiles->ChangeCurrentLanguage(restore.langid);
            if (FAILED(result)) return result;
            result = manager->ActivateProfile(restore.dwProfileType, restore.langid, restore.clsid, restore.guidProfile, restore.hkl, TF_IPPMF_FORSESSION);
            if (result != S_OK) return FAILED(result) ? result : E_FAIL;
        }
        result = profiles->EnableLanguageProfile(OpenKeyTextService::ServiceId, OpenKeyTextService::Language, OpenKeyTextService::ProfileId, FALSE);
        if (result == S_OK && !OpenKeyCaretSettings::Save(false)) return HRESULT_FROM_WIN32(ERROR_WRITE_FAULT);
        return result;
    }
}
