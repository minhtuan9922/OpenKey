#pragma once
#include <string>
#include <vector>

// Stateless Unicode NFC editing. No typing history or dictionary is retained.
namespace OpenKeyCaretTone {
    struct Vowel { int row = -1, tone = 0; bool upper = false; };
    inline const wchar_t* Row(int row, bool upper) {
        static const wchar_t* const lower[] = {
            L"a\u00e1\u00e0\u1ea3\u00e3\u1ea1", L"\u0103\u1eaf\u1eb1\u1eb3\u1eb5\u1eb7",
            L"\u00e2\u1ea5\u1ea7\u1ea9\u1eab\u1ead", L"e\u00e9\u00e8\u1ebb\u1ebd\u1eb9",
            L"\u00ea\u1ebf\u1ec1\u1ec3\u1ec5\u1ec7", L"i\u00ed\u00ec\u1ec9\u0129\u1ecb",
            L"o\u00f3\u00f2\u1ecf\u00f5\u1ecd", L"\u00f4\u1ed1\u1ed3\u1ed5\u1ed7\u1ed9",
            L"\u01a1\u1edb\u1edd\u1edf\u1ee1\u1ee3", L"u\u00fa\u00f9\u1ee7\u0169\u1ee5",
            L"\u01b0\u1ee9\u1eeb\u1eed\u1eef\u1ef1", L"y\u00fd\u1ef3\u1ef7\u1ef9\u1ef5" };
        static const wchar_t* const capital[] = {
            L"A\u00c1\u00c0\u1ea2\u00c3\u1ea0", L"\u0102\u1eae\u1eb0\u1eb2\u1eb4\u1eb6",
            L"\u00c2\u1ea4\u1ea6\u1ea8\u1eaa\u1eac", L"E\u00c9\u00c8\u1eba\u1ebc\u1eb8",
            L"\u00ca\u1ebe\u1ec0\u1ec2\u1ec4\u1ec6", L"I\u00cd\u00cc\u1ec8\u0128\u1eca",
            L"O\u00d3\u00d2\u1ece\u00d5\u1ecc", L"\u00d4\u1ed0\u1ed2\u1ed4\u1ed6\u1ed8",
            L"\u01a0\u1eda\u1edc\u1ede\u1ee0\u1ee2", L"U\u00da\u00d9\u1ee6\u0168\u1ee4",
            L"\u01af\u1ee8\u1eea\u1eec\u1eee\u1ef0", L"Y\u00dd\u1ef2\u1ef6\u1ef8\u1ef4" };
        return upper ? capital[row] : lower[row];
    }
    inline Vowel Decode(wchar_t ch) {
        for (int row = 0; row < 12; ++row) for (int upper = 0; upper < 2; ++upper)
            for (int tone = 0; tone < 6; ++tone) if (Row(row, upper != 0)[tone] == ch) return {row, tone, upper != 0};
        return {};
    }
    inline bool Letter(wchar_t ch) {
        return (ch >= L'a' && ch <= L'z') || (ch >= L'A' && ch <= L'Z') ||
            ch == L'\u0111' || ch == L'\u0110' || Decode(ch).row >= 0;
    }
    inline bool Boundary(wchar_t ch) {
        // Unknown scripts, digits, combining marks and connectors cannot be
        // treated as word boundaries: do not edit a substring inside them.
        return ch == 0 || ch == L' ' || ch == L'\t' || ch == L'\r' || ch == L'\n' || ch == L'\u2028' || ch == L'\u2029' ||
            std::wstring(L".,;:!?()[]{}\"'/\\-\u2013\u2014\u2018\u2019\u201c\u201d").find(ch) != std::wstring::npos;
    }
    struct Word { std::wstring text; size_t start = 0, caret = 0; };
    inline bool Around(const std::wstring& context, size_t caret, Word& output) {
        if (caret > context.size()) return false;
        size_t start = caret, end = caret;
        while (start && Letter(context[start - 1])) --start;
        while (end < context.size() && Letter(context[end])) ++end;
        if (end == start || end - start > 31 ||
            (start && !Boundary(context[start - 1])) || (end < context.size() && !Boundary(context[end]))) return false;
        output = {context.substr(start, end - start), start, caret - start};
        return true;
    }
    inline int ToneKey(unsigned key, int inputType) {
        if (inputType < 0 || inputType > 3) return -1;
        const char* keys = inputType == 1 ? "012345" : "ZSFRXJ";
        for (int tone = 0; tone < 6; ++tone) if (key == static_cast<unsigned>(keys[tone])) return tone;
        return -1;
    }
    enum Shape { HatA = 6, HatE, HatO, HornOrBreve, Breve, StrokeD, HatAny, HornOnly };
    inline int EditKey(unsigned key, int inputType) {
        const int tone = ToneKey(key, inputType);
        if (tone >= 0 || inputType < 0 || inputType > 3) return tone;
        if (inputType == 1) {
            switch (key) { case '6': return HatAny; case '7': return HornOnly; case '8': return Breve; case '9': return StrokeD; }
        } else {
            switch (key) { case 'A': return HatA; case 'E': return HatE; case 'O': return HatO; case 'W': return HornOrBreve; case 'D': return StrokeD; }
        }
        return -1;
    }
    inline bool Edit(const std::wstring& word, int tone, bool modern, std::wstring& output) {
        if (word.empty() || word.size() > 31 || tone < 0 || tone > 5) return false;
        std::vector<Vowel> vowels;
        std::wstring base;
        int first = -1, last = -1, marked = -1;
        for (size_t i = 0; i < word.size(); ++i) {
            if (!Letter(word[i])) return false;
            const auto v = Decode(word[i]); vowels.push_back(v);
            base += v.row >= 0 ? L"aaaeeiooouuy"[v.row] :
                word[i] == L'\u0110' ? L'\u0111' : word[i] >= L'A' && word[i] <= L'Z' ? word[i] + 32 : word[i];
            if (v.tone) { if (marked >= 0) return false; marked = static_cast<int>(i); }
        }
        for (int i = 0; i < static_cast<int>(word.size()); ++i) if (vowels[i].row >= 0) {
            // 'qu' and 'gi' belong to the onset when followed by a vowel.
            if (i == 1 && i + 1 < static_cast<int>(word.size()) && vowels[i + 1].row >= 0 &&
                ((base[0] == L'q' && base[1] == L'u') || (base[0] == L'g' && base[1] == L'i'))) continue;
            if (last >= 0 && last + 1 != i) return false;
            if (first < 0) first = i;
            last = i;
        }
        if (first < 0 || last - first > 2) return false;
        const std::wstring onset = base.substr(0, first), coda = base.substr(last + 1);
        const std::wstring onsets = L"|b|c|ch|d|\u0111|g|gh|gi|h|k|kh|l|m|n|ng|ngh|nh|p|ph|q|qu|r|s|t|th|tr|v|x|";
        const std::wstring codas = L"|c|ch|m|n|ng|nh|p|t|";
        if (onsets.find(L"|" + onset + L"|") == std::wstring::npos && !onset.empty()) return false;
        if (codas.find(L"|" + coda + L"|") == std::wstring::npos && !coda.empty()) return false;
        if (tone && tone != 1 && tone != 5 && (coda == L"c" || coda == L"ch" || coda == L"p" || coda == L"t")) return false;
        int target = marked;
        if (target < 0) {
            target = first;
            const auto nucleus = base.substr(first, last - first + 1);
            if (last - first == 2) target = first + 1;
            else if (last > first) {
                if (modern) {
                    if (nucleus == L"oa" || nucleus == L"oe" || nucleus == L"uy" || nucleus == L"uo" ||
                        (!coda.empty() && nucleus != L"ai" && nucleus != L"oi" && nucleus != L"ui" && nucleus != L"ay" && nucleus != L"au" && nucleus != L"eu" && nucleus != L"iu" && nucleus != L"uu" && nucleus != L"ua")) target = last;
                } else if (!coda.empty()) target = last;
            }
            // Circumflex e and horn o take the tone in both orthographies.
            for (int i = first; i <= last; ++i) if (vowels[i].row == 4 || vowels[i].row == 8) target = i;
            if (vowels[last].row == 1 || vowels[last].row == 2 || vowels[last].row == 7) target = last;
        }
        if (target < first || target > last) return false;
        // Repeating the same tone removes it; z / VNI 0 explicitly clears it.
        const int applied = vowels[target].tone == tone ? 0 : tone;
        output = word;
        output[target] = Row(vowels[target].row, vowels[target].upper)[applied];
        return output != word;
    }
    inline bool EditAction(const std::wstring& word, int action, bool modern, std::wstring& output) {
        if (action >= 0 && action <= 5) return Edit(word, action, modern, output);
        if (action < HatA || action > HornOnly || word.empty() || word.size() > 31) return false;
        std::wstring checked;
        const bool singleD = word == L"d" || word == L"D" || word == L"\u0111" || word == L"\u0110";
        if (!Edit(word, 1, modern, checked) && !(action == StrokeD && singleD)) return false;
        output = word;
        int oldTone = 0;
        std::vector<Vowel> vowels;
        for (wchar_t ch : word) {
            const auto v = Decode(ch); vowels.push_back(v);
            if (v.tone) oldTone = v.tone;
        }
        const auto setRow = [&](size_t index, int row) { output[index] = Row(row, vowels[index].upper)[vowels[index].tone]; };
        const auto isA = [](int row) { return row >= 0 && row <= 2; };
        const auto isO = [](int row) { return row >= 6 && row <= 8; };
        if (action == StrokeD) {
            const auto i = word.find_first_of(L"dD\u0111\u0110");
            if (i == std::wstring::npos) return false;
            switch (word[i]) {
                case L'd': output[i] = L'\u0111'; break; case L'D': output[i] = L'\u0110'; break;
                case L'\u0111': output[i] = L'd'; break; case L'\u0110': output[i] = L'D'; break;
            }
        } else if (action == HatA || action == HatE || action == HatO || action == HatAny || action == Breve) {
            int target = -1, plain = -1, shaped = -1;
            for (int i = static_cast<int>(word.size()) - 1; i >= 0; --i) {
                const int row = vowels[i].row;
                if ((action == HatA || action == HatAny || action == Breve) && isA(row)) { plain = 0; shaped = action == Breve ? 1 : 2; }
                else if ((action == HatE || action == HatAny) && (row == 3 || row == 4)) { plain = 3; shaped = 4; }
                else if ((action == HatO || action == HatAny) && isO(row)) { plain = 6; shaped = 7; }
                else continue;
                target = i; break;
            }
            if (target < 0) return false;
            setRow(target, vowels[target].row == shaped ? plain : shaped);
        } else {
            std::vector<size_t> nucleus;
            for (size_t i = 0; i < word.size(); ++i) if (vowels[i].row >= 0) {
                const wchar_t onset = i ? word[i - 1] : 0;
                if (i == 1 && i + 1 < word.size() && vowels[i + 1].row >= 0 &&
                    (((onset == L'q' || onset == L'Q') && vowels[i].row == 9) ||
                     ((onset == L'g' || onset == L'G') && vowels[i].row == 5))) continue;
                nucleus.push_back(i);
            }
            if (nucleus.empty()) return false;
            const size_t first = nucleus[0];
            std::vector<size_t> targets;
            if (nucleus.size() == 1) targets.push_back(first);
            else {
                const size_t second = nucleus[1];
                const int a = vowels[first].row, b = vowels[second].row;
                if ((a == 9 || a == 10) && isO(b)) {
                    const bool bareThuo = first == 2 && (word[0] == L't' || word[0] == L'T') &&
                        (word[1] == L'h' || word[1] == L'H') && second + 1 == word.size();
                    if (!bareThuo || (a == 10 && b == 8)) targets.push_back(first);
                    targets.push_back(second);
                }
                else if (((a == 9 || a == 10) && (isA(b) || b == 5 || b == 9 || b == 10)) || (isO(a) && b == 5)) targets.push_back(first);
                else if ((a == 5 && isO(b)) || (isO(a) && isA(b))) targets.push_back(second);
                else return false;
            }
            bool remove = true;
            for (size_t i : targets) {
                const int row = vowels[i].row;
                if ((!isA(row) && !isO(row) && row != 9 && row != 10) || (action == HornOnly && isA(row))) return false;
                if (row != 1 && row != 8 && row != 10) remove = false;
            }
            for (size_t i : targets) {
                const int row = vowels[i].row;
                setRow(i, isA(row) ? (remove ? 0 : 1) : isO(row) ? (remove ? 6 : 8) : (remove ? 9 : 10));
            }
        }
        if (output == word) return false;
        if (oldTone && action != StrokeD) {
            // A new hat/horn can move the proper tone position in a diphthong.
            // Reapply the original tone using the configured orthography.
            for (auto& ch : output) { const auto v = Decode(ch); if (v.row >= 0) ch = Row(v.row, v.upper)[0]; }
            std::wstring toned;
            if (!Edit(output, oldTone, modern, toned)) return false;
            output = std::move(toned);
        }
        return output != word;
    }
}
