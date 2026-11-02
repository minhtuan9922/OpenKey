#pragma once
#include <windows.h>
#include <msctf.h>
#include <wrl/client.h>
#include "CaretTrace.h"
#include "../../../engine/CaretEditPlan.h"

namespace OpenKeyTextService {
    // Stable identifiers shared by both x64 and x86 registrations.
    const CLSID ServiceId = {0x727f9e3b,0x0491,0x4d5c,{0xb3,0x3d,0x64,0xd5,0x9b,0xe2,0x8a,0x70}};
    const GUID ProfileId = {0x03ed9fc5,0x956e,0x4f7e,{0xa4,0xa7,0xf4,0xa3,0x05,0x39,0x60,0xab}};
    constexpr LANGID Language = 0x042a; // Vietnamese
    constexpr wchar_t ArmedProperty[] = L"OpenKey.TSF.CaretEdit";
    constexpr wchar_t ActiveProperty[] = L"OpenKey.TSF.Active";
    struct Preferences { bool enabled = true, modern = false; int inputType = 0; bool caretIntent = false; };
    Preferences ReadPreferences();
    bool ReadContext(ITfContext* context, TfEditCookie cookie, OpenKeyCaretTone::Word* word,
        std::wstring& before, std::wstring& after, Microsoft::WRL::ComPtr<ITfRange>& caret,
        OpenKeyCaretTrace::Record* trace = nullptr, ITfComposition* ownComposition = nullptr);
    bool Apply(ITfContext* context, TfEditCookie cookie, const OpenKeyCaretTone::EditPlan& plan, ITfComposition* ownComposition = nullptr);
}
