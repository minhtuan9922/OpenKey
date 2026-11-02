#pragma once
#include "../OpenKey.TextService/CaretTextService.h"
#include "CaretSelectionProtocol.h"
#include "CaretSelectionSettings.h"
#include <wtsapi32.h>
#pragma comment(lib, "wtsapi32.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "uuid.lib")

namespace OpenKeyCaretSession {
    struct ProfileState { bool known = false, ours = false, enabled = false, otherService = false; };
    inline bool ShellSurface(const wchar_t* windowClass, const wchar_t* executable) {
        for (const auto* name : {L"Shell_TrayWnd", L"Shell_SecondaryTrayWnd", L"Progman", L"WorkerW"})
            if (!_wcsicmp(windowClass, name)) return true;
        for (const auto* name : {L"StartMenuExperienceHost.exe", L"SearchHost.exe", L"SearchApp.exe", L"ShellExperienceHost.exe"})
            if (!_wcsicmp(executable, name)) return true;
        return false;
    }
    inline bool ShellWindow(HWND window) {
        wchar_t windowClass[256]{};
        if (!window || !GetClassNameW(window, windowClass, ARRAYSIZE(windowClass))) return false;
        if (ShellSurface(windowClass, L"")) return true;
        DWORD process = 0; GetWindowThreadProcessId(window, &process);
        HANDLE handle = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, process);
        if (!handle) return false;
        wchar_t file[1024]{}; DWORD size = ARRAYSIZE(file);
        const bool read = QueryFullProcessImageNameW(handle, 0, file, &size) != FALSE;
        CloseHandle(handle);
        const wchar_t* name = wcsrchr(file, L'\\');
        return read && ShellSurface(windowClass, name ? name + 1 : file);
    }
    class ShellTransition {
        bool left = false, right = false, pending = false, explicitSwitch = false;
    public:
        void Key(UINT key, bool down, bool shell) {
            if (key == VK_LWIN || key == VK_RWIN) {
                if (key == VK_LWIN) left = down; else right = down;
                if (down) { pending = true; explicitSwitch = false; }
            } else if (down && key == VK_SPACE && (left || right)) {
                pending = false; explicitSwitch = true; // Win+Space is a deliberate layout choice.
            } else if (down && !left && !right && !shell) {
                pending = false; explicitSwitch = false;
            }
        }
        void Foreground(bool shell) {
            // Win+Space can move focus into the shell's language picker before
            // its chosen profile arrives. Keep that explicit gesture valid.
            if (shell) pending = true;
            else if (!left && !right) pending = false;
        }
        bool Ignore(bool shell) const { return !explicitSwitch && (pending || shell); }
        bool WatchingKeys() const { return left || right || pending || explicitSwitch; }
        void Reset() { left = right = pending = explicitSwitch = false; }
    };
    class SelectionMemory {
        HWND foreground = nullptr;
        ProfileState state;
        bool frozen = false;
        ShellTransition shell;
    public:
        void Key(UINT key, bool down, bool shellSurface) { if (!frozen) shell.Key(key, down, shellSurface); }
        bool WatchingKeys() const { return !frozen && shell.WatchingKeys(); }
        void Foreground(HWND window, bool userDesktop, bool shellSurface = false) {
            if (frozen || !userDesktop || !window) return;
            shell.Foreground(shellSurface);
            if (shell.Ignore(shellSurface) || window == foreground) return;
            foreground = window; state = {};
        }
        bool Observe(HWND window, bool selected, bool userDesktop, bool shellSurface = false) {
            if (frozen || !userDesktop || !window || shell.Ignore(shellSurface)) return false;
            foreground = window; state = {true, selected, selected, false};
            return true;
        }
        ProfileState Lock() { frozen = true; return state; }
        void Unlock() { frozen = false; shell.Reset(); }
        void Reset() { frozen = false; foreground = nullptr; state = {}; shell.Reset(); }
    };
    // Changes the running desktop only; never changes Windows' default input profile.
    class Recovery {
        bool locked = false, restore = false;
        unsigned attempts = 0;
    public:
        void Startup(bool selected) { locked = restore = false; attempts = selected ? 60 : 0; }
        void Lock(ProfileState state) {
            if (locked) return;
            locked = true; restore = state.known && state.ours && state.enabled; attempts = 0;
        }
        bool Unlock() {
            if (!locked) return false;
            locked = false; attempts = restore ? 10 : 0; restore = false;
            return Pending();
        }
        bool Pending() const { return attempts != 0; }
        void Cancel() { locked = restore = false; attempts = 0; }
        template<class Backend> bool Tick(Backend& backend) {
            if (locked || !attempts) return false;
            --attempts;
            // Never inspect or activate a profile on Winlogon/UAC desktops.
            if (!backend.OnUserDesktop()) return Pending();
            const auto state = backend.Read();
            if (!state.known) return Pending();
            if (!state.enabled || state.otherService) { attempts = 0; return false; }
            if (state.ours || backend.Restore()) attempts = 0;
            return Pending();
        }
    };
    inline bool IsOurs(const TF_INPUTPROCESSORPROFILE& profile) {
        return profile.dwProfileType == TF_PROFILETYPE_INPUTPROCESSOR &&
            profile.langid == OpenKeyTextService::Language && profile.clsid == OpenKeyTextService::ServiceId &&
            profile.guidProfile == OpenKeyTextService::ProfileId;
    }
    inline ProfileState ForegroundState(bool enabled, LANGID language) {
        return {language != 0, false, enabled,
            language && language != OpenKeyTextService::Language && PRIMARYLANGID(language) != LANG_ENGLISH};
    }
    struct Apartment {
        HRESULT result = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
        ~Apartment() { if (SUCCEEDED(result)) CoUninitialize(); }
        bool Ready() const { return SUCCEEDED(result) || result == RPC_E_CHANGED_MODE; }
    };
    struct NativeBackend {
        bool OnUserDesktop() const {
            return OpenKeyCaretSelection::OnUserDesktop();
        }
        ProfileState Read() const {
            Apartment apartment;
            ProfileState state;
            if (!apartment.Ready()) return state;
            Microsoft::WRL::ComPtr<ITfInputProcessorProfiles> profiles;
            BOOL enabled = FALSE;
            if (FAILED(CoCreateInstance(CLSID_TF_InputProcessorProfiles, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&profiles))) ||
                profiles->IsEnabledLanguageProfile(OpenKeyTextService::ServiceId, OpenKeyTextService::Language, OpenKeyTextService::ProfileId, &enabled) != S_OK) return state;
            // GetActiveProfile on this background thread can remain ENG while
            // the foreground editor uses our TIP. Never use it as proof that
            // foreground restoration has already completed.
            DWORD thread = GetWindowThreadProcessId(GetForegroundWindow(), nullptr);
            const LANGID language = thread ? LOWORD(reinterpret_cast<ULONG_PTR>(GetKeyboardLayout(thread))) : 0;
            return ForegroundState(enabled != FALSE, language);
        }
        bool Restore() const {
            Apartment apartment;
            if (!apartment.Ready()) return false;
            Microsoft::WRL::ComPtr<ITfInputProcessorProfiles> profiles;
            Microsoft::WRL::ComPtr<ITfInputProcessorProfileMgr> manager;
            BOOL enabled = FALSE;
            if (FAILED(CoCreateInstance(CLSID_TF_InputProcessorProfiles, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&profiles))) ||
                FAILED(profiles.As(&manager)) ||
                profiles->IsEnabledLanguageProfile(OpenKeyTextService::ServiceId, OpenKeyTextService::Language, OpenKeyTextService::ProfileId, &enabled) != S_OK || !enabled ||
                FAILED(profiles->ChangeCurrentLanguage(OpenKeyTextService::Language))) return false;
            return manager->ActivateProfile(TF_PROFILETYPE_INPUTPROCESSOR, OpenKeyTextService::Language,
                OpenKeyTextService::ServiceId, OpenKeyTextService::ProfileId, nullptr, TF_IPPMF_FORSESSION) == S_OK;
        }
    };
    class Monitor {
        Recovery recovery;
        SelectionMemory selection;
        NativeBackend backend;
        OpenKeyCaretSettings::Store store;
        OpenKeyCaretSettings::RememberedChoice remembered;
        HWND owner = nullptr;
        bool startup = false;
        DWORD session = 0;
        bool registered = false;
        HWND classifiedWindow = nullptr;
        bool classifiedShell = false;
        bool IsShell(HWND window) {
            if (window != classifiedWindow) { classifiedWindow = window; classifiedShell = ShellWindow(window); }
            return classifiedShell;
        }
        static constexpr UINT_PTR Timer = 0x4f4b5301;
        void Trace(OpenKeyCaretTrace::Stage stage, ProfileState state = {}, HRESULT result = S_OK) {
            OpenKeyCaretTrace::Record record; record.kind = 2; record.stage = stage; record.hr = result;
            record.flags = (state.known ? 1u : 0u) | (state.ours ? 2u : 0u) | (state.enabled ? 4u : 0u) |
                (state.otherService ? 8u : 0u) | (recovery.Pending() ? 16u : 0u) | (backend.OnUserDesktop() ? 32u : 0u);
            OpenKeyCaretTrace::Publish(record);
        }
    public:
        void Start(HWND window) {
            owner = window;
            startup = remembered.Load(store);
            recovery.Startup(startup);
            if (startup && !SetTimer(window, Timer, 500, nullptr)) { recovery.Cancel(); startup = false; }
            if (!registered && ProcessIdToSessionId(GetCurrentProcessId(), &session))
                registered = WTSRegisterSessionNotification(window, NOTIFY_FOR_THIS_SESSION) != FALSE;
            Trace(OpenKeyCaretTrace::SessionRegister, {}, registered ? S_OK : HRESULT_FROM_WIN32(GetLastError()));
        }
        void Stop(HWND window) {
            KillTimer(window, Timer); recovery.Cancel(); selection.Reset();
            if (registered) WTSUnRegisterSessionNotification(window);
            registered = false;
            owner = nullptr; startup = false;
        }
        void BeginEndSession(HWND window) {
            selection.Lock(); recovery.Cancel(); startup = false; KillTimer(window, Timer);
        }
        void CancelEndSession() { selection.Unlock(); }
        void Session(HWND window, WPARAM event, LPARAM id) {
            if (!registered || static_cast<DWORD>(id) != session) return;
            if (event == WTS_SESSION_LOCK) {
                KillTimer(window, Timer); const auto saved = selection.Lock(); recovery.Lock(saved);
                Trace(OpenKeyCaretTrace::SessionLock, saved);
            } else if (event == WTS_SESSION_UNLOCK) {
                selection.Unlock();
                if (recovery.Unlock() && !SetTimer(window, Timer, 500, nullptr)) recovery.Cancel();
                Trace(OpenKeyCaretTrace::SessionUnlock);
            } else if (event == WTS_SESSION_LOGOFF) {
                BeginEndSession(window);
            }
        }
        void Foreground(HWND window) {
            if (window == GetForegroundWindow()) selection.Foreground(window, backend.OnUserDesktop(), IsShell(window));
        }
        void Key(UINT key, bool down) {
            if (key != VK_LWIN && key != VK_RWIN && !selection.WatchingKeys()) return;
            if (backend.OnUserDesktop()) selection.Key(key, down, IsShell(GetForegroundWindow()));
        }
        void Observe(HWND target, bool selected) {
            if (target == GetForegroundWindow()) {
                const bool accepted = selection.Observe(target, selected, backend.OnUserDesktop(), IsShell(target));
                if (accepted) {
                    const bool saved = remembered.Observe(selected, store);
                    if (!saved) Trace(OpenKeyCaretTrace::SessionSelection, {}, HRESULT_FROM_WIN32(ERROR_WRITE_FAULT));
                    if (startup) { recovery.Cancel(); startup = false; if (owner) KillTimer(owner, Timer); }
                }
                Trace(accepted ? OpenKeyCaretTrace::SessionSelection : OpenKeyCaretTrace::SessionIgnored, {true, selected, selected, false});
            }
        }
        bool Tick(HWND window, WPARAM timer) {
            if (timer != Timer) return false;
            if (!recovery.Tick(backend)) { KillTimer(window, Timer); startup = false; }
            Trace(recovery.Pending() ? OpenKeyCaretTrace::SessionRetry : OpenKeyCaretTrace::SessionDone);
            return true;
        }
    };
}
