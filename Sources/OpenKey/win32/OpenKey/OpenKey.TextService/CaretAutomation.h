#pragma once
#include <windows.h>
#include <UIAutomation.h>
#include <wrl/client.h>
#include <functional>
#include "../../../engine/CaretEditPlan.h"

namespace OpenKeyTextService {
    struct AutomationContext {
        HWND foreground = nullptr;
        DWORD process = 0;
        std::wstring before, after;
        OpenKeyCaretTone::Word word;
    };
    // Read/discovery only. TextPattern has no range SetText; actual edits use
    // the in-process TSF service, never whole-document ValuePattern.SetValue.
    // Instantiate/use/release on a dedicated windowless MTA thread.
    class AutomationReader {
        Microsoft::WRL::ComPtr<IUIAutomation> client;
        Microsoft::WRL::ComPtr<IUIAutomationCacheRequest> cache;
        const char* stage = "idle";
        bool Initialize() {
            if (client) return true;
            Microsoft::WRL::ComPtr<IUIAutomation> next;
            Microsoft::WRL::ComPtr<IUIAutomation2> limits;
            if (FAILED(CoCreateInstance(CLSID_CUIAutomation8, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&next))) ||
                FAILED(next.As(&limits)) || FAILED(limits->put_ConnectionTimeout(40)) || FAILED(limits->put_TransactionTimeout(40))) return false;
            Microsoft::WRL::ComPtr<IUIAutomationCacheRequest> request;
            if (FAILED(next->CreateCacheRequest(&request))) return false;
            for (PROPERTYID id : {UIA_IsPasswordPropertyId, UIA_IsEnabledPropertyId, UIA_HasKeyboardFocusPropertyId,
                UIA_IsKeyboardFocusablePropertyId, UIA_IsOffscreenPropertyId, UIA_ControlTypePropertyId, UIA_ProcessIdPropertyId})
                if (FAILED(request->AddProperty(id))) return false;
            client = std::move(next); cache = std::move(request); return true;
        }
        bool Collapsed(IUIAutomationTextRange* range) {
            int distance = 1;
            return range && SUCCEEDED(range->CompareEndpoints(TextPatternRangeEndpoint_Start, range, TextPatternRangeEndpoint_End, &distance)) && distance == 0;
        }
    public:
        const char* Stage() const { return stage; }
        bool Read(AutomationContext& output, const std::function<bool()>& valid) {
            using Microsoft::WRL::ComPtr;
            const ULONGLONG deadline = GetTickCount64() + 250;
            const auto alive = [&] { return valid() && GetTickCount64() < deadline; };
            stage = "initialization";
            if (!alive() || !Initialize() || !alive()) return false;
            AutomationContext next;
            next.foreground = GetForegroundWindow();
            if (!next.foreground) return false;
            // RichEdit providers can report IsPassword=false for a mask added
            // at runtime. Native control policy takes precedence when present.
            GUITHREADINFO nativeInfo{sizeof(nativeInfo)};
            if (GetGUIThreadInfo(GetWindowThreadProcessId(next.foreground, nullptr), &nativeInfo) && nativeInfo.hwndFocus) {
                wchar_t name[64]{}; GetClassNameW(nativeInfo.hwndFocus, name, 64);
                if (_wcsicmp(name, L"Edit") == 0 || _wcsnicmp(name, L"RichEdit", 8) == 0) {
                    DWORD_PTR mask = 0;
                    if ((GetWindowLongPtrW(nativeInfo.hwndFocus, GWL_STYLE) & (ES_PASSWORD | ES_READONLY)) ||
                        !SendMessageTimeoutW(nativeInfo.hwndFocus, EM_GETPASSWORDCHAR, 0, 0, SMTO_ABORTIFHUNG | SMTO_BLOCK, 10, &mask) || mask) {
                        stage = "native-policy"; return false;
                    }
                }
            }
            ComPtr<IUIAutomationElement> element;
            stage = "field-policy";
            BOOL password = TRUE, enabled = FALSE, focus = FALSE, focusable = FALSE, offscreen = TRUE;
            int type = 0, process = 0;
            if (FAILED(client->GetFocusedElementBuildCache(cache.Get(), &element)) || !element || !alive() ||
                FAILED(element->get_CachedIsPassword(&password)) || password ||
                FAILED(element->get_CachedIsEnabled(&enabled)) || !enabled ||
                FAILED(element->get_CachedHasKeyboardFocus(&focus)) || !focus ||
                FAILED(element->get_CachedIsKeyboardFocusable(&focusable)) || !focusable ||
                FAILED(element->get_CachedIsOffscreen(&offscreen)) || offscreen ||
                FAILED(element->get_CachedControlType(&type)) || (type != UIA_EditControlTypeId && type != UIA_DocumentControlTypeId) ||
                FAILED(element->get_CachedProcessId(&process)) || process <= 0) return false;
            next.process = static_cast<DWORD>(process);
            DWORD foregroundProcess = 0; GetWindowThreadProcessId(next.foreground, &foregroundProcess);
            if (next.process != foregroundProcess) {
                // Chromium renderer PIDs differ from their browser process.
                // Prove that the focused provider's HWND ancestry belongs here.
                ComPtr<IUIAutomationTreeWalker> walker; ComPtr<IUIAutomationElement> ancestor = element;
                if (FAILED(client->get_RawViewWalker(&walker))) return false;
                bool inside = false;
                for (int depth = 0; depth < 16 && ancestor && alive(); ++depth) {
                    UIA_HWND native = nullptr;
                    if (FAILED(ancestor->get_CurrentNativeWindowHandle(&native))) return false;
                    if (native && GetAncestor(static_cast<HWND>(native), GA_ROOT) == next.foreground) { inside = true; break; }
                    ComPtr<IUIAutomationElement> parent;
                    if (FAILED(walker->GetParentElement(ancestor.Get(), &parent))) return false;
                    ancestor = std::move(parent);
                }
                if (!inside) return false;
            }
            if (!alive()) return false;
            stage = "text-pattern";
            ComPtr<IUIAutomationTextPattern> pattern;
            ComPtr<IUIAutomationTextRangeArray> selected;
            ComPtr<IUIAutomationTextRange> caret;
            int count = 0;
            if (FAILED(element->GetCurrentPatternAs(UIA_TextPatternId, IID_PPV_ARGS(&pattern))) || !pattern ||
                FAILED(pattern->GetSelection(&selected)) || !selected || FAILED(selected->get_Length(&count)) || count != 1 ||
                FAILED(selected->GetElement(0, &caret)) || !Collapsed(caret.Get()) || !alive()) return false;
            stage = "readonly";
            VARIANT readonly; VariantInit(&readonly);
            const HRESULT queried = caret->GetAttributeValue(UIA_IsReadOnlyAttributeId, &readonly);
            const bool editable = SUCCEEDED(queried) && readonly.vt == VT_BOOL && readonly.boolVal == VARIANT_FALSE;
            VariantClear(&readonly);
            if (!editable || !alive()) return false;
            ComPtr<IUIAutomationTextRange> left, right;
            int moved = 0;
            stage = "bounded-context";
            if (FAILED(caret->Clone(&left)) || FAILED(caret->Clone(&right)) ||
                FAILED(left->MoveEndpointByUnit(TextPatternRangeEndpoint_Start, TextUnit_Character, -33, &moved)) ||
                FAILED(right->MoveEndpointByUnit(TextPatternRangeEndpoint_End, TextUnit_Character, 33, &moved)) || !alive()) return false;
            BSTR text = nullptr;
            if (FAILED(left->GetText(34, &text))) { SysFreeString(text); return false; }
            next.before.assign(text ? text : L"", SysStringLen(text)); SysFreeString(text); text = nullptr;
            if (FAILED(right->GetText(34, &text))) { SysFreeString(text); return false; }
            next.after.assign(text ? text : L"", SysStringLen(text)); SysFreeString(text);
            if (next.before.size() > 33 || next.after.size() > 33 || !alive() ||
                !OpenKeyCaretTone::Around(next.before + next.after, next.before.size(), next.word)) return false;
            stage = "focus-validation";
            ComPtr<IUIAutomationElement> finalFocus; BOOL same = FALSE;
            if (GetForegroundWindow() != next.foreground || FAILED(client->GetFocusedElement(&finalFocus)) ||
                FAILED(client->CompareElements(element.Get(), finalFocus.Get(), &same)) || !same || !alive()) return false;
            output = std::move(next); stage = "ready"; return true;
        }
    };
}
