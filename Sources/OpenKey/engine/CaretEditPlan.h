#pragma once
#include "CaretToneEdit.h"

namespace OpenKeyCaretTone {
    struct EditPlan {
        std::wstring word, replacement, removed, inserted;
        size_t caret = 0;
        int offset = 0; // Changed span start relative to the original caret.
    };
    inline bool Plan(const std::wstring& before, const std::wstring& after, int action, bool modern, EditPlan& output) {
        if (before.size() > 33 || after.size() > 33) return false;
        Word word; std::wstring replacement;
        if (!Around(before + after, before.size(), word) || !EditAction(word.text, action, modern, replacement) ||
            replacement.size() != word.text.size()) return false;
        size_t first = 0, last = word.text.size();
        while (first < last && word.text[first] == replacement[first]) ++first;
        while (last > first && word.text[last - 1] == replacement[last - 1]) --last;
        if (first == last) return false;
        output = {word.text, replacement, word.text.substr(first, last - first), replacement.substr(first, last - first),
            word.caret, static_cast<int>(first) - static_cast<int>(word.caret)};
        return true;
    }
    // Chromium commits an existing-text replacement backwards from its entry
    // caret. Carry unchanged suffix characters in the replacement too, so the
    // range ends exactly at that caret. Never attempt a forward replacement on
    // this backend: its frontend cannot preserve that range/caret contract.
    inline bool AnchorAtCaret(EditPlan& plan) {
        const int begin = static_cast<int>(plan.caret) + plan.offset;
        if (begin < 0 || plan.caret > plan.word.size() ||
            static_cast<size_t>(begin) + plan.removed.size() > plan.caret) return false;
        plan.removed = plan.word.substr(begin, plan.caret - begin);
        plan.inserted = plan.replacement.substr(begin, plan.caret - begin);
        return true;
    }
}
