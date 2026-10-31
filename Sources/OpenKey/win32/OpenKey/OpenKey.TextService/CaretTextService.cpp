#include "CaretTextService.h"
#include <initguid.h>
#include <inputscope.h>
#include <atomic>
#include <functional>
#include <new>
#include "../OpenKey/CaretSettingsProtocol.h"
#include "../OpenKey/TsfTyping.h"

using Microsoft::WRL::ComPtr;
namespace OpenKeyTextService {
    HMODULE module = nullptr;
    std::atomic<long> objects{0}, serverLocks{0};
    bool WordHost() {
        wchar_t path[MAX_PATH]{};
        if (!GetModuleFileNameW(nullptr, path, MAX_PATH)) return false;
        const auto* file = wcsrchr(path, L'\\');
        return _wcsicmp(file ? file + 1 : path, L"WINWORD.EXE") == 0;
    }
    bool RendererHost() {
        wchar_t path[MAX_PATH]{};
        if (!GetModuleFileNameW(nullptr, path, MAX_PATH)) return false;
        const auto* name = wcsrchr(path, L'\\'); name = name ? name + 1 : path;
        for (const auto* host : {L"Code.exe", L"Code - Insiders.exe", L"chrome.exe", L"msedge.exe", L"brave.exe"})
            if (_wcsicmp(name, host) == 0) return true;
        return false;
    }
    bool TypingRpc(unsigned part, UINT key, OpenKeyTsfTyping::Result& result) {
        const HWND engine = FindWindowW(L"OpenKeyVietnameseInputMethod", nullptr);
        const HWND target = GetForegroundWindow();
        const auto query = [&](unsigned operation, LPARAM argument, DWORD_PTR& value) {
            return engine && target == GetForegroundWindow() && SendMessageTimeoutW(engine,
                OpenKeyTsfTyping::Message(operation), reinterpret_cast<WPARAM>(target), argument,
                SMTO_ABORTIFHUNG | SMTO_BLOCK, 30, &value);
        };
        DWORD_PTR packet = 0;
        if (!query(part, key, packet)) return false;
        if (part != 1) return packet == 1;
        if ((packet & 0xc0000000u) != 0x40000000u) return false;
        result = {}; result.changed = (packet & 0x200000u) != 0;
        result.removed = (packet >> 13) & 255;
        const unsigned count = packet & 8191, serial = (packet >> 22) & 255;
        for (unsigned i = 0; i < count; ++i) {
            DWORD_PTR character = 0;
            if (!query(2, MAKELPARAM(i, serial), character) || !character || character > 65536) return false;
            result.inserted += static_cast<wchar_t>(character - 1);
        }
        return true;
    }
    Preferences ReadPreferences() {
        Preferences result; HKEY key = nullptr;
        if (RegOpenKeyExW(HKEY_CURRENT_USER, L"SOFTWARE\\TuyenMai\\OpenKey", 0, KEY_QUERY_VALUE, &key) == ERROR_SUCCESS) {
            const auto value = [&](const wchar_t* name, DWORD fallback) {
                DWORD data = fallback, bytes = sizeof(data), type = 0;
                if (RegQueryValueExW(key, name, nullptr, &type, reinterpret_cast<BYTE*>(&data), &bytes) != ERROR_SUCCESS ||
                    type != REG_DWORD || bytes != sizeof(data)) return fallback;
                return data;
            };
            result.enabled = value(L"vLanguage", 1) != 0 && value(L"vCodeTable", 0) == 0;
            result.inputType = static_cast<int>(value(L"vInputType", 0));
            result.modern = value(L"vUseModernOrthography", 0) != 0;
            RegCloseKey(key);
        }
        if (const HWND engine = FindWindowW(L"OpenKeyVietnameseInputMethod", nullptr)) {
            DWORD_PTR packet = 0;
            if (!SendMessageTimeoutW(engine, OpenKeyCaretRuntime::Message(), reinterpret_cast<WPARAM>(GetForegroundWindow()),
                0, SMTO_ABORTIFHUNG | SMTO_BLOCK, 30, &packet) ||
                !OpenKeyCaretRuntime::Decode(packet, result.enabled, result.inputType, result.modern, &result.caretIntent)) result.enabled = false;
        }
        return result;
    }
    bool ReadContext(ITfContext* context, TfEditCookie cookie, OpenKeyCaretTone::Word* word,
        std::wstring& before, std::wstring& after, ComPtr<ITfRange>& caret, OpenKeyCaretTrace::Record* trace, ITfComposition* ownComposition) {
        const auto stage = [&](OpenKeyCaretTrace::Stage value) { if (trace) trace->stage = value; };
        const auto statusOf = [&](HRESULT value) { if (trace && FAILED(value)) trace->hr = value; return value; };
        stage(OpenKeyCaretTrace::Status);
        if (!context) return false;
        TF_STATUS status{};
        if (FAILED(statusOf(context->GetStatus(&status))) || (status.dwDynamicFlags & TF_SD_READONLY)) return false;
        stage(OpenKeyCaretTrace::Composition);
        ComPtr<ITfContextComposition> compositions;
        if (SUCCEEDED(context->QueryInterface(IID_PPV_ARGS(&compositions)))) {
            ComPtr<IEnumITfCompositionView> active; ComPtr<ITfCompositionView> composition; ULONG fetched = 0;
            if (FAILED(statusOf(compositions->EnumCompositions(&active))) ||
                FAILED(statusOf(active->Next(1, &composition, &fetched)))) return false;
            if (!ownComposition && fetched) return false;
            if (ownComposition) {
                if (fetched != 1) return false;
                ComPtr<ITfRange> ownRange, viewRange;
                ComPtr<IUnknown> ownIdentity, viewIdentity;
                LONG start = 1, end = 1; CLSID owner{};
                if (FAILED(ownComposition->GetRange(&ownRange)) || FAILED(composition->GetRange(&viewRange)) ||
                    FAILED(composition->GetOwnerClsid(&owner)) ||
                    FAILED(ownComposition->QueryInterface(IID_PPV_ARGS(&ownIdentity))) || FAILED(composition.As(&viewIdentity)) ||
                    (owner != ServiceId && ownIdentity.Get() != viewIdentity.Get()) ||
                    FAILED(ownRange->CompareStart(cookie, viewRange.Get(), TF_ANCHOR_START, &start)) || start ||
                    FAILED(ownRange->CompareEnd(cookie, viewRange.Get(), TF_ANCHOR_END, &end)) || end) return false;
                ComPtr<ITfCompositionView> extra; fetched = 0;
                if (FAILED(active->Next(1, &extra, &fetched)) || fetched) return false;
            }
        } else if (ownComposition) {
            return false;
        }
        stage(OpenKeyCaretTrace::Selection);
        TF_SELECTION selection{}; ULONG count = 0;
        if (FAILED(statusOf(context->GetSelection(cookie, TF_DEFAULT_SELECTION, 1, &selection, &count))) || count != 1 || !selection.range) return false;
        ComPtr<ITfRange> selected; selected.Attach(selection.range);
        BOOL empty = FALSE;
        if (trace && selection.style.fInterimChar) trace->flags |= 4;
        if (selection.style.fInterimChar || FAILED(statusOf(selected->IsEmpty(cookie, &empty))) || !empty) return false;
        stage(OpenKeyCaretTrace::Scope);
        ComPtr<ITfProperty> scopeProperty;
        if (SUCCEEDED(context->GetProperty(GUID_PROP_INPUTSCOPE, &scopeProperty)) && scopeProperty) {
            VARIANT value; VariantInit(&value);
            const HRESULT read = statusOf(scopeProperty->GetValue(cookie, selected.Get(), &value));
            bool refused = FAILED(read);
            if (!refused && value.vt == VT_UNKNOWN && value.punkVal) {
                ComPtr<ITfInputScope> scopes;
                InputScope* inputScopes = nullptr; UINT length = 0;
                refused = FAILED(value.punkVal->QueryInterface(IID_PPV_ARGS(&scopes))) ||
                    FAILED(scopes->GetInputScopes(&inputScopes, &length));
                if (!refused) for (UINT i = 0; i < length; ++i)
                    if (inputScopes[i] == IS_PASSWORD || inputScopes[i] == IS_NUMERIC_PASSWORD) refused = true;
                CoTaskMemFree(inputScopes);
            }
            VariantClear(&value);
            if (refused) return false;
        }
        ComPtr<ITfRange> left, right;
        LONG moved = 0;
        stage(OpenKeyCaretTrace::Clone);
        if (FAILED(statusOf(selected->Clone(&left))) || FAILED(statusOf(selected->Clone(&right)))) return false;
        stage(OpenKeyCaretTrace::LeftShift);
        if (FAILED(statusOf(left->ShiftStart(cookie, -33, &moved, nullptr)))) return false;
        ComPtr<ITfRange> end; LONG relative = -1;
        const bool atEnd = SUCCEEDED(context->GetEnd(cookie, &end)) && end &&
            SUCCEEDED(selected->CompareEnd(cookie, end.Get(), TF_ANCHOR_END, &relative)) && relative == 0;
        if (trace && atEnd) trace->flags |= 8;
        stage(OpenKeyCaretTrace::RightShift);
        if (!atEnd && FAILED(statusOf(right->ShiftEnd(cookie, 33, &moved, nullptr)))) return false;
        const auto read = [&](ITfRange* range, std::wstring& text) {
            BOOL emptyRange = FALSE;
            if (FAILED(statusOf(range->IsEmpty(cookie, &emptyRange)))) return false;
            text.clear();
            // End of document is a valid boundary. Some providers cannot read
            // a collapsed range there; don't turn an empty right context into
            // a failed word read, or reuse the left side's copied count.
            if (emptyRange) return true;
            wchar_t buffer[34]{}; ULONG copied = 0;
            if (FAILED(statusOf(range->GetText(cookie, 0, buffer, 34, &copied))) || copied > 33) return false;
            text.assign(buffer, copied); return true;
        };
        stage(OpenKeyCaretTrace::LeftRead);
        if (!read(left.Get(), before)) return false;
        if (trace) trace->before = static_cast<int>(before.size());
        stage(OpenKeyCaretTrace::RightRead);
        if (!read(right.Get(), after)) return false;
        if (trace) trace->after = static_cast<int>(after.size());
        stage(OpenKeyCaretTrace::WordBoundary);
        if (word && !OpenKeyCaretTone::Around(before + after, before.size(), *word)) return false;
        stage(OpenKeyCaretTrace::ContextReady);
        caret = std::move(selected); return true;
    }
    bool ChangedSpan(ITfRange* caret, TfEditCookie cookie, const OpenKeyCaretTone::EditPlan& plan, ComPtr<ITfRange>& changed) {
        if (FAILED(caret->Clone(&changed))) return false;
        LONG moved = 0;
        if (plan.offset < 0) {
            if (FAILED(changed->ShiftStart(cookie, plan.offset, &moved, nullptr)) || moved != plan.offset ||
                FAILED(changed->Collapse(cookie, TF_ANCHOR_START))) return false;
        } else {
            if (FAILED(changed->ShiftEnd(cookie, plan.offset, &moved, nullptr)) || moved != plan.offset ||
                FAILED(changed->Collapse(cookie, TF_ANCHOR_END))) return false;
        }
        if (FAILED(changed->ShiftEnd(cookie, static_cast<LONG>(plan.removed.size()), &moved, nullptr)) ||
            moved != static_cast<LONG>(plan.removed.size())) return false;
        wchar_t actual[32]{}; ULONG count = 0;
        if (FAILED(changed->GetText(cookie, 0, actual, 32, &count)) || std::wstring(actual, count) != plan.removed) return false;
        return true;
    }
    bool Apply(ITfContext* context, TfEditCookie cookie, const OpenKeyCaretTone::EditPlan& plan, ITfComposition* ownComposition) {
        std::wstring before, after; ComPtr<ITfRange> caret;
        OpenKeyCaretTone::Word word;
        if (!ReadContext(context, cookie, &word, before, after, caret, nullptr, ownComposition) || word.text != plan.word || word.caret != plan.caret) return false;
        ComPtr<ITfRange> changed;
        if (!ChangedSpan(caret.Get(), cookie, plan, changed)) return false;
        if (FAILED(changed->SetText(cookie, TF_ST_CORRECTION, plan.inserted.c_str(), static_cast<LONG>(plan.inserted.size())))) return false;
        // SetText updates the changed range to cover its inserted content.
        // Restore the caret from this stable anchor, including mid-word edits.
        const LONG delta = -plan.offset;
        LONG moved = 0;
        if (FAILED(changed->Collapse(cookie, TF_ANCHOR_START))) return true;
        if (delta < 0) {
            if (FAILED(changed->ShiftStart(cookie, delta, &moved, nullptr)) || moved != delta ||
                FAILED(changed->Collapse(cookie, TF_ANCHOR_START))) return true;
        } else {
            if (FAILED(changed->ShiftEnd(cookie, delta, &moved, nullptr)) || moved != delta ||
                FAILED(changed->Collapse(cookie, TF_ANCHOR_END))) return true;
        }
        TF_SELECTION selection{changed.Get(), {TF_AE_NONE, FALSE}};
        context->SetSelection(cookie, 1, &selection);
        return true; // A successful SetText must consume the original key once.
    }
    bool ApplyTypingResult(ITfContext* context, TfEditCookie cookie, const OpenKeyTsfTyping::Result& result) {
        std::wstring before, after; ComPtr<ITfRange> caret;
        if (!result.changed || !ReadContext(context, cookie, nullptr, before, after, caret) || result.removed > before.size()) return false;
        ComPtr<ITfRange> changed; LONG moved = 0;
        if (FAILED(caret->Clone(&changed)) || FAILED(changed->ShiftStart(cookie,
            -static_cast<LONG>(result.removed), &moved, nullptr)) || moved != -static_cast<LONG>(result.removed)) return false;
        if (FAILED(changed->SetText(cookie, TF_ST_CORRECTION, result.inserted.c_str(), static_cast<LONG>(result.inserted.size())))) return false;
        if (SUCCEEDED(changed->Collapse(cookie, TF_ANCHOR_END))) {
            TF_SELECTION selection{changed.Get(), {TF_AE_NONE, FALSE}};
            context->SetSelection(cookie, 1, &selection);
        }
        return true;
    }
    class EditSession final : public ITfEditSession {
        ULONG references = 1;
        std::function<void(TfEditCookie)> callback;
    public:
        explicit EditSession(std::function<void(TfEditCookie)> fn) : callback(std::move(fn)) { ++objects; }
        ~EditSession() { --objects; }
        HRESULT STDMETHODCALLTYPE QueryInterface(REFIID id, void** out) override {
            if (!out) return E_POINTER; *out = nullptr;
            if (id == IID_IUnknown || id == IID_ITfEditSession) { *out = static_cast<ITfEditSession*>(this); AddRef(); return S_OK; }
            return E_NOINTERFACE;
        }
        ULONG STDMETHODCALLTYPE AddRef() override { return ++references; }
        ULONG STDMETHODCALLTYPE Release() override { const auto left = --references; if (!left) delete this; return left; }
        HRESULT STDMETHODCALLTYPE DoEditSession(TfEditCookie cookie) override {
            try { callback(cookie); return S_OK; } catch (...) { return E_FAIL; }
        }
    };
    class CompositionSink final : public ITfCompositionSink {
        ULONG references = 1;
    public:
        bool terminated = false;
        CompositionSink() { ++objects; }
        ~CompositionSink() { --objects; }
        HRESULT STDMETHODCALLTYPE QueryInterface(REFIID id, void** out) override {
            if (!out) return E_POINTER; *out = nullptr;
            if (id != IID_IUnknown && id != IID_ITfCompositionSink) return E_NOINTERFACE;
            *out = static_cast<ITfCompositionSink*>(this); AddRef(); return S_OK;
        }
        ULONG STDMETHODCALLTYPE AddRef() override { return ++references; }
        ULONG STDMETHODCALLTYPE Release() override { const auto left = --references; if (!left) delete this; return left; }
        HRESULT STDMETHODCALLTYPE OnCompositionTerminated(TfEditCookie, ITfComposition*) override { terminated = true; return S_OK; }
    };
    class Service final : public ITfTextInputProcessorEx, public ITfKeyEventSink,
        public ITfThreadMgrEventSink, public ITfTextEditSink {
        ULONG references = 1;
        ComPtr<ITfThreadMgr> manager;
        ComPtr<ITfContext> context;
        TfClientId client = TF_CLIENTID_NULL;
        DWORD managerCookie = TF_INVALID_COOKIE, editCookie = TF_INVALID_COOKIE;
        HWND published = nullptr;
        HWND activeWindow = nullptr;
        HWND notifiedEngine = nullptr;
        DWORD notifiedProcess = 0;
        bool notifiedReady = false, bridgeKnown = false;
        bool armed = false, secure = false, foregroundKeys = true;
        bool wordTyping = false;
        bool renderer = false;
        bool reconverting = false;
        Preferences (*preferences)() = ReadPreferences;
        bool (*typing)(unsigned, UINT, OpenKeyTsfTyping::Result&) = TypingRpc;
        bool swallowed[256]{};
        bool typingKeyUp[256]{};
        bool Signal(unsigned kind, HWND target, HWND engine) {
            if (!target || !engine) return false;
            DWORD_PTR acknowledged = 0;
            const auto message = OpenKeyCaretRuntime::PresenceMessage(kind);
            if (SendMessageTimeoutW(engine, message, reinterpret_cast<WPARAM>(target), GetCurrentThreadId(),
                SMTO_ABORTIFHUNG | SMTO_BLOCK, 30, &acknowledged) && acknowledged == 1) return true;
            PostMessageW(engine, message, reinterpret_cast<WPARAM>(target), GetCurrentThreadId());
            return false;
        }
        void NotifyBridge() {
            const HWND engine = FindWindowW(L"OpenKeyVietnameseInputMethod", nullptr);
            DWORD process = 0; if (engine) GetWindowThreadProcessId(engine, &process);
            if (!activeWindow || !engine) { bridgeKnown = false; return; }
            if (!bridgeKnown || notifiedEngine != engine || notifiedProcess != process) {
                bridgeKnown = Signal(OpenKeyCaretRuntime::Activate, activeWindow, engine);
                notifiedEngine = engine; notifiedProcess = process;
                if (bridgeKnown) bridgeKnown = Signal(armed ? OpenKeyCaretRuntime::Ready : OpenKeyCaretRuntime::Unready, activeWindow, engine);
                notifiedReady = armed;
            } else if (notifiedReady != armed) {
                bridgeKnown = Signal(armed ? OpenKeyCaretRuntime::Ready : OpenKeyCaretRuntime::Unready, activeWindow, engine);
                notifiedReady = armed;
            }
            if (wordTyping) Signal(OpenKeyCaretRuntime::RangeTyping, activeWindow, engine);
        }
        void ClearActive() {
            Signal(OpenKeyCaretRuntime::Deactivate, activeWindow, FindWindowW(L"OpenKeyVietnameseInputMethod", nullptr));
            DWORD process = 0;
            if (activeWindow && GetWindowThreadProcessId(activeWindow, &process) && process == GetCurrentProcessId() &&
                GetPropW(activeWindow, ActiveProperty) == this) RemovePropW(activeWindow, ActiveProperty);
            activeWindow = nullptr;
            bridgeKnown = false;
        }
        void RefreshActive(bool fromKey = false) {
            if (secure || !foregroundKeys || !manager) { ClearActive(); return; }
            const HWND focused = GetForegroundWindow();
            // Keep ownership through text/selection callbacks. A renderer's TSF
            // thread need not be the native top-level window's owner thread.
            if (activeWindow == focused && focused) {
                if (fromKey) {
                    const HWND engine = FindWindowW(L"OpenKeyVietnameseInputMethod", nullptr);
                    bridgeKnown = Signal(armed ? OpenKeyCaretRuntime::ActiveReady : OpenKeyCaretRuntime::ActiveUnready, focused, engine);
                    notifiedEngine = engine; notifiedReady = armed;
                    if (engine) GetWindowThreadProcessId(engine, &notifiedProcess);
                    if (wordTyping) Signal(OpenKeyCaretRuntime::RangeTyping, focused, engine);
                } else NotifyBridge();
                return;
            }
            if (!context) { ClearActive(); return; }
            BOOL threadFocus = FALSE;
            if (!fromKey && (FAILED(manager->IsThreadFocus(&threadFocus)) || !threadFocus)) { ClearActive(); return; }
            ComPtr<ITfContextView> view; HWND window = nullptr;
            if (SUCCEEDED(context->GetActiveView(&view)) && view) view->GetWnd(&window);
            // A real foreground key callback is authoritative even when the
            // provider exposes a hidden/renderer HWND rather than the app root.
            if (fromKey) window = focused;
            if (window) window = GetAncestor(window, GA_ROOT); else window = focused;
            if (!window || window != focused) { ClearActive(); return; }
            ClearActive(); activeWindow = window;
            DWORD process = 0; GetWindowThreadProcessId(window, &process);
            // Properties are diagnostics only; IPC works for windowless and
            // renderer-hosted editors without writing foreign HWND properties.
            if (process == GetCurrentProcessId()) SetPropW(window, ActiveProperty, this);
            NotifyBridge();
        }
        void Publish(bool value) {
            DWORD process = 0;
            if (published && GetWindowThreadProcessId(published, &process) && process == GetCurrentProcessId() &&
                GetPropW(published, ArmedProperty) == this) RemovePropW(published, ArmedProperty);
            published = nullptr; armed = value;
            RefreshActive();
            if (activeWindow) {
                GetWindowThreadProcessId(activeWindow, &process);
                if (value && process == GetCurrentProcessId() && SetPropW(activeWindow, ArmedProperty, this)) published = activeWindow;
            }
        }
        void Bind(ITfDocumentMgr* document, ITfContext* keyContext = nullptr) {
            Publish(false);
            if (context && editCookie != TF_INVALID_COOKIE) {
                ComPtr<ITfSource> source; if (SUCCEEDED(context.As(&source))) source->UnadviseSink(editCookie);
            }
            editCookie = TF_INVALID_COOKIE; context.Reset();
            if (keyContext) context = keyContext;
            else if (document) document->GetTop(&context);
            if (context) {
                ComPtr<ITfSource> source;
                if (SUCCEEDED(context.As(&source))) source->AdviseSink(IID_ITfTextEditSink,
                    static_cast<ITfTextEditSink*>(this), &editCookie);
                Publish(false);
            }
        }
        bool Disabled(ITfContext* target) {
            if (secure || !target) return true;
            ComPtr<ITfCompartmentMgr> compartments;
            if (SUCCEEDED(target->QueryInterface(IID_PPV_ARGS(&compartments)))) {
                for (const auto* id : {&GUID_COMPARTMENT_KEYBOARD_DISABLED, &GUID_COMPARTMENT_EMPTYCONTEXT}) {
                    ComPtr<ITfCompartment> part; VARIANT value; VariantInit(&value);
                    bool disabled = FAILED(compartments->GetCompartment(*id, &part)) || FAILED(part->GetValue(&value));
                    if (value.vt == VT_I4 && value.lVal) disabled = true;
                    VariantClear(&value); if (disabled) return true;
                }
            }
            return false;
        }
        bool Request(ITfContext* target, DWORD flags, std::function<void(TfEditCookie)> callback, std::int32_t* failure = nullptr) {
            try {
                ComPtr<ITfEditSession> session; session.Attach(new EditSession(std::move(callback)));
                HRESULT performed = E_FAIL;
                // A refused synchronous lock passes the physical key through;
                // never consume a key and schedule an ambiguous later write.
                const HRESULT requested = target->RequestEditSession(client, session.Get(), TF_ES_SYNC | flags, &performed);
                if (failure) *failure = FAILED(requested) ? requested : FAILED(performed) ? performed : S_OK;
                return SUCCEEDED(requested) && SUCCEEDED(performed);
            } catch (...) { if (failure) *failure = E_FAIL; return false; }
        }
        bool Candidate(ITfContext* target, WPARAM key, bool apply, OpenKeyCaretTrace::Record* trace = nullptr) {
            const auto fail = [&](OpenKeyCaretTrace::Stage stage) { if (trace) trace->stage = stage; return false; };
            if ((OpenKeyCaretTone::EditKey(static_cast<unsigned>(key), 0) < 0 &&
                OpenKeyCaretTone::EditKey(static_cast<unsigned>(key), 1) < 0)) return fail(OpenKeyCaretTrace::Unmapped);
            if (!foregroundKeys) return fail(OpenKeyCaretTrace::Inactive);
            if (target != context.Get()) return fail(OpenKeyCaretTrace::ContextMismatch);
            if (Disabled(target)) return fail(OpenKeyCaretTrace::Disabled);
            if ((GetKeyState(VK_CONTROL) & 0x8000) || (GetKeyState(VK_MENU) & 0x8000) ||
                (GetKeyState(VK_LWIN) & 0x8000) || (GetKeyState(VK_RWIN) & 0x8000)) return fail(OpenKeyCaretTrace::Modifier);
            const auto settings = preferences();
            if (trace) { trace->mode = settings.inputType; trace->flags |= (settings.enabled ? 1u : 0u) | (settings.caretIntent ? 2u : 0u); }
            const int action = OpenKeyCaretTone::EditKey(static_cast<unsigned>(key), settings.inputType);
            if (!settings.enabled) return fail(OpenKeyCaretTrace::SettingsDisabled);
            if (!settings.caretIntent) return fail(OpenKeyCaretTrace::IntentMissing);
            if (action < 0) return fail(OpenKeyCaretTrace::Unmapped);
            bool result = false; OpenKeyCaretTone::EditPlan plan;
            if (trace) trace->stage = OpenKeyCaretTrace::ReadLock;
            std::int32_t lockFailure = 0;
            const bool locked = Request(target, TF_ES_READ, [&](TfEditCookie cookie) {
                std::wstring before, after; ComPtr<ITfRange> caret;
                if (!ReadContext(target, cookie, nullptr, before, after, caret, trace)) return;
                const auto classify = [](wchar_t ch) -> unsigned {
                    if (OpenKeyCaretTone::Letter(ch)) return 1;
                    if (ch == L'\r' || ch == L'\n' || ch == L'\u2028' || ch == L'\u2029') return 2;
                    if (ch == L' ' || ch == L'\t') return 3;
                    if (ch >= L'0' && ch <= L'9') return 4;
                    if (ch >= 0x300 && ch <= 0x36f) return 5;
                    if (ch >= 0x200b && ch <= 0x200f) return 6;
                    return OpenKeyCaretTone::Boundary(ch) ? 7 : 8;
                };
                if (trace) { trace->beforeClass = before.empty() ? 0 : classify(before.back()); trace->afterClass = after.empty() ? 0 : classify(after.front()); }
                OpenKeyCaretTone::Word word;
                if (!OpenKeyCaretTone::Around(before + after, before.size(), word)) { fail(OpenKeyCaretTrace::WordBoundary); return; }
                if (trace) trace->word = static_cast<int>(word.text.size());
                if (!OpenKeyCaretTone::Plan(before, after, action, settings.modern, plan)) { fail(OpenKeyCaretTrace::EditRules); return; }
                if (renderer && !OpenKeyCaretTone::AnchorAtCaret(plan)) { fail(OpenKeyCaretTrace::ForwardUnsupported); return; }
                result = true;
                if (trace) trace->stage = OpenKeyCaretTrace::Ready;
            }, &lockFailure);
            if (!locked && trace) { trace->stage = OpenKeyCaretTrace::ReadLock; trace->hr = lockFailure; }
            if (!result || !apply) return result;
            if (renderer) return Reconvert(target, plan, trace);
            result = false;
            if (trace) trace->stage = OpenKeyCaretTrace::WriteLock;
            const bool written = Request(target, TF_ES_READWRITE, [&](TfEditCookie cookie) {
                result = Apply(target, cookie, plan);
                if (trace) trace->stage = result ? OpenKeyCaretTrace::Applied : OpenKeyCaretTrace::WriteRejected;
            }, &lockFailure);
            if (!written && trace) { trace->stage = OpenKeyCaretTrace::WriteLock; trace->hr = lockFailure; }
            return result;
        }
        bool Reconvert(ITfContext* target, const OpenKeyCaretTone::EditPlan& plan, OpenKeyCaretTrace::Record* trace) {
            Publish(true);
            struct ReconversionScope { bool& active; explicit ReconversionScope(bool& value) : active(value) { active = true; } ~ReconversionScope() { active = false; } } scope(reconverting);
            ComPtr<ITfComposition> composition;
            ComPtr<CompositionSink> sink; sink.Attach(new CompositionSink);
            std::int32_t failure = 0; HRESULT startFailure = S_OK;
            if (trace) trace->stage = OpenKeyCaretTrace::CompositionStart;
            Request(target, TF_ES_READWRITE, [&](TfEditCookie cookie) {
                std::wstring before, after; ComPtr<ITfRange> caret, span; OpenKeyCaretTone::Word word;
                ComPtr<ITfContextComposition> owner;
                if (!ReadContext(target, cookie, &word, before, after, caret) || word.text != plan.word || word.caret != plan.caret ||
                    !ChangedSpan(caret.Get(), cookie, plan, span) || FAILED(target->QueryInterface(IID_PPV_ARGS(&owner)))) return;
                // End this session without SetText: Chromium must first bind a
                // composition to the EXISTING frontend range before replacement.
                startFailure = owner->StartComposition(cookie, span.Get(), sink.Get(), &composition);
            }, &failure);
            if (!composition) {
                if (trace) { trace->stage = OpenKeyCaretTrace::CompositionDenied; trace->hr = FAILED(startFailure) ? startFailure : failure; }
                return false;
            }
            bool written = false, ended = false;
            Request(target, TF_ES_READWRITE, [&](TfEditCookie cookie) {
                if (!sink->terminated) written = Apply(target, cookie, plan, composition.Get());
                ended = sink->terminated || SUCCEEDED(composition->EndComposition(cookie));
            }, &failure);
            if (!ended) {
                Request(target, TF_ES_READWRITE, [&](TfEditCookie cookie) { ended = sink->terminated || SUCCEEDED(composition->EndComposition(cookie)); });
                if (!ended) {
                    // Cleanup only, never a delayed SetText. The session holds
                    // the COM/module references and does not capture Service.
                    try {
                        ComPtr<ITfEditSession> cleanup;
                        cleanup.Attach(new EditSession([composition, sink](TfEditCookie cookie) { if (!sink->terminated) composition->EndComposition(cookie); }));
                        HRESULT performed = E_FAIL;
                        target->RequestEditSession(client, cleanup.Get(), TF_ES_ASYNCDONTCARE | TF_ES_READWRITE, &performed);
                    } catch (...) { /* Never reinsert a key after a successful text write. */ }
                }
            }
            if (trace) {
                trace->stage = written ? OpenKeyCaretTrace::Applied : !ended ? OpenKeyCaretTrace::CompositionEnd : OpenKeyCaretTrace::WriteRejected;
                trace->hr = failure; trace->flags |= 64u | (!ended ? 128u : 0u);
            }
            return written; // A successful SetText always consumes the raw key.
        }
        void TraceKey(OpenKeyCaretTrace::Record record, WPARAM key, bool eaten) {
            if (OpenKeyCaretTone::EditKey(static_cast<unsigned>(key), 0) < 0 && OpenKeyCaretTone::EditKey(static_cast<unsigned>(key), 1) < 0) return;
            record.eaten = eaten; record.flags |= (renderer ? 16u : 0u) | (wordTyping ? 32u : 0u);
            OpenKeyCaretTrace::Publish(record);
        }
        void PrepareKeyContext(ITfContext* target, WPARAM key, LPARAM flags) {
            // KeyUp can be lost when the host changes focus or routes IME keys.
            // Windows' previous-state bit identifies a NEW physical press even
            // if our old keyup bookkeeping still says that key is held.
            if (key < 256 && !(flags & (1LL << 30))) swallowed[key] = typingKeyUp[key] = false;
            if (manager && target && context.Get() != target) Bind(nullptr, target);
            RefreshActive(true);
        }
        void ReportCorrectionIntent(WPARAM key, bool correction) {
            if (OpenKeyCaretTone::EditKey(static_cast<unsigned>(key), 0) < 0 && OpenKeyCaretTone::EditKey(static_cast<unsigned>(key), 1) < 0) return;
            const auto current = preferences();
            if (current.caretIntent && OpenKeyCaretTone::EditKey(static_cast<unsigned>(key), current.inputType) >= 0)
                Signal(correction ? OpenKeyCaretRuntime::CaretAccepted : OpenKeyCaretRuntime::CaretRejected,
                    activeWindow, FindWindowW(L"OpenKeyVietnameseInputMethod", nullptr));
        }
        bool TestTyping(ITfContext* target, WPARAM key) {
            if (!wordTyping || !foregroundKeys || target != context.Get() || Disabled(target) ||
                !OpenKeyTsfTyping::Key(static_cast<UINT>(key)) ||
                (GetKeyState(VK_CONTROL) & 0x8000) || (GetKeyState(VK_MENU) & 0x8000)) return false;
            if (!preferences().enabled) return false;
            OpenKeyTsfTyping::Result unused;
            return typing(0, static_cast<UINT>(key), unused);
        }
        bool Type(ITfContext* target, WPARAM key) {
            if (!TestTyping(target, key)) return false;
            bool consumed = false, completed = false;
            Request(target, TF_ES_READWRITE, [&](TfEditCookie cookie) {
                std::wstring before, after; ComPtr<ITfRange> caret;
                if (!ReadContext(target, cookie, nullptr, before, after, caret)) return;
                OpenKeyTsfTyping::Result result;
                if (!typing(1, static_cast<UINT>(key), result)) return;
                if (!result.changed) { completed = true; return; }
                consumed = completed = ApplyTypingResult(target, cookie, result);
            });
            if (!completed) { OpenKeyTsfTyping::Result unused; typing(3, 0, unused); }
            return consumed;
        }
    public:
        explicit Service(Preferences (*read)() = ReadPreferences, bool word = WordHost(),
            bool (*backend)(unsigned, UINT, OpenKeyTsfTyping::Result&) = TypingRpc, bool rendererHost = RendererHost())
            : wordTyping(word), renderer(rendererHost), preferences(read), typing(backend) { ++objects; }
        ~Service() { Deactivate(); --objects; }
        HRESULT STDMETHODCALLTYPE QueryInterface(REFIID id, void** out) override {
            if (!out) return E_POINTER; *out = nullptr;
            if (id == IID_IUnknown || id == IID_ITfTextInputProcessor || id == IID_ITfTextInputProcessorEx)
                *out = static_cast<ITfTextInputProcessorEx*>(this);
            else if (id == IID_ITfKeyEventSink) *out = static_cast<ITfKeyEventSink*>(this);
            else if (id == IID_ITfThreadMgrEventSink) *out = static_cast<ITfThreadMgrEventSink*>(this);
            else if (id == IID_ITfTextEditSink) *out = static_cast<ITfTextEditSink*>(this);
            else return E_NOINTERFACE;
            AddRef(); return S_OK;
        }
        ULONG STDMETHODCALLTYPE AddRef() override { return ++references; }
        ULONG STDMETHODCALLTYPE Release() override { const auto left = --references; if (!left) delete this; return left; }
        HRESULT STDMETHODCALLTYPE Activate(ITfThreadMgr* thread, TfClientId id) override { return ActivateEx(thread, id, 0); }
        HRESULT STDMETHODCALLTYPE ActivateEx(ITfThreadMgr* thread, TfClientId id, DWORD flags) override {
            if (!thread || manager) return E_UNEXPECTED;
            secure = (flags & TF_TMAE_SECUREMODE) != 0;
            foregroundKeys = true;
            manager = thread; client = id;
            ComPtr<ITfKeystrokeMgr> keys; ComPtr<ITfSource> source;
            if (secure || FAILED(manager.As(&keys)) || FAILED(keys->AdviseKeyEventSink(client, this, TRUE)) ||
                FAILED(manager.As(&source)) || FAILED(source->AdviseSink(IID_ITfThreadMgrEventSink,
                    static_cast<ITfThreadMgrEventSink*>(this), &managerCookie))) { Deactivate(); return E_FAIL; }
            ComPtr<ITfDocumentMgr> focus; if (SUCCEEDED(manager->GetFocus(&focus))) Bind(focus.Get());
            return S_OK;
        }
        HRESULT STDMETHODCALLTYPE Deactivate() override {
            foregroundKeys = false;
            Bind(nullptr);
            if (manager) {
                ComPtr<ITfKeystrokeMgr> keys; if (SUCCEEDED(manager.As(&keys))) keys->UnadviseKeyEventSink(client);
                ComPtr<ITfSource> source;
                if (managerCookie != TF_INVALID_COOKIE && SUCCEEDED(manager.As(&source))) source->UnadviseSink(managerCookie);
            }
            managerCookie = TF_INVALID_COOKIE; manager.Reset(); client = TF_CLIENTID_NULL;
            foregroundKeys = false; ZeroMemory(swallowed, sizeof(swallowed)); ZeroMemory(typingKeyUp, sizeof(typingKeyUp)); return S_OK;
        }
        HRESULT STDMETHODCALLTYPE OnSetFocus(BOOL foreground) override {
            foregroundKeys = foreground != FALSE;
            Publish(foreground && armed); if (!foreground) ClearActive(); return S_OK;
        }
        HRESULT STDMETHODCALLTYPE OnTestKeyDown(ITfContext* target, WPARAM key, LPARAM flags, BOOL* eaten) override {
            if (!eaten) return E_POINTER;
            PrepareKeyContext(target, key, flags);
            if (wordTyping && !OpenKeyTsfTyping::Key(static_cast<UINT>(key)) && !OpenKeyCaretRuntime::Modifier(static_cast<UINT>(key))) {
                OpenKeyTsfTyping::Result unused; typing(4, 0, unused);
            }
            OpenKeyCaretTrace::Record trace;
            const bool correction = Candidate(target, key, false, &trace);
            ReportCorrectionIntent(key, correction);
            *eaten = (key < 256 && swallowed[key]) || correction || TestTyping(target, key);
            TraceKey(trace, key, *eaten != FALSE);
            return S_OK;
        }
        HRESULT STDMETHODCALLTYPE OnKeyDown(ITfContext* target, WPARAM key, LPARAM flags, BOOL* eaten) override {
            if (!eaten) return E_POINTER; *eaten = FALSE;
            PrepareKeyContext(target, key, flags);
            OpenKeyCaretTrace::Record trace; trace.kind = 1;
            if (key < 256 && swallowed[key]) { *eaten = TRUE; trace.stage = OpenKeyCaretTrace::Held; TraceKey(trace, key, true); return S_OK; }
            const bool correction = !(flags & (1LL << 30)) && Candidate(target, key, true, &trace);
            ReportCorrectionIntent(key, correction);
            if (correction) {
                if (key < 256) swallowed[key] = true; *eaten = TRUE;
            } else if (Type(target, key)) {
                if (key < 256) typingKeyUp[key] = true; *eaten = TRUE;
            } else Publish(false);
            TraceKey(trace, key, *eaten != FALSE);
            return S_OK;
        }
        HRESULT STDMETHODCALLTYPE OnTestKeyUp(ITfContext*, WPARAM key, LPARAM, BOOL* eaten) override {
            if (!eaten) return E_POINTER; *eaten = key < 256 && (swallowed[key] || typingKeyUp[key]); return S_OK;
        }
        HRESULT STDMETHODCALLTYPE OnKeyUp(ITfContext*, WPARAM key, LPARAM, BOOL* eaten) override {
            if (!eaten) return E_POINTER; *eaten = key < 256 && (swallowed[key] || typingKeyUp[key]);
            if (key < 256) swallowed[key] = typingKeyUp[key] = false; return S_OK;
        }
        HRESULT STDMETHODCALLTYPE OnPreservedKey(ITfContext*, REFGUID, BOOL* eaten) override {
            if (!eaten) return E_POINTER; *eaten = FALSE; return S_OK;
        }
        HRESULT STDMETHODCALLTYPE OnInitDocumentMgr(ITfDocumentMgr*) override { return S_OK; }
        HRESULT STDMETHODCALLTYPE OnUninitDocumentMgr(ITfDocumentMgr* document) override {
            ComPtr<ITfDocumentMgr> owner;
            if (context && SUCCEEDED(context->GetDocumentMgr(&owner)) && owner.Get() == document) Bind(nullptr);
            return S_OK;
        }
        HRESULT STDMETHODCALLTYPE OnSetFocus(ITfDocumentMgr* focus, ITfDocumentMgr*) override { Bind(focus); return S_OK; }
        HRESULT STDMETHODCALLTYPE OnPushContext(ITfContext* target) override {
            ComPtr<ITfDocumentMgr> focus, document;
            if (manager && target && SUCCEEDED(manager->GetFocus(&focus)) &&
                SUCCEEDED(target->GetDocumentMgr(&document)) && focus.Get() == document.Get()) Bind(document.Get());
            return S_OK;
        }
        HRESULT STDMETHODCALLTYPE OnPopContext(ITfContext*) override {
            ComPtr<ITfDocumentMgr> document; if (manager && SUCCEEDED(manager->GetFocus(&document))) Bind(document.Get()); return S_OK;
        }
        HRESULT STDMETHODCALLTYPE OnEndEdit(ITfContext* target, TfEditCookie cookie, ITfEditRecord* record) override {
            try {
                if (target != context.Get() || !record) return S_OK;
                if (reconverting) return S_OK; // Keep hook handoff stable during our two owned edit sessions.
                // This records word availability, not user intent. Word and
                // Chromium can emit selection-only notifications while typing;
                // only the hook's real mouse/navigation state enables an edit.
                std::wstring before, after; ComPtr<ITfRange> caret; OpenKeyCaretTone::Word word;
                Publish(!Disabled(target) && ReadContext(target, cookie, &word, before, after, caret));
            } catch (...) { Publish(false); }
            return S_OK;
        }
    };
    class Factory final : public IClassFactory {
        std::atomic<ULONG> references{1};
    public:
        Factory() { ++objects; } ~Factory() { --objects; }
        HRESULT STDMETHODCALLTYPE QueryInterface(REFIID id, void** out) override {
            if (!out) return E_POINTER; *out = nullptr;
            if (id != IID_IUnknown && id != IID_IClassFactory) return E_NOINTERFACE;
            *out = static_cast<IClassFactory*>(this); AddRef(); return S_OK;
        }
        ULONG STDMETHODCALLTYPE AddRef() override { return ++references; }
        ULONG STDMETHODCALLTYPE Release() override { const auto left = --references; if (!left) delete this; return left; }
        HRESULT STDMETHODCALLTYPE CreateInstance(IUnknown* outer, REFIID id, void** out) override {
            if (!out) return E_POINTER; *out = nullptr; if (outer) return CLASS_E_NOAGGREGATION;
            auto* service = new (std::nothrow) Service;
            if (!service) return E_OUTOFMEMORY;
            const HRESULT result = service->QueryInterface(id, out); service->Release(); return result;
        }
        HRESULT STDMETHODCALLTYPE LockServer(BOOL lock) override { if (lock) ++serverLocks; else --serverLocks; return S_OK; }
    };
}
extern "C" BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID) {
    // /MT needs CRT thread attach/detach notifications; do not disable them.
    if (reason == DLL_PROCESS_ATTACH) OpenKeyTextService::module = instance;
    return TRUE;
}
extern "C" HRESULT WINAPI DllCanUnloadNow() {
    return OpenKeyTextService::objects == 0 && OpenKeyTextService::serverLocks == 0 ? S_OK : S_FALSE;
}
extern "C" HRESULT WINAPI DllGetClassObject(REFCLSID clsid, REFIID iid, void** out) {
    if (!out) return E_POINTER; *out = nullptr;
    if (clsid != OpenKeyTextService::ServiceId) return CLASS_E_CLASSNOTAVAILABLE;
    auto* factory = new (std::nothrow) OpenKeyTextService::Factory;
    if (!factory) return E_OUTOFMEMORY;
    const auto result = factory->QueryInterface(iid, out); factory->Release(); return result;
}
