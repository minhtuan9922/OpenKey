#pragma once
#include "../OpenKey/MacroService.h"

namespace OpenKeyUIMacroEditor
{
    inline bool HasUnsavedChanges(const std::vector<MacroService::Entry>& entries,
        const std::wstring& originalShortcut, const std::wstring& shortcut,
        const std::wstring& replacement)
    {
        if (originalShortcut.empty()) return !shortcut.empty() || !replacement.empty();
        for (const auto& entry : entries)
        {
            if (entry.shortcut == originalShortcut)
                return shortcut != entry.shortcut || replacement != entry.replacement;
        }
        // The original no longer exists in the snapshot; preserve the draft.
        return true;
    }
}
