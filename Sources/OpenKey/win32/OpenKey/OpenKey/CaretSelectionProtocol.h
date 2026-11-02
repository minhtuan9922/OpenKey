#pragma once
#include <windows.h>

namespace OpenKeyCaretSelection {
    inline UINT Message(bool selected) {
        static const UINT active = RegisterWindowMessageW(L"OpenKey.CaretSelection.Active.v1");
        static const UINT inactive = RegisterWindowMessageW(L"OpenKey.CaretSelection.Inactive.v1");
        return selected ? active : inactive;
    }
    inline bool OnUserDesktop() {
        HDESK input = OpenInputDesktop(0, FALSE, DESKTOP_READOBJECTS);
        if (!input) return false;
        wchar_t inputName[256]{}, threadName[256]{}; DWORD bytes = 0;
        const bool matches = GetUserObjectInformationW(input, UOI_NAME, inputName, sizeof(inputName), &bytes) &&
            GetUserObjectInformationW(GetThreadDesktop(GetCurrentThreadId()), UOI_NAME, threadName, sizeof(threadName), &bytes) &&
            _wcsicmp(inputName, threadName) == 0;
        CloseDesktop(input); return matches;
    }
}
