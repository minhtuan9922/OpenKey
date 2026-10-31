#include "../Sources/OpenKey/win32/OpenKey/OpenKey/CaretSessionRestore.h"
#include <iostream>
#include <iomanip>
#include <cstdlib>
using Microsoft::WRL::ComPtr;
class ProfileSink : public ITfInputProcessorProfileActivationSink {
    ULONG references = 1;
public:
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID id, void** out) override {
        if (!out) return E_POINTER; *out = nullptr;
        if (id != IID_IUnknown && id != IID_ITfInputProcessorProfileActivationSink) return E_NOINTERFACE;
        *out = static_cast<ITfInputProcessorProfileActivationSink*>(this); AddRef(); return S_OK;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return ++references; }
    ULONG STDMETHODCALLTYPE Release() override { const auto left = --references; if (!left) delete this; return left; }
    HRESULT STDMETHODCALLTYPE OnActivated(DWORD type, LANGID language, REFCLSID clsid, REFGUID category, REFGUID profile, HKL, DWORD flags) override {
        if (category != GUID_TFCAT_TIP_KEYBOARD) return S_OK;
        TF_INPUTPROCESSORPROFILE value{}; value.dwProfileType=type; value.langid=language; value.clsid=clsid; value.guidProfile=profile;
        std::cout << "{\"event\":\"profile\",\"active\":" << ((flags & TF_IPSINK_FLAG_ACTIVE) ? "true" : "false")
            << ",\"ours\":" << (OpenKeyCaretSession::IsOurs(value) ? "true" : "false") << ",\"type\":" << type << ",\"language\":" << language
            << ",\"userDesktop\":" << (OpenKeyCaretSession::NativeBackend{}.OnUserDesktop() ? "true" : "false") << "}\n" << std::flush;
        return S_OK;
    }
};
void Snapshot(const char* event) {
    using namespace OpenKeyCaretSession;
    Apartment apartment;
    ComPtr<ITfInputProcessorProfiles> profiles;
    ComPtr<ITfInputProcessorProfileMgr> manager;
    TF_INPUTPROCESSORPROFILE active{}; BOOL enabled = FALSE;
    HRESULT created = apartment.Ready() ? CoCreateInstance(CLSID_TF_InputProcessorProfiles, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&profiles)) : apartment.result;
    HRESULT queried = FAILED(created) ? created : profiles.As(&manager);
    HRESULT read = FAILED(queried) ? queried : manager->GetActiveProfile(GUID_TFCAT_TIP_KEYBOARD, &active);
    HRESULT enabledResult = FAILED(created) ? created : profiles->IsEnabledLanguageProfile(OpenKeyTextService::ServiceId, OpenKeyTextService::Language, OpenKeyTextService::ProfileId, &enabled);
    std::cout << "{\"event\":\"" << event << "\",\"activeResult\":\"0x" << std::hex << std::setw(8) << std::setfill('0') << static_cast<unsigned>(read)
        << "\",\"enabledResult\":\"0x" << std::setw(8) << static_cast<unsigned>(enabledResult) << std::dec
        << "\",\"ours\":" << (read == S_OK && IsOurs(active) ? "true" : "false") << ",\"enabled\":" << (enabled ? "true" : "false")
        << ",\"type\":" << active.dwProfileType << ",\"language\":" << active.langid
        << ",\"userDesktop\":" << (NativeBackend{}.OnUserDesktop() ? "true" : "false") << "}\n" << std::flush;
}
LRESULT CALLBACK ProbeWindow(HWND window, UINT message, WPARAM w, LPARAM l) {
    if (message == WM_WTSSESSION_CHANGE) {
        if (w == WTS_SESSION_LOCK) Snapshot("lock");
        if (w == WTS_SESSION_UNLOCK) { Snapshot("unlock"); SetTimer(window, 1, 1000, nullptr); }
    } else if (message == WM_TIMER) {
        KillTimer(window, 1); Snapshot("after-unlock");
    }
    return DefWindowProcW(window, message, w, l);
}
int main(int argc, char** argv) {
    const int seconds = argc > 1 ? std::atoi(argv[1]) : 120;
    if (seconds < 1 || seconds > 300) return 1;
    OpenKeyCaretSession::Apartment apartment;
    ComPtr<ITfThreadMgrEx> thread; ComPtr<ITfSource> source; ComPtr<ProfileSink> sink; sink.Attach(new ProfileSink);
    TfClientId client = TF_CLIENTID_NULL; DWORD cookie = TF_INVALID_COOKIE;
    HRESULT observer = CoCreateInstance(CLSID_TF_ThreadMgr, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&thread));
    if (SUCCEEDED(observer)) observer = thread->ActivateEx(&client, TF_TMAE_NOACTIVATETIP | TF_TMAE_NOACTIVATEKEYBOARDLAYOUT);
    if (SUCCEEDED(observer)) observer = thread.As(&source);
    if (SUCCEEDED(observer)) observer = source->AdviseSink(IID_ITfInputProcessorProfileActivationSink, sink.Get(), &cookie);
    std::cout << "{\"event\":\"observer\",\"result\":\"0x" << std::hex << static_cast<unsigned>(observer) << std::dec << "\"}\n";
    WNDCLASSW type{}; type.lpfnWndProc = ProbeWindow; type.hInstance = GetModuleHandleW(nullptr); type.lpszClassName = L"OpenKey.ReadOnlySessionProbe";
    if (!RegisterClassW(&type)) return 1;
    HWND window = CreateWindowW(type.lpszClassName, L"", 0, 0, 0, 0, 0, nullptr, nullptr, type.hInstance, nullptr);
    if (!window) return 1;
    const BOOL registered = WTSRegisterSessionNotification(window, NOTIFY_FOR_THIS_SESSION);
    std::cout << "{\"event\":\"register\",\"success\":" << (registered ? "true" : "false") << ",\"error\":" << (registered ? 0 : GetLastError()) << "}\n";
    Snapshot("start");
    const auto until = GetTickCount64() + seconds * 1000ull;
    while (GetTickCount64() < until) {
        MSG msg;
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) { TranslateMessage(&msg); DispatchMessageW(&msg); }
        Sleep(50);
    }
    if (registered) WTSUnRegisterSessionNotification(window);
    DestroyWindow(window); UnregisterClassW(type.lpszClassName, type.hInstance);
    if (source && cookie != TF_INVALID_COOKIE) source->UnadviseSink(cookie);
    if (thread && client != TF_CLIENTID_NULL) thread->Deactivate();
    return 0;
}
