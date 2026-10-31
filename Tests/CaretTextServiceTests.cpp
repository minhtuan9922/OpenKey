#define NOMINMAX
// Compile the production service with an isolated TSF context fixture. No
// registration, user registry writes, hooks or external editor interaction.
#include "../Sources/OpenKey/win32/OpenKey/OpenKey.TextService/CaretTextService.cpp"
#include "../Sources/OpenKey/win32/OpenKey/OpenKey.TextService/CaretAutomation.h"
#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <richedit.h>
#include <future>
#include <thread>
using namespace OpenKeyTextService;
using namespace OpenKeyCaretTone;
namespace {
    int checks = 0;
    void Check(bool ok, const char* label) { if (!ok) throw std::runtime_error(label); ++checks; }
    // Only unused interface methods are stubs. Read/write locks, ranges,
    // selection and notifications below model the actual TSF COM contracts.
#define COM_REFS(Type) ULONG refs = 1; ULONG STDMETHODCALLTYPE AddRef() override { return ++refs; } \
    ULONG STDMETHODCALLTYPE Release() override { auto n = --refs; if (!n) delete this; return n; } \
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID id, void** out) override { if (!out) return E_POINTER; *out = nullptr; \
        if (id != IID_IUnknown && id != __uuidof(Type)) return E_NOINTERFACE; *out = static_cast<Type*>(this); AddRef(); return S_OK; }
#define STUB(name, args) HRESULT STDMETHODCALLTYPE name args override { return E_NOTIMPL; }
    struct Document { std::wstring text; LONG caret = 0, selectedEnd = 0; bool writing = false, readonly = false;
        bool denyRead = false, denyWrite = false; int writes = 0, locks = 0, password = 0; ULONG maxRead = 0; bool denyAdvice = false;
        bool caretRelativeCommit = false; LONG changedStart = 0; std::wstring pendingText;
        int edgePolicy = 0, emptyReads = 0, invalidEndMoves = 0;
        bool composing = false, frontendComposition = false, denyComposition = false, denyAfterStart = false, failNextWrite = false;
        bool appendWithoutComposition = false, changeAfterStart = false, terminateAfterStart = false;
        int compositionsStarted = 0, compositionsEnded = 0, endFailures = 0; };
    class Scope final : public ITfInputScope {
        int kind;
    public:
        explicit Scope(int value) : kind(value) {}
        COM_REFS(ITfInputScope)
        HRESULT STDMETHODCALLTYPE GetInputScopes(InputScope** values, UINT* count) override {
            *values = static_cast<InputScope*>(CoTaskMemAlloc(sizeof(InputScope)));
            if (!*values) return E_OUTOFMEMORY;
            **values = kind == 1 ? IS_PASSWORD : IS_NUMERIC_PASSWORD; *count = 1; return S_OK;
        }
        STUB(GetPhrase, (BSTR**, UINT*))
        STUB(GetRegularExpression, (BSTR*))
        STUB(GetSRGS, (BSTR*))
        STUB(GetXML, (BSTR*))
    };
    class ScopeProperty final : public ITfProperty {
        int kind;
    public:
        explicit ScopeProperty(int value) : kind(value) {}
        COM_REFS(ITfProperty)
        HRESULT STDMETHODCALLTYPE GetValue(TfEditCookie, ITfRange*, VARIANT* value) override {
            VariantInit(value); value->vt = VT_UNKNOWN; value->punkVal = new Scope(kind); return S_OK;
        }
        STUB(GetType, (GUID*))
        STUB(EnumRanges, (TfEditCookie, IEnumTfRanges**, ITfRange*))
        STUB(GetContext, (ITfContext**))
        STUB(FindRange, (TfEditCookie, ITfRange*, ITfRange**, TfAnchor))
        STUB(SetValueStore, (TfEditCookie, ITfRange*, ITfPropertyStore*))
        STUB(SetValue, (TfEditCookie, ITfRange*, const VARIANT*))
        STUB(Clear, (TfEditCookie, ITfRange*))
    };
    class Range final : public ITfRange {
    public:
        Document& doc; LONG start, end;
        Range(Document& d, LONG a, LONG b) : doc(d), start(a), end(b) {}
        COM_REFS(ITfRange)
        HRESULT STDMETHODCALLTYPE GetText(TfEditCookie, DWORD, WCHAR* text, ULONG max, ULONG* count) override {
            if (start == end) { ++doc.emptyReads; if (doc.edgePolicy) return E_INVALIDARG; }
            doc.maxRead = (std::max)(doc.maxRead, max); *count = (std::min)(max, static_cast<ULONG>(end - start));
            std::copy_n(doc.text.data() + start, *count, text); return S_OK;
        }
        HRESULT STDMETHODCALLTYPE SetText(TfEditCookie cookie, DWORD flags, const WCHAR* text, LONG count) override {
            if (!doc.writing || cookie != 2 || doc.readonly || flags != TF_ST_CORRECTION) return E_ACCESSDENIED;
            doc.changedStart = start; doc.pendingText.assign(text, count);
            doc.text.replace(start, end - start, text, count); end = start + count; ++doc.writes; return S_OK;
        }
        HRESULT STDMETHODCALLTYPE ShiftStart(TfEditCookie, LONG requested, LONG* moved, const TF_HALTCOND*) override {
            LONG next = (std::max)(0L, (std::min)(start + requested, end)); *moved = next - start; start = next; return S_OK;
        }
        HRESULT STDMETHODCALLTYPE ShiftEnd(TfEditCookie, LONG requested, LONG* moved, const TF_HALTCOND*) override {
            LONG limit = static_cast<LONG>(doc.text.size());
            if (doc.edgePolicy >= 4) {
                const auto lineEnd = doc.text.find_first_of(L"\r\n\u2028\u2029", start);
                if (lineEnd != std::wstring::npos) limit = static_cast<LONG>(lineEnd);
            }
            if ((doc.edgePolicy == 2 || doc.edgePolicy == 4) && requested > 0 && end == limit) {
                ++doc.invalidEndMoves; return TF_E_INVALIDPOS;
            }
            LONG next = (std::max)(start, (std::min)(end + requested, limit)); *moved = next - end; end = next; return S_OK;
        }
        HRESULT STDMETHODCALLTYPE IsEmpty(TfEditCookie, BOOL* empty) override { *empty = start == end; return S_OK; }
        HRESULT STDMETHODCALLTYPE Collapse(TfEditCookie, TfAnchor anchor) override { if (anchor == TF_ANCHOR_START) end = start; else start = end; return S_OK; }
        HRESULT STDMETHODCALLTYPE Clone(ITfRange** result) override { *result = new Range(doc, start, end); return S_OK; }
        STUB(GetFormattedText, (TfEditCookie, IDataObject**))
        STUB(GetEmbedded, (TfEditCookie, REFGUID, REFIID, IUnknown**))
        STUB(InsertEmbedded, (TfEditCookie, DWORD, IDataObject*))
        STUB(ShiftStartToRange, (TfEditCookie, ITfRange*, TfAnchor))
        STUB(ShiftEndToRange, (TfEditCookie, ITfRange*, TfAnchor))
        STUB(ShiftStartRegion, (TfEditCookie, TfShiftDir, BOOL*))
        STUB(ShiftEndRegion, (TfEditCookie, TfShiftDir, BOOL*))
        STUB(IsEqualStart, (TfEditCookie, ITfRange*, TfAnchor, BOOL*))
        STUB(IsEqualEnd, (TfEditCookie, ITfRange*, TfAnchor, BOOL*))
        HRESULT STDMETHODCALLTYPE CompareStart(TfEditCookie, ITfRange* other, TfAnchor anchor, LONG* result) override {
            const auto* range = static_cast<Range*>(other);
            if (&range->doc != &doc) return E_INVALIDARG;
            const LONG position = anchor == TF_ANCHOR_START ? range->start : range->end;
            *result = start < position ? -1 : start > position ? 1 : 0; return S_OK;
        }
        HRESULT STDMETHODCALLTYPE CompareEnd(TfEditCookie, ITfRange* other, TfAnchor anchor, LONG* result) override {
            const auto* range = static_cast<Range*>(other);
            if (&range->doc != &doc) return E_INVALIDARG;
            const LONG position = anchor == TF_ANCHOR_START ? range->start : range->end;
            *result = end < position ? -1 : end > position ? 1 : 0; return S_OK;
        }
        STUB(AdjustForInsert, (TfEditCookie, ULONG, BOOL*))
        STUB(GetGravity, (TfGravity*, TfGravity*))
        STUB(SetGravity, (TfEditCookie, TfGravity, TfGravity))
        STUB(GetContext, (ITfContext**))
    };
    class Ranges final : public IEnumTfRanges {
        Document& doc; bool available;
    public:
        Ranges(Document& d, bool textChanged) : doc(d), available(textChanged) {}
        COM_REFS(IEnumTfRanges)
        HRESULT STDMETHODCALLTYPE Next(ULONG max, ITfRange** out, ULONG* fetched) override {
            *fetched = 0;
            if (max && available) { *out = new Range(doc, 0, static_cast<LONG>(doc.text.size())); *fetched = 1; available = false; return S_OK; }
            return S_FALSE;
        }
        STUB(Clone, (IEnumTfRanges**))
        STUB(Reset, ())
        STUB(Skip, (ULONG))
    };
    class Record final : public ITfEditRecord {
        Document& doc; bool textChanged;
    public:
        Record(Document& d, bool changed) : doc(d), textChanged(changed) {}
        COM_REFS(ITfEditRecord)
        HRESULT STDMETHODCALLTYPE GetSelectionStatus(BOOL* changed) override { *changed = TRUE; return S_OK; }
        HRESULT STDMETHODCALLTYPE GetTextAndPropertyUpdates(DWORD, const GUID**, ULONG, IEnumTfRanges** out) override {
            *out = new Ranges(doc, textChanged); return S_OK;
        }
    };
    class Composition final : public ITfComposition, public ITfCompositionView {
        ULONG refs = 1; Document& doc; ComPtr<ITfRange> range; ComPtr<ITfCompositionSink> sink;
    public:
        Composition(Document& value, ITfRange* span, ITfCompositionSink* events) : doc(value), range(span), sink(events) {}
        HRESULT STDMETHODCALLTYPE QueryInterface(REFIID id, void** out) override {
            *out = nullptr;
            if (id == IID_IUnknown || id == IID_ITfComposition) *out = static_cast<ITfComposition*>(this);
            else if (id == IID_ITfCompositionView) *out = static_cast<ITfCompositionView*>(this);
            else return E_NOINTERFACE;
            AddRef(); return S_OK;
        }
        ULONG STDMETHODCALLTYPE AddRef() override { return ++refs; }
        ULONG STDMETHODCALLTYPE Release() override { auto n = --refs; if (!n) delete this; return n; }
        HRESULT STDMETHODCALLTYPE GetRange(ITfRange** out) override { return range.CopyTo(out); }
        HRESULT STDMETHODCALLTYPE GetOwnerClsid(CLSID* id) override { *id = ServiceId; return S_OK; }
        HRESULT STDMETHODCALLTYPE EndComposition(TfEditCookie cookie) override {
            if (!doc.writing || cookie != 2) return TF_E_NOLOCK;
            if (doc.endFailures) { --doc.endFailures; return E_FAIL; }
            Terminate();
            return S_OK;
        }
        void Terminate() {
            if (doc.composing) {
                doc.composing = false; ++doc.compositionsEnded;
                if (sink) sink->OnCompositionTerminated(2, this);
                sink.Reset();
            }
        }
        STUB(ShiftStart, (TfEditCookie, ITfRange*))
        STUB(ShiftEnd, (TfEditCookie, ITfRange*))
    };
    class Compositions final : public IEnumITfCompositionView {
        ComPtr<ITfCompositionView> current;
    public:
        explicit Compositions(ITfCompositionView* view) : current(view) {}
        COM_REFS(IEnumITfCompositionView)
        HRESULT STDMETHODCALLTYPE Next(ULONG count, ITfCompositionView** out, ULONG* fetched) override {
            *fetched = 0;
            if (!count || !current) return S_FALSE;
            current.CopyTo(out); current.Reset(); *fetched = 1; return S_OK;
        }
        STUB(Clone, (IEnumITfCompositionView**))
        STUB(Reset, ())
        STUB(Skip, (ULONG))
    };
    class Context final : public ITfContext, public ITfSource, public ITfContextComposition {
        ULONG refs = 1;
    public:
        Document& doc; ComPtr<ITfTextEditSink> sink;
        ComPtr<Composition> active;
        explicit Context(Document& d) : doc(d) {}
        HRESULT STDMETHODCALLTYPE QueryInterface(REFIID id, void** out) override {
            *out = nullptr;
            if (id == IID_IUnknown || id == IID_ITfContext) *out = static_cast<ITfContext*>(this);
            else if (id == IID_ITfSource) *out = static_cast<ITfSource*>(this);
            else if (id == IID_ITfContextComposition) *out = static_cast<ITfContextComposition*>(this); else return E_NOINTERFACE;
            AddRef(); return S_OK;
        }
        ULONG STDMETHODCALLTYPE AddRef() override { return ++refs; }
        ULONG STDMETHODCALLTYPE Release() override { auto n = --refs; if (!n) delete this; return n; }
        HRESULT STDMETHODCALLTYPE AdviseSink(REFIID id, IUnknown* object, DWORD* cookie) override {
            if (doc.denyAdvice) { *cookie = TF_INVALID_COOKIE; return E_NOINTERFACE; }
            *cookie = 1; return id == IID_ITfTextEditSink ? object->QueryInterface(IID_PPV_ARGS(&sink)) : E_NOINTERFACE;
        }
        HRESULT STDMETHODCALLTYPE UnadviseSink(DWORD) override { sink.Reset(); return S_OK; }
        HRESULT STDMETHODCALLTYPE RequestEditSession(TfClientId, ITfEditSession* session, DWORD flags, HRESULT* performed) override {
            if (doc.failNextWrite && (flags & TF_ES_READWRITE)) { doc.failNextWrite = false; *performed = TF_E_SYNCHRONOUS; return TF_E_SYNCHRONOUS; }
            if (doc.denyRead || ((flags & TF_ES_READWRITE) && doc.denyWrite)) {
                *performed = TF_E_SYNCHRONOUS; return TF_E_SYNCHRONOUS;
            }
            doc.writing = (flags & TF_ES_READWRITE) != 0; ++doc.locks;
            const int oldWrites = doc.writes;
            const auto oldText = doc.text; const LONG oldCaret = doc.caret;
            const bool hadComposition = doc.frontendComposition;
            *performed = session->DoEditSession(doc.writing ? 2 : 1);
            doc.writing = false;
            if (doc.appendWithoutComposition && !hadComposition && doc.writes != oldWrites) {
                doc.text = oldText + doc.pendingText; doc.caret = doc.selectedEnd = oldCaret;
            } else if (doc.caretRelativeCommit && !hadComposition && doc.writes != oldWrites) {
                // A renderer commits from its frontend caret at lock ENTRY,
                // not the SetText range end, and ignores a collapsed selection
                // restored during the text-writing lock. Pure selection locks
                // do update its frontend caret. This differs from RichEdit.
                const LONG start = (std::min)(oldCaret, doc.changedStart);
                doc.text = oldText.substr(0, start) + doc.pendingText + oldText.substr(oldCaret);
                doc.caret = doc.selectedEnd = start + static_cast<LONG>(doc.pendingText.size());
            }
            if (doc.composing && !hadComposition) {
                doc.frontendComposition = true;
                if (doc.denyAfterStart) doc.failNextWrite = true;
                if (doc.changeAfterStart) doc.text = L"other text";
                if (doc.terminateAfterStart) { active->Terminate(); doc.frontendComposition = false; }
            } else if (!doc.composing) doc.frontendComposition = false;
            if (sink && doc.writes != oldWrites) {
                ComPtr<ITfEditRecord> record; record.Attach(new Record(doc, true)); sink->OnEndEdit(this, 1, record.Get());
            }
            return S_OK;
        }
        HRESULT STDMETHODCALLTYPE StartComposition(TfEditCookie cookie, ITfRange* range, ITfCompositionSink* events, ITfComposition** result) override {
            *result = nullptr;
            if (!doc.writing || cookie != 2) return TF_E_NOLOCK;
            if (!events) return E_INVALIDARG; // Matches the actual Windows RichEdit fixture.
            if (doc.denyComposition) return S_OK; // TSF contract: S_OK + null means owner refusal.
            if (doc.composing) return E_UNEXPECTED;
            active.Attach(new Composition(doc, range, events)); doc.composing = true; ++doc.compositionsStarted;
            return active->QueryInterface(IID_PPV_ARGS(result));
        }
        HRESULT STDMETHODCALLTYPE EnumCompositions(IEnumITfCompositionView** result) override {
            *result = new Compositions(doc.composing ? static_cast<ITfCompositionView*>(active.Get()) : nullptr); return S_OK;
        }
        STUB(FindComposition, (TfEditCookie, ITfRange*, IEnumITfCompositionView**))
        STUB(TakeOwnership, (TfEditCookie, ITfCompositionView*, ITfCompositionSink*, ITfComposition**))
        HRESULT STDMETHODCALLTYPE GetStatus(TF_STATUS* status) override { *status = {doc.readonly ? TF_SD_READONLY : 0u, 0}; return S_OK; }
        HRESULT STDMETHODCALLTYPE GetSelection(TfEditCookie, ULONG, ULONG count, TF_SELECTION* selection, ULONG* fetched) override {
            *fetched = count ? 1 : 0; if (count) *selection = {new Range(doc, doc.caret, doc.selectedEnd), {TF_AE_NONE, FALSE}}; return S_OK;
        }
        HRESULT STDMETHODCALLTYPE SetSelection(TfEditCookie cookie, ULONG count, const TF_SELECTION* selection) override {
            if (!doc.writing || cookie != 2 || count != 1) return E_ACCESSDENIED;
            const auto* range = static_cast<Range*>(selection->range); doc.caret = range->start; doc.selectedEnd = range->end; return S_OK;
        }
        STUB(InWriteSession, (TfClientId, BOOL*))
        STUB(GetStart, (TfEditCookie, ITfRange**))
        HRESULT STDMETHODCALLTYPE GetEnd(TfEditCookie, ITfRange** result) override {
            *result = nullptr;
            if (doc.edgePolicy == 3 || doc.edgePolicy == 5) return E_NOTIMPL;
            auto end = doc.text.size();
            if (doc.edgePolicy == 4) {
                const auto lineEnd = doc.text.find_first_of(L"\r\n\u2028\u2029", doc.caret);
                if (lineEnd != std::wstring::npos) end = lineEnd;
            }
            *result = new Range(doc, static_cast<LONG>(end), static_cast<LONG>(end)); return S_OK;
        }
        STUB(GetActiveView, (ITfContextView**))
        STUB(EnumViews, (IEnumTfContextViews**))
        HRESULT STDMETHODCALLTYPE GetProperty(REFGUID id, ITfProperty** result) override {
            *result = nullptr;
            if (id != GUID_PROP_INPUTSCOPE || !doc.password) return E_NOTIMPL;
            *result = new ScopeProperty(doc.password); return S_OK;
        }
        STUB(GetAppProperty, (REFGUID, ITfReadOnlyProperty**))
        STUB(TrackProperties, (const GUID**, ULONG, const GUID**, ULONG, ITfReadOnlyProperty**))
        STUB(EnumProperties, (IEnumTfProperties**))
        STUB(GetDocumentMgr, (ITfDocumentMgr**))
        STUB(CreateRangeBackup, (TfEditCookie, ITfRange*, ITfRangeBackup**))
    };
    class DocumentManager final : public ITfDocumentMgr {
        ComPtr<ITfContext> context;
    public:
        explicit DocumentManager(ITfContext* value) : context(value) {}
        COM_REFS(ITfDocumentMgr)
        HRESULT STDMETHODCALLTYPE GetTop(ITfContext** result) override { return context.CopyTo(result); }
        STUB(CreateContext, (TfClientId, DWORD, IUnknown*, ITfContext**, TfEditCookie*))
        STUB(Push, (ITfContext*))
        STUB(Pop, (DWORD))
        STUB(GetBase, (ITfContext**))
        STUB(EnumContexts, (IEnumTfContexts**))
    };
    Preferences settings;
    Preferences TestPreferences() { return settings; }
    void NativeFramework(bool interactive) {
        const HRESULT com = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
        Check(SUCCEEDED(com), "owned TSF fixture initializes STA");
        struct ComCleanup { ~ComCleanup() { CoUninitialize(); } } comCleanup;
        ComPtr<ITfThreadMgr> manager;
        Check(SUCCEEDED(CoCreateInstance(CLSID_TF_ThreadMgr, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&manager))), "create real TSF thread manager");
        TfClientId client = TF_CLIENTID_NULL;
        Check(SUCCEEDED(manager->Activate(&client)), "activate real TSF manager without registering any profile");
        HMODULE rich = LoadLibraryW(L"Msftedit.dll"); Check(rich != nullptr, "load owned RichEdit TSF provider");
        HWND window = CreateWindowExW(0, L"STATIC", L"OpenKey owned TSF/UIA fixture", WS_OVERLAPPEDWINDOW,
            100, 150, 600, 260, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
        HWND editor = CreateWindowExW(WS_EX_CLIENTEDGE, L"RICHEDIT50W", L"noi dung ti\u1ebfng Vi\u1ec7t",
            WS_CHILD | WS_VISIBLE | ES_MULTILINE, 20, 25, 530, 100, window, nullptr, GetModuleHandleW(nullptr), nullptr);
        Check(window && editor, "create owned interactive fixture controls");
        struct Cleanup { HWND window; HMODULE rich; ITfThreadMgr* manager;
            ~Cleanup() { DestroyWindow(window); manager->Deactivate(); FreeLibrary(rich); } } cleanup{window, rich, manager.Get()};
        SendMessageW(editor, EM_SETEDITSTYLE, SES_USECTF, SES_USECTF);
        ShowWindow(window, interactive ? SW_SHOW : SW_SHOWNOACTIVATE);
        if (interactive) SetForegroundWindow(window);
        SetFocus(editor); SendMessageW(editor, EM_SETSEL, 2, 2);
        const auto pump = [] { MSG message{}; while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) { TranslateMessage(&message); DispatchMessageW(&message); } };
        for (int i = 0; i < 30; ++i) { pump(); Sleep(10); }
        if (interactive) Check(GetForegroundWindow() == window, "owned interactive fixture must remain foreground; no focus bypass");
        ComPtr<ITfDocumentMgr> document; ComPtr<ITfContext> context;
        Check(SUCCEEDED(manager->GetFocus(&document)) && document && SUCCEEDED(document->GetTop(&context)) && context,
            "RichEdit exposes its real focused TSF text store");
        settings = {}; settings.modern = true;
        // A test client is not a registered keyboard TIP, so it must not advise
        // a foreground keystroke sink. Exercise the production range adapter
        // directly under real TSF locks; key-sink routing has a separate fixture.
        const auto modify = [&](int action) {
            bool applied = false;
            ComPtr<ITfEditSession> edit;
            edit.Attach(new EditSession([&](TfEditCookie cookie) {
                std::wstring before, after; ComPtr<ITfRange> caret; EditPlan plan;
                applied = ReadContext(context.Get(), cookie, nullptr, before, after, caret) &&
                    Plan(before, after, action, true, plan) && Apply(context.Get(), cookie, plan);
            }));
            HRESULT performed = E_FAIL;
            Check(SUCCEEDED(context->RequestEditSession(client, edit.Get(), TF_ES_READWRITE | TF_ES_SYNC, &performed)) &&
                SUCCEEDED(performed) && applied, "production adapter writes under a real TSF text-store lock");
        };
        modify(HatO); pump(); modify(5); pump();
        wchar_t text[128]{}; GetWindowTextW(editor, text, 128);
        Check(std::wstring(text) == L"n\u1ed9i dung ti\u1ebfng Vi\u1ec7t", "real text store contains the expected sentence");
        DWORD begin = 0, end = 0;
        SendMessageW(editor, EM_GETSEL, reinterpret_cast<WPARAM>(&begin), reinterpret_cast<LPARAM>(&end));
        Check(begin == 2 && end == 2, "real TSF preserves a mid-word caret");
        SetWindowTextW(editor, L"l\u00e2p"); SendMessageW(editor, EM_SETSEL, 3, 3); pump();
        ComPtr<ITfEditSession> typingEdit;
        bool typingApplied = false;
        typingEdit.Attach(new EditSession([&](TfEditCookie cookie) {
            typingApplied = ApplyTypingResult(context.Get(), cookie, {2, L"\u1eadp", true});
        }));
        HRESULT typingPerformed = E_FAIL;
        Check(SUCCEEDED(context->RequestEditSession(client, typingEdit.Get(), TF_ES_READWRITE | TF_ES_SYNC, &typingPerformed)) &&
            SUCCEEDED(typingPerformed) && typingApplied, "typing result writes under a real TSF text-store lock");
        pump(); GetWindowTextW(editor, text, 128);
        Check(std::wstring(text) == L"l\u1eadp", "real TSF store replaces hat and final consonant without duplicate hat");
        SendMessageW(editor, EM_GETSEL, reinterpret_cast<WPARAM>(&begin), reinterpret_cast<LPARAM>(&end));
        Check(begin == 3 && end == 3, "real TSF typing result leaves caret after the converted word");
        SetWindowTextW(editor, L"c\u1ed9ng"); SendMessageW(editor, EM_SETSEL, 4, 4); pump();
        ComPtr<ITfEditSession> suffixEdit; bool suffixApplied = false;
        suffixEdit.Attach(new EditSession([&](TfEditCookie cookie) {
            std::wstring before, after; ComPtr<ITfRange> caret; EditPlan plan;
            suffixApplied = ReadContext(context.Get(), cookie, nullptr, before, after, caret) &&
                Plan(before, after, 0, true, plan) && AnchorAtCaret(plan) && Apply(context.Get(), cookie, plan);
        }));
        HRESULT suffixPerformed = E_FAIL;
        Check(SUCCEEDED(context->RequestEditSession(client, suffixEdit.Get(), TF_ES_READWRITE | TF_ES_SYNC, &suffixPerformed)) &&
            SUCCEEDED(suffixPerformed) && suffixApplied, "caret-anchored suffix correction writes under a real TSF lock");
        pump(); GetWindowTextW(editor, text, 128);
        Check(std::wstring(text) == L"c\u00f4ng", "real TSF suffix correction preserves ng when clearing the tone");
        SendMessageW(editor, EM_GETSEL, reinterpret_cast<WPARAM>(&begin), reinterpret_cast<LPARAM>(&end));
        Check(begin == 4 && end == 4, "real TSF suffix correction preserves the end-of-word caret");
        SetWindowTextW(editor, L"nam"); SendMessageW(editor, EM_SETSEL, 3, 3); pump();
        ComPtr<ITfComposition> existing; ComPtr<CompositionSink> compositionSink; compositionSink.Attach(new CompositionSink);
        EditPlan reconversion; bool started = false; HRESULT startStatus = E_FAIL; int prepareStage = 0;
        ComPtr<ITfEditSession> prepare;
        prepare.Attach(new EditSession([&](TfEditCookie cookie) {
            std::wstring before, after; ComPtr<ITfRange> caret, span; ComPtr<ITfContextComposition> owner;
            prepareStage = 1; if (!ReadContext(context.Get(), cookie, nullptr, before, after, caret)) return;
            prepareStage = 2; if (!Plan(before, after, HatA, true, reconversion) || !AnchorAtCaret(reconversion)) return;
            prepareStage = 3; if (!ChangedSpan(caret.Get(), cookie, reconversion, span)) return;
            prepareStage = 4; if (FAILED(context.As(&owner))) return;
            prepareStage = 5; startStatus = owner->StartComposition(cookie, span.Get(), compositionSink.Get(), &existing);
            started = SUCCEEDED(startStatus) && existing;
        }));
        HRESULT preparePerformed = E_FAIL;
        const HRESULT prepareRequested = context->RequestEditSession(client, prepare.Get(), TF_ES_READWRITE | TF_ES_SYNC, &preparePerformed);
        if (FAILED(prepareRequested) || FAILED(preparePerformed) || !started) std::cerr << "Native composition preparation stage=" << prepareStage << " request=" << std::hex << prepareRequested << " session=" << preparePerformed << " start=" << startStatus << std::dec << '\n';
        Check(SUCCEEDED(prepareRequested) && SUCCEEDED(preparePerformed) && started, "real TSF starts a composition on existing nam text without inserting characters");
        pump(); bool reconverted = false, ended = false;
        ComPtr<ITfEditSession> commit;
        commit.Attach(new EditSession([&](TfEditCookie cookie) {
            reconverted = Apply(context.Get(), cookie, reconversion, existing.Get());
            ended = SUCCEEDED(existing->EndComposition(cookie));
        }));
        HRESULT commitPerformed = E_FAIL;
        Check(SUCCEEDED(context->RequestEditSession(client, commit.Get(), TF_ES_READWRITE | TF_ES_SYNC, &commitPerformed)) &&
            SUCCEEDED(commitPerformed) && reconverted && ended, "real TSF validates ownership and commits existing-text reconversion in a later lock");
        pump(); GetWindowTextW(editor, text, 128);
        Check(std::wstring(text) == L"n\u00e2m", "real TSF reconversion produces nam with hat without appended text");
        const auto probe = [&](bool expectReadable) {
            auto result = std::async(std::launch::async, [&] {
                const HRESULT initialized = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
                bool readable = false;
                if (SUCCEEDED(initialized)) {
                    { AutomationReader reader; AutomationContext captured; readable = reader.Read(captured, [&] { return GetForegroundWindow() == window; }); }
                    CoUninitialize();
                }
                return readable;
            });
            while (result.wait_for(std::chrono::milliseconds(0)) != std::future_status::ready) { pump(); Sleep(2); }
            Check(result.get() == expectReadable, "real UIA provider context policy agrees with the owned field");
        };
        if (interactive) {
            probe(true);
            SendMessageW(editor, EM_SETPASSWORDCHAR, L'*', 0); probe(false);
        }
    }
    void Binary(const wchar_t* path) {
        HMODULE dll = LoadLibraryW(path); Check(dll != nullptr, "load the actual production TSF DLL without registering it");
        using GetClass = HRESULT (WINAPI*)(REFCLSID, REFIID, void**);
        using CanUnload = HRESULT (WINAPI*)();
        const auto factoryEntry = reinterpret_cast<GetClass>(GetProcAddress(dll, "DllGetClassObject"));
        const auto unload = reinterpret_cast<CanUnload>(GetProcAddress(dll, "DllCanUnloadNow"));
        Check(factoryEntry && unload && GetProcAddress(dll, "DllRegisterServer") && GetProcAddress(dll, "DllUnregisterServer"), "all undecorated COM entry points exist");
        Check(unload() == S_OK, "unreferenced production module can unload");
        ComPtr<IClassFactory> factory;
        Check(factoryEntry(ServiceId, IID_PPV_ARGS(&factory)) == S_OK && unload() == S_FALSE, "production class factory holds the module alive");
        ComPtr<ITfTextInputProcessorEx> service;
        Check(factory->CreateInstance(nullptr, IID_PPV_ARGS(&service)) == S_OK, "production factory creates the TSF service");
        ComPtr<ITfKeyEventSink> keys; Check(SUCCEEDED(service.As(&keys)), "production service exposes its TSF key sink");
        Check(FAILED(service->ActivateEx(nullptr, 0, 0)), "invalid activation fails without touching editor state");
        keys.Reset(); service.Reset(); factory.Reset();
        Check(unload() == S_OK, "production COM object release permits unload");
        FreeLibrary(dll);
    }
    OpenKeyTsfTyping::Result typedResult;
    int typingRequests = 0, typingResets = 0;
    bool TestTypingRpc(unsigned part, UINT, OpenKeyTsfTyping::Result& result) {
        if (part == 0) return true;
        if (part == 3) { ++typingResets; return true; }
        if (part == 1) { ++typingRequests; result = typedResult; return true; }
        return false;
    }
    void RangeTyping() {
        settings = {}; typingRequests = typingResets = 0;
        Document doc{L"l\u00e2p", 3, 3};
        ComPtr<Context> context; context.Attach(new Context(doc));
        ComPtr<DocumentManager> document; document.Attach(new DocumentManager(context.Get()));
        auto* service = new Service(TestPreferences, true, TestTypingRpc);
        service->OnSetFocus(document.Get(), nullptr);
        typedResult = {2, L"\u1eadp", true};
        BOOL eaten = FALSE;
        service->OnTestKeyDown(context.Get(), 'J', 0, &eaten);
        Check(eaten && typingRequests == 0 && doc.writes == 0, "Word test-key does not advance the shared engine or write text");
        service->OnKeyDown(context.Get(), 'J', 0, &eaten);
        Check(eaten && doc.text == L"l\u1eadp" && doc.writes == 1 && doc.caret == 3 && typingRequests == 1,
            "Word replaces both old characters in one range write, without injected Backspace or duplicate hat");
        service->OnKeyUp(context.Get(), 'J', 0, &eaten); Check(eaten, "range typing consumes exactly the converted key's keyup");
        typedResult = {};
        service->OnKeyDown(context.Get(), 'L', 0, &eaten);
        Check(!eaten && typingRequests == 2 && doc.writes == 1, "unchanged engine result lets Word insert the original character");
        typedResult = {2, L"\u1eadp", true};
        doc.text = L"l\u00e2p"; doc.caret = doc.selectedEnd = 3; doc.denyWrite = true;
        service->OnKeyDown(context.Get(), 'J', 0, &eaten);
        Check(!eaten && typingRequests == 2 && typingResets == 1 && doc.text == L"l\u00e2p", "refused Word write lock passes the key and resets engine history");
        doc.denyWrite = false; doc.password = 1; doc.maxRead = 0;
        service->OnKeyDown(context.Get(), 'J', 0, &eaten);
        Check(!eaten && typingRequests == 2 && typingResets == 2 && doc.maxRead == 0, "password field is refused before any typing context read or engine request");
        doc.password = 0; typedResult.removed = 4;
        service->OnKeyDown(context.Get(), 'J', 0, &eaten);
        Check(!eaten && doc.writes == 1 && typingResets == 3, "engine replacement cannot delete beyond available context");
        service->Deactivate(); service->Release();
        // Lazy providers may not publish any text/selection notification after
        // focus or clicking. Read the current key context directly instead.
        for (bool noNotifications : {false, true}) {
            Document lazy{L"noi", 2, 2}; lazy.denyAdvice = noNotifications;
            ComPtr<Context> input; input.Attach(new Context(lazy));
            ComPtr<DocumentManager> owner; owner.Attach(new DocumentManager(input.Get()));
            settings.caretIntent = true;
            auto* editor = new Service(TestPreferences, false);
            editor->OnSetFocus(owner.Get(), nullptr);
            editor->OnTestKeyDown(input.Get(), 'O', 0, &eaten);
            Check(eaten && lazy.writes == 0, "cold or notification-free provider is read on the correction key");
            editor->OnKeyDown(input.Get(), 'O', 0, &eaten);
            Check(eaten && lazy.text == L"n\u00f4i" && lazy.caret == 2, "caret correction works without a preceding OnEndEdit");
            editor->Deactivate(); editor->Release();
        }
        settings = {};
        Check(objects == 0, "range typing and lazy-provider fixtures release all COM objects");
    }
    void RendererCorrections() {
        struct Case { const wchar_t* original; const wchar_t* expected; UINT key, vniKey; LONG minimumCaret; };
        const Case cases[] = {{L"c\u1ed9ng", L"c\u00f4ng", 'Z', '0', 2}, {L"c\u00f4ng", L"c\u1ed9ng", 'J', '5', 2},
            {L"b\u0103ng", L"b\u1eb1ng", 'F', '2', 2}, {L"n\u1ed9i", L"n\u1ecdi", 'O', '6', 2},
            {L"tuong", L"t\u01b0\u01a1ng", 'W', '7', 3}};
        settings = {}; settings.caretIntent = true;
        for (int mode : {0, 1}) for (bool renderer : {false, true}) for (const auto& item : cases) {
            settings.inputType = mode;
            const auto key = mode ? item.vniKey : item.key;
            for (LONG caret = 0; caret <= static_cast<LONG>(wcslen(item.original)); ++caret) {
                Document doc{std::wstring(L"abc ") + item.original + L" def", caret + 4, caret + 4};
                doc.caretRelativeCommit = renderer;
                ComPtr<Context> context; context.Attach(new Context(doc));
                ComPtr<DocumentManager> document; document.Attach(new DocumentManager(context.Get()));
                auto* service = new Service(TestPreferences, false, TypingRpc, renderer);
                service->OnSetFocus(document.Get(), nullptr);
                const bool supported = !renderer || caret >= item.minimumCaret;
                BOOL eaten = FALSE;
                service->OnTestKeyDown(context.Get(), key, 0, &eaten);
                Check((eaten != FALSE) == supported && doc.writes == 0 && doc.caret == caret + 4, "correction test-key never moves the frontend caret");
                if (!supported) {
                    Check(doc.text == std::wstring(L"abc ") + item.original + L" def", "unsupported forward renderer edit leaves document and caret untouched");
                    service->Deactivate(); service->Release(); continue;
                }
                service->OnKeyDown(context.Get(), key, 0, &eaten);
                Check(eaten && doc.text == std::wstring(L"abc ") + item.expected + L" def" && doc.writes == 1,
                    "renderer and native providers preserve unchanged suffix and surrounding words");
                Check(doc.caret == caret + 4 && doc.selectedEnd == caret + 4, "correction restores every original caret position after renderer commit");
                service->OnKeyUp(context.Get(), key, 0, &eaten);
                Check(eaten, "renderer correction consumes the paired keyup");
                service->Deactivate(); service->Release();
            }
        }
        settings = {};
        Check(objects == 0, "renderer correction fixtures release every COM session");
    }
    void EndOfTextCorrections() {
        settings = {}; settings.caretIntent = true;
        for (int policy : {0, 1, 2, 3, 4, 5}) for (int mode : {0, 1}) for (const auto* suffix :
            {L"", L".", L" ", L"\n", L"\r\n", L"\u2028", L"\u2029", L"\nnext line"}) {
            settings.inputType = mode;
            Document doc{std::wstring(L"abc nam") + suffix, 7, 7};
            doc.edgePolicy = policy; doc.caretRelativeCommit = true;
            ComPtr<Context> context; context.Attach(new Context(doc));
            ComPtr<DocumentManager> document; document.Attach(new DocumentManager(context.Get()));
            auto* service = new Service(TestPreferences, false, TypingRpc, true);
            service->OnSetFocus(document.Get(), nullptr);
            const auto key = mode ? '6' : 'A'; BOOL eaten = FALSE;
            service->OnTestKeyDown(context.Get(), key, 0, &eaten);
            Check(eaten && doc.writes == 0, "final word is readable without a following period, including strict EOF providers");
            service->OnKeyDown(context.Get(), key, 0, &eaten);
            Check(eaten && doc.text == std::wstring(L"abc n\u00e2m") + suffix && doc.writes == 1,
                "end-of-line/end-of-document nam plus a or 6 becomes nam with hat without appending a key");
            Check(doc.caret == 7 && doc.selectedEnd == 7 && doc.emptyReads == 0 && doc.invalidEndMoves == 0,
                "end-boundary correction preserves caret and never reads or moves past an empty EOF range");
            service->OnKeyUp(context.Get(), key, 0, &eaten);
            Check(eaten, "end-boundary correction consumes its keyup");
            service->Deactivate(); service->Release();
        }
        settings = {}; Check(objects == 0, "end-boundary fixtures release every COM object");
    }
    void MissingKeyUpCorrections() {
        settings = {}; settings.caretIntent = true;
        for (int mode : {0, 1, 2, 3}) for (bool testKeyFirst : {false, true}) {
            settings.inputType = mode;
            const UINT key = mode == 1 ? '6' : 'A';
            Document doc{L"nam", 3, 3}; doc.appendWithoutComposition = true;
            ComPtr<Context> context; context.Attach(new Context(doc));
            ComPtr<DocumentManager> owner; owner.Attach(new DocumentManager(context.Get()));
            auto* service = new Service(TestPreferences, false, TypingRpc, true);
            service->OnSetFocus(owner.Get(), nullptr);
            BOOL eaten = FALSE;
            for (int press = 0; press < 32; ++press) {
                // A provider/focus transition can omit KeyUp. The next physical
                // down has previous-state bit 30 clear and must start a new edit.
                if (testKeyFirst) service->OnTestKeyDown(context.Get(), key, 0, &eaten);
                service->OnKeyDown(context.Get(), key, 0, &eaten);
                Check(eaten && doc.text == (press % 2 ? L"nam" : L"n\u00e2m") && doc.writes == press + 1,
                    "fresh physical press recovers a missing keyup rather than staying swallowed forever");
                const auto text = doc.text;
                service->OnTestKeyDown(context.Get(), key, 1LL << 30, &eaten);
                Check(eaten, "held converted key is still claimed by test-key");
                service->OnKeyDown(context.Get(), key, 1LL << 30, &eaten);
                Check(eaten && doc.text == text && doc.writes == press + 1,
                    "actual held autorepeat never toggles the correction again");
            }
            service->OnKeyUp(context.Get(), key, 0, &eaten);
            Check(eaten, "recovered press retains its own paired keyup");
            service->Deactivate(); service->Release();
        }
        for (bool testKeyFirst : {false, true}) {
            settings.inputType = 0;
            Document doc{L"c\u1ed9ng", 4, 4}; doc.appendWithoutComposition = true;
            ComPtr<Context> context; context.Attach(new Context(doc));
            ComPtr<DocumentManager> owner; owner.Attach(new DocumentManager(context.Get()));
            auto* service = new Service(TestPreferences, false, TypingRpc, true);
            service->OnSetFocus(owner.Get(), nullptr);
            BOOL eaten = FALSE;
            service->OnKeyDown(context.Get(), 'Z', 0, &eaten);
            Check(eaten && doc.text == L"c\u00f4ng" && doc.caret == 4 && doc.writes == 1,
                "Chrome end-of-word clear-tone preserves ng and original caret");
            if (testKeyFirst) {
                service->OnTestKeyDown(context.Get(), 'Z', 0, &eaten);
                Check(!eaten, "fresh z with no tone to remove is not claimed by a stale swallowed flag");
            }
            service->OnKeyDown(context.Get(), 'Z', 0, &eaten);
            Check(!eaten && doc.text == L"c\u00f4ng" && doc.writes == 1,
                "fresh non-correction passes through even when the previous converted keyup was missing");
            service->OnKeyUp(context.Get(), 'Z', 0, &eaten);
            Check(!eaten, "passed-through physical key retains its normal keyup");
            service->Deactivate(); service->Release();
        }
        settings = {};
        Check(objects == 0, "missing-keyup fixtures release every composition and service");
    }
    void ExistingTextReconversion() {
        settings = {}; settings.caretIntent = true;
        // Negative control: this provider reproduces the trace's 3+0 -> 3+2
        // lengths after a successful uncomposed SetText. Text is synthetic;
        // the user's trace deliberately contains no document content.
        Document broken{L"nam", 3, 3}; broken.appendWithoutComposition = true;
        ComPtr<Context> source; source.Attach(new Context(broken));
        bool accepted = false;
        ComPtr<ITfEditSession> direct; direct.Attach(new EditSession([&](TfEditCookie cookie) {
            EditPlan plan; accepted = Plan(L"nam", L"", HatA, false, plan) && AnchorAtCaret(plan) && Apply(source.Get(), cookie, plan);
        }));
        HRESULT performed = E_FAIL;
        source->RequestEditSession(0, direct.Get(), TF_ES_SYNC | TF_ES_READWRITE, &performed);
        Check(accepted && broken.text.size() == 5 && broken.caret == 3,
            "uncomposed renderer write reproduces trace growth despite a successful SetText");
        for (int mode : {0, 1, 2, 3}) for (const auto* suffix : {L"", L".", L" ", L"\nnext line"}) {
            settings.inputType = mode;
            Document doc{std::wstring(L"nam") + suffix, 3, 3}; doc.appendWithoutComposition = true;
            ComPtr<Context> context; context.Attach(new Context(doc));
            ComPtr<DocumentManager> owner; owner.Attach(new DocumentManager(context.Get()));
            auto* service = new Service(TestPreferences, false, TypingRpc, true);
            service->OnSetFocus(owner.Get(), nullptr);
            BOOL eaten = FALSE;
            // Some hosts call KeyDown directly without TestKeyDown (as in the
            // supplied trace). This path must prepare/commit reconversion too.
            service->OnKeyDown(context.Get(), mode == 1 ? '6' : 'A', 0, &eaten);
            Check(eaten && doc.text == std::wstring(L"n\u00e2m") + suffix && doc.caret == 3 && doc.writes == 1,
                "existing-text composition replaces the final word without frontend growth or caret drift");
            Check(doc.compositionsStarted == 1 && doc.compositionsEnded == 1 && !doc.composing && !doc.frontendComposition,
                "renderer reconversion completes exactly one composition across separate locks");
            service->OnKeyUp(context.Get(), mode == 1 ? '6' : 'A', 0, &eaten);
            service->OnKeyDown(context.Get(), mode == 1 ? '6' : 'A', 0, &eaten);
            Check(eaten && doc.text == std::wstring(L"nam") + suffix && doc.caret == 3 && doc.writes == 2 &&
                doc.compositionsStarted == 2 && doc.compositionsEnded == 2,
                "the next correction still sees a three-letter word rather than growing right-hand context");
            service->OnKeyUp(context.Get(), mode == 1 ? '6' : 'A', 0, &eaten);
            service->Deactivate(); service->Release();
        }
        for (int failure : {0, 1, 2, 3, 4, 5}) {
            settings.inputType = 2; // Actual mode reported by the supplied trace.
            Document doc{L"nam", 3, 3}; doc.appendWithoutComposition = true;
            doc.denyComposition = failure == 0; doc.denyAfterStart = failure == 1;
            doc.changeAfterStart = failure == 2; doc.endFailures = failure == 3 ? 1 : failure == 4 ? 2 : 0;
            doc.terminateAfterStart = failure == 5;
            ComPtr<Context> context; context.Attach(new Context(doc));
            ComPtr<DocumentManager> owner; owner.Attach(new DocumentManager(context.Get()));
            auto* service = new Service(TestPreferences, false, TypingRpc, true);
            service->OnSetFocus(owner.Get(), nullptr);
            BOOL eaten = FALSE; service->OnKeyDown(context.Get(), 'A', 0, &eaten);
            if (failure < 3 || failure == 5) Check(!eaten && doc.writes == 0 && doc.text == (failure == 2 ? L"other text" : L"nam"),
                "rejected composition, refused second lock or stale context never overwrites existing text");
            else Check(eaten && doc.writes == 1 && doc.text == L"n\u00e2m",
                "cleanup retry never reinserts a converted physical key");
            Check(!doc.composing && !doc.frontendComposition, "failed reconversion or composition-end retry leaves no owned composition active");
            service->Deactivate(); service->Release();
        }
        settings = {};
        {
            settings.caretIntent = true;
            Document doc{L"nam", 3, 3}; ComPtr<Context> context; context.Attach(new Context(doc));
            ComPtr<CompositionSink> events; events.Attach(new CompositionSink);
            ComPtr<ITfComposition> existing;
            ComPtr<ITfEditSession> prepare; prepare.Attach(new EditSession([&](TfEditCookie cookie) {
                ComPtr<ITfRange> range; range.Attach(new Range(doc, 0, 3));
                context->StartComposition(cookie, range.Get(), events.Get(), &existing);
            }));
            HRESULT done = E_FAIL; context->RequestEditSession(0, prepare.Get(), TF_ES_SYNC | TF_ES_READWRITE, &done);
            ComPtr<DocumentManager> owner; owner.Attach(new DocumentManager(context.Get()));
            auto* service = new Service(TestPreferences, false, TypingRpc, true);
            service->OnSetFocus(owner.Get(), nullptr);
            BOOL eaten = FALSE; service->OnKeyDown(context.Get(), 'A', 0, &eaten);
            Check(!eaten && doc.writes == 0 && doc.composing && doc.maxRead == 0,
                "an existing composition not opened by this correction is neither read nor replaced or ended");
            ComPtr<ITfEditSession> cleanup; cleanup.Attach(new EditSession([&](TfEditCookie cookie) { existing->EndComposition(cookie); }));
            context->RequestEditSession(0, cleanup.Get(), TF_ES_SYNC | TF_ES_READWRITE, &done);
            service->Deactivate(); service->Release();
        }
        settings = {};
        direct.Reset(); source.Reset();
        Check(objects == 0, "reconversion and cleanup sessions release every module reference");
    }
    void Diagnostics() {
        using namespace OpenKeyCaretTrace;
        static_assert(sizeof(OpenKeyCaretTrace::Record) == 56 && sizeof(Slot) == 60, "diagnostic ABI must match x86 and x64");
        HANDLE mapping = CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0, sizeof(Buffer), Name);
        Check(mapping && GetLastError() != ERROR_ALREADY_EXISTS, "isolated diagnostic channel is not already owned by a collector");
        auto* buffer = static_cast<Buffer*>(MapViewOfFile(mapping, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(Buffer)));
        Check(buffer != nullptr, "map opt-in metadata-only diagnostic channel");
        struct Cleanup { Buffer* buffer; HANDLE mapping; ~Cleanup() { UnmapViewOfFile(buffer); CloseHandle(mapping); } } cleanup{buffer, mapping};
        ZeroMemory(buffer, sizeof(Buffer)); buffer->magic = Magic; buffer->enabled = 1;
        settings = {};
        Document doc{L"nam", 3, 3}; ComPtr<Context> context; context.Attach(new Context(doc));
        ComPtr<DocumentManager> owner; owner.Attach(new DocumentManager(context.Get()));
        auto* service = new Service(TestPreferences, false, TypingRpc, true);
        service->OnSetFocus(owner.Get(), nullptr);
        const auto latest = [&]() { return buffer->slots[static_cast<unsigned>(buffer->next) % Capacity].record; };
        BOOL eaten = FALSE;
        service->OnTestKeyDown(context.Get(), 'A', 0, &eaten);
        Check(!eaten && latest().stage == IntentMissing && latest().before == -1 && latest().after == -1,
            "trace distinguishes missing hook intent from an empty provider context");
        settings.caretIntent = true; doc.denyRead = true;
        service->OnTestKeyDown(context.Get(), 'A', 0, &eaten);
        Check(!eaten && latest().stage == ReadLock && FAILED(latest().hr), "trace records the real refused-lock HRESULT");
        doc.denyRead = false;
        service->OnTestKeyDown(context.Get(), 'A', 0, &eaten);
        auto record = latest();
        Check(eaten && record.stage == Ready && record.before == 3 && record.after == 0 && record.word == 3 &&
            (record.flags & 27) == 27 && record.kind == 0, "trace reports only lengths, flags and acceptance for an end-of-word test");
        service->OnKeyDown(context.Get(), 'A', 0, &eaten); record = latest();
        Check(eaten && record.stage == Applied && record.kind == 1 && record.eaten == 1 && doc.text == L"n\u00e2m",
            "trace separates successful write from test-key acceptance");
        const LONG before = buffer->next; buffer->enabled = 0;
        Publish(record);
        Check(buffer->next == before, "disabled collector receives no events");
        buffer->enabled = 1;
        for (unsigned i = 0; i < Capacity + 2; ++i) Publish(record);
        Check(buffer->next == before + static_cast<LONG>(Capacity) + 2 &&
            buffer->slots[static_cast<unsigned>(buffer->next) % Capacity].committed == buffer->next,
            "metadata ring stays bounded and commits complete sequence numbers when wrapping");
        service->Deactivate(); service->Release(); settings = {};
        Check(objects == 0, "diagnostic fixture releases every COM session");
    }
    void NormalTyping() {
        struct Input { const char* keys; std::vector<std::wstring> rendered; const wchar_t* unmarked; };
        const Input cases[] = {
            {"laapj", {L"l", L"la", L"l\u00e2", L"l\u00e2p", L"l\u1eadp"}, L"l\u00e2p"},
            {"coongj", {L"c", L"co", L"c\u00f4", L"c\u00f4n", L"c\u00f4ng", L"c\u1ed9ng"}, L"c\u00f4ng"},
            {"xax", {L"x", L"xa", L"x\u00e3"}, L"xa"},
            {"hoaf", {L"h", L"ho", L"hoa", L"h\u00f2a"}, L"hoa"},
            {"aapj", {L"a", L"\u00e2", L"\u00e2p", L"\u1eadp"}, L"\u00e2p"}
        };
        settings = {}; // No movement intent: all ordinary typing belongs to hook.
        for (const auto& input : cases) {
            Document doc; ComPtr<Context> context; context.Attach(new Context(doc));
            ComPtr<DocumentManager> document; document.Attach(new DocumentManager(context.Get()));
            auto* service = new Service(TestPreferences); service->OnSetFocus(document.Get(), nullptr);
            for (size_t i = 0; input.keys[i]; ++i) {
                BOOL eaten = FALSE;
                service->OnTestKeyDown(context.Get(), input.keys[i] - 'a' + 'A', 0, &eaten);
                Check(!eaten && doc.writes == 0, "ordinary Telex key is never consumed by TSF caret editing");
                doc.text = input.rendered[i]; doc.caret = doc.selectedEnd = static_cast<LONG>(doc.text.size());
                // Exercise both notifications: real text changes and the
                // selection-only callbacks that caused false arming in Word.
                for (bool changed : {true, false}) {
                    ComPtr<ITfEditRecord> record; record.Attach(new Record(doc, changed));
                    service->OnEndEdit(context.Get(), 1, record.Get());
                }
            }
            Check(doc.text == input.rendered.back() && doc.writes == 0, "selection notifications cannot mutate a normally typed word");
            settings.caretIntent = true;
            BOOL eaten = FALSE; service->OnTestKeyDown(context.Get(), 'Z', 0, &eaten);
            Check(eaten, "real movement intent enables correction of the existing word");
            service->OnKeyDown(context.Get(), 'Z', 0, &eaten);
            Check(eaten && doc.text == input.unmarked && doc.writes == 1, "tone removal still works after explicit cursor movement");
            service->OnKeyUp(context.Get(), 'Z', 0, &eaten);
            service->Deactivate(); service->Release(); settings.caretIntent = false;
        }
        Check(objects == 0, "normal typing fixtures release every service/session");
        OpenKeyCaretRuntime::Runtime runtime;
        const auto window = reinterpret_cast<HWND>(1), other = reinterpret_cast<HWND>(2);
        runtime.profile.Notify(OpenKeyCaretRuntime::Activate, window, 77);
        runtime.profile.Notify(OpenKeyCaretRuntime::Ready, window, 77);
        for (int i = 0; i < 20; ++i) {
            runtime.profile.Notify(OpenKeyCaretRuntime::Unready, window, 77);
            Check(runtime.profile.Active(window), "selection/readiness changes never clear active-profile ownership");
            runtime.profile.Notify(OpenKeyCaretRuntime::Ready, window, 77);
        }
        runtime.intent.BeginKey(VK_LEFT, true, window);
        Check(runtime.Delegate(window, 'O', 0, true), "arrow movement precedes provider selection callbacks");
        runtime.intent.PassKey('L', true);
        Check(!runtime.Delegate(window, 'A', 0, true), "ordinary text clears intent even if the provider remains ready");
        runtime.intent.Move(window);
        runtime.profile.Notify(OpenKeyCaretRuntime::Deactivate, window, 88);
        Check(runtime.profile.Active(window), "an old service cannot deactivate another owner's profile");
        runtime.profile.Notify(OpenKeyCaretRuntime::Deactivate, window, 77);
        Check(!runtime.profile.Active(window) && !runtime.Delegate(window, 'O', 0, true), "actual profile deactivation releases hook ownership");
        Check(!runtime.Delegate(other, 'O', 0, true), "foreground changes cannot inherit caret intent");
        runtime.profile.Notify(OpenKeyCaretRuntime::ActiveReady, window, 99);
        Check(runtime.profile.Active(window) && runtime.profile.ready && runtime.profile.owner == 99,
            "foreground key callback atomically claims renderer ownership and readiness");
        runtime.profile.Notify(OpenKeyCaretRuntime::ActiveUnready, window, 100);
        Check(runtime.profile.Active(window) && !runtime.profile.ready && runtime.profile.owner == 100,
            "profile remains active when a new input context has no editable word");
    }
    void Integration() {
        for (int input = 0; input <= 3; ++input) for (bool enabled : {false, true}) for (bool modern : {false, true}) {
            bool actualEnabled = !enabled, actualModern = !modern; int actualInput = -1;
            Check(OpenKeyCaretRuntime::Decode(OpenKeyCaretRuntime::Encode(enabled, input, modern), actualEnabled, actualInput, actualModern) &&
                actualEnabled == enabled && actualModern == modern && actualInput == input, "runtime IPC preserves per-application language and typing settings");
        }
        bool enabled = true, modern = false; int input = 0;
        Check(!OpenKeyCaretRuntime::Decode(0, enabled, input, modern) && !OpenKeyCaretRuntime::Decode(0x4f4b0040, enabled, input, modern),
            "unsupported or absent engine IPC is refused");
        settings = {}; settings.modern = true;
        settings.caretIntent = true;
        for (LONG caret = 0; caret <= 3; ++caret) for (int mode : {0, 1}) {
            settings.inputType = mode;
            Document doc{L"noi dung ti\u1ebfng Vi\u1ec7t", caret, caret};
            ComPtr<Context> context; context.Attach(new Context(doc));
            ComPtr<DocumentManager> document; document.Attach(new DocumentManager(context.Get()));
            auto* service = new Service(TestPreferences);
            service->OnSetFocus(document.Get(), nullptr);
            ComPtr<ITfEditRecord> record; record.Attach(new Record(doc, false));
            service->OnEndEdit(context.Get(), 1, record.Get());
            BOOL eaten = FALSE;
            const auto first = mode == 1 ? '6' : 'O', second = mode == 1 ? '5' : 'J';
            service->OnTestKeyDown(context.Get(), first, 0, &eaten);
            Check(eaten && doc.writes == 0, "test-key inspects a word without writing");
            service->OnKeyDown(context.Get(), first, 0, &eaten);
            Check(eaten && doc.writes == 1 && doc.text == L"n\u00f4i dung ti\u1ebfng Vi\u1ec7t", "TSF write session adds a hat");
            service->OnKeyDown(context.Get(), first, 1LL << 30, &eaten);
            Check(eaten && doc.writes == 1, "TSF held key never applies twice");
            service->OnKeyUp(context.Get(), first, 0, &eaten); Check(eaten, "TSF consumes matching keyup");
            service->OnTestKeyDown(context.Get(), second, 0, &eaten); Check(eaten, "TSF retains caret editing after its own change");
            service->OnKeyDown(context.Get(), second, 0, &eaten);
            Check(eaten && doc.text == L"n\u1ed9i dung ti\u1ebfng Vi\u1ec7t" && doc.writes == 2, "TSF noi + oj or 65 gives noi with hat and dot");
            Check(doc.caret == caret && doc.selectedEnd == caret, "TSF restores the exact original caret");
            Check(doc.maxRead <= 34 && !doc.writing, "TSF context reads stay bounded and locks are released");
            service->OnKeyUp(context.Get(), second, 0, &eaten);
            doc.denyWrite = true;
            service->OnKeyDown(context.Get(), first, 0, &eaten); Check(!eaten && doc.writes == 2, "denied synchronous write passes the physical key through");
            service->Deactivate(); service->Release();
            Check(!context->sink, "deactivation releases the advised text edit sink");
        }
        Document doc{L"tuong", 3, 3}; ComPtr<Context> context; context.Attach(new Context(doc));
        std::wstring before, after; ComPtr<ITfRange> caret; EditPlan plan;
        Check(ReadContext(context.Get(), 1, nullptr, before, after, caret) && Plan(before, after, HornOrBreve, true, plan), "build a two-character TSF edit plan");
        doc.writing = true;
        Check(Apply(context.Get(), 2, plan) && doc.text == L"t\u01b0\u01a1ng" && doc.caret == 3, "TSF horn pair is replaced in one range write");
        doc.writing = false;
        Check(!Apply(context.Get(), 2, plan), "stale TSF text cannot be replaced again");
        doc.readonly = true; Check(!ReadContext(context.Get(), 1, nullptr, before, after, caret), "readonly TSF context is skipped");
        doc.readonly = false; doc.selectedEnd = 4;
        Check(!ReadContext(context.Get(), 1, nullptr, before, after, caret), "noncollapsed TSF selection is skipped");
        doc.selectedEnd = doc.caret;
        for (int kind : {1, 2}) {
            doc.password = kind; doc.maxRead = 0;
            Check(!ReadContext(context.Get(), 1, nullptr, before, after, caret) && doc.maxRead == 0,
                "password and numeric-password scopes are refused before reading any text");
        }
        AutomationReader reader; AutomationContext untouched;
        Check(!reader.Read(untouched, [] { return false; }) && untouched.before.empty(), "cancel UIA read before touching desktop or initializing providers");
        Check(objects == 0, "COM service, factory and edit sessions release every module reference");
    }
}
int wmain(int argc, wchar_t** argv) {
    try { Integration(); NormalTyping(); RangeTyping(); RendererCorrections(); EndOfTextCorrections(); MissingKeyUpCorrections(); ExistingTextReconversion(); Diagnostics(); if (argc >= 2) Binary(argv[1]);
        if (argc == 3 && std::wstring(argv[2]) == L"--interactive") NativeFramework(true);
        if (argc == 3 && std::wstring(argv[2]) == L"--native-context") NativeFramework(false);
        std::cout << checks << " TSF/UIA checks passed (" << (argc == 3 ? "COM fixture plus owned native TSF context" : "isolated COM fixture")
            << "; no registration or external document access).\n"; return 0; }
    catch (const std::exception& e) { std::cerr << "FAIL: " << e.what() << '\n'; return 1; }
}
