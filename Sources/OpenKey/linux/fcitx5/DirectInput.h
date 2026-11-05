#pragma once

#include "Composition.h"
#include <fcitx-utils/utf8.h>
#include <fcitx/surroundingtext.h>
#include <algorithm>
#include <deque>
#include <optional>

namespace openkey {

inline std::u32string codepoints(const std::string& text) {
    std::u32string result;
    for (auto it = text.begin(); it != text.end();) {
        uint32_t value = 0;
        it = fcitx::utf8::getNextChar(it, text.end(), &value);
        if (!fcitx::utf8::isValidChar(value)) throw std::invalid_argument("Invalid UTF-8");
        result.push_back(value);
    }
    return result;
}

inline std::string utf8(const std::u32string& text) {
    std::string result;
    for (auto ch : text) result += fcitx::utf8::UCS4ToUTF8(ch);
    return result;
}

// tone: 0 none, 1 sắc, 2 huyền, 3 hỏi, 4 ngã, 5 nặng. -1 when ch is not a vowel.
inline int vietTone(char32_t ch, char32_t& base) {
    struct Row { char32_t composed; char32_t stem; int tone; };
    static constexpr Row rows[] = {
        {U'a', U'a', 0}, {U'à', U'a', 2}, {U'á', U'a', 1}, {U'ả', U'a', 3}, {U'ã', U'a', 4}, {U'ạ', U'a', 5},
        {U'ă', U'ă', 0}, {U'ằ', U'ă', 2}, {U'ắ', U'ă', 1}, {U'ẳ', U'ă', 3}, {U'ẵ', U'ă', 4}, {U'ặ', U'ă', 5},
        {U'â', U'â', 0}, {U'ầ', U'â', 2}, {U'ấ', U'â', 1}, {U'ẩ', U'â', 3}, {U'ẫ', U'â', 4}, {U'ậ', U'â', 5},
        {U'e', U'e', 0}, {U'è', U'e', 2}, {U'é', U'e', 1}, {U'ẻ', U'e', 3}, {U'ẽ', U'e', 4}, {U'ẹ', U'e', 5},
        {U'ê', U'ê', 0}, {U'ề', U'ê', 2}, {U'ế', U'ê', 1}, {U'ể', U'ê', 3}, {U'ễ', U'ê', 4}, {U'ệ', U'ê', 5},
        {U'i', U'i', 0}, {U'ì', U'i', 2}, {U'í', U'i', 1}, {U'ỉ', U'i', 3}, {U'ĩ', U'i', 4}, {U'ị', U'i', 5},
        {U'o', U'o', 0}, {U'ò', U'o', 2}, {U'ó', U'o', 1}, {U'ỏ', U'o', 3}, {U'õ', U'o', 4}, {U'ọ', U'o', 5},
        {U'ô', U'ô', 0}, {U'ồ', U'ô', 2}, {U'ố', U'ô', 1}, {U'ổ', U'ô', 3}, {U'ỗ', U'ô', 4}, {U'ộ', U'ô', 5},
        {U'ơ', U'ơ', 0}, {U'ờ', U'ơ', 2}, {U'ớ', U'ơ', 1}, {U'ở', U'ơ', 3}, {U'ỡ', U'ơ', 4}, {U'ợ', U'ơ', 5},
        {U'u', U'u', 0}, {U'ù', U'u', 2}, {U'ú', U'u', 1}, {U'ủ', U'u', 3}, {U'ũ', U'u', 4}, {U'ụ', U'u', 5},
        {U'ư', U'ư', 0}, {U'ừ', U'ư', 2}, {U'ứ', U'ư', 1}, {U'ử', U'ư', 3}, {U'ữ', U'ư', 4}, {U'ự', U'ư', 5},
        {U'y', U'y', 0}, {U'ỳ', U'y', 2}, {U'ý', U'y', 1}, {U'ỷ', U'y', 3}, {U'ỹ', U'y', 4}, {U'ỵ', U'y', 5},
        {U'A', U'A', 0}, {U'À', U'A', 2}, {U'Á', U'A', 1}, {U'Ả', U'A', 3}, {U'Ã', U'A', 4}, {U'Ạ', U'A', 5},
        {U'Ă', U'Ă', 0}, {U'Ằ', U'Ă', 2}, {U'Ắ', U'Ă', 1}, {U'Ẳ', U'Ă', 3}, {U'Ẵ', U'Ă', 4}, {U'Ặ', U'Ă', 5},
        {U'Â', U'Â', 0}, {U'Ầ', U'Â', 2}, {U'Ấ', U'Â', 1}, {U'Ẩ', U'Â', 3}, {U'Ẫ', U'Â', 4}, {U'Ậ', U'Â', 5},
        {U'E', U'E', 0}, {U'È', U'E', 2}, {U'É', U'E', 1}, {U'Ẻ', U'E', 3}, {U'Ẽ', U'E', 4}, {U'Ẹ', U'E', 5},
        {U'Ê', U'Ê', 0}, {U'Ề', U'Ê', 2}, {U'Ế', U'Ê', 1}, {U'Ể', U'Ê', 3}, {U'Ễ', U'Ê', 4}, {U'Ệ', U'Ê', 5},
        {U'I', U'I', 0}, {U'Ì', U'I', 2}, {U'Í', U'I', 1}, {U'Ỉ', U'I', 3}, {U'Ĩ', U'I', 4}, {U'Ị', U'I', 5},
        {U'O', U'O', 0}, {U'Ò', U'O', 2}, {U'Ó', U'O', 1}, {U'Ỏ', U'O', 3}, {U'Õ', U'O', 4}, {U'Ọ', U'O', 5},
        {U'Ô', U'Ô', 0}, {U'Ồ', U'Ô', 2}, {U'Ố', U'Ô', 1}, {U'Ổ', U'Ô', 3}, {U'Ỗ', U'Ô', 4}, {U'Ộ', U'Ô', 5},
        {U'Ơ', U'Ơ', 0}, {U'Ờ', U'Ơ', 2}, {U'Ớ', U'Ơ', 1}, {U'Ở', U'Ơ', 3}, {U'Ỡ', U'Ơ', 4}, {U'Ợ', U'Ơ', 5},
        {U'U', U'U', 0}, {U'Ù', U'U', 2}, {U'Ú', U'U', 1}, {U'Ủ', U'U', 3}, {U'Ũ', U'U', 4}, {U'Ụ', U'U', 5},
        {U'Ư', U'Ư', 0}, {U'Ừ', U'Ư', 2}, {U'Ứ', U'Ư', 1}, {U'Ử', U'Ư', 3}, {U'Ữ', U'Ư', 4}, {U'Ự', U'Ư', 5},
        {U'Y', U'Y', 0}, {U'Ỳ', U'Y', 2}, {U'Ý', U'Y', 1}, {U'Ỷ', U'Y', 3}, {U'Ỹ', U'Y', 4}, {U'Ỵ', U'Y', 5},
    };
    for (const auto& row : rows) {
        if (row.composed == ch) {
            base = row.stem;
            return row.tone;
        }
    }
    return -1;
}

inline char32_t vietCompose(char32_t stem, int tone) {
    char32_t base = 0;
    if (vietTone(stem, base) < 0) return stem;
    struct Row { char32_t composed; char32_t stem; int tone; };
    static constexpr Row rows[] = {
        {U'a', U'a', 0}, {U'à', U'a', 2}, {U'á', U'a', 1}, {U'ả', U'a', 3}, {U'ã', U'a', 4}, {U'ạ', U'a', 5},
        {U'ă', U'ă', 0}, {U'ằ', U'ă', 2}, {U'ắ', U'ă', 1}, {U'ẳ', U'ă', 3}, {U'ẵ', U'ă', 4}, {U'ặ', U'ă', 5},
        {U'â', U'â', 0}, {U'ầ', U'â', 2}, {U'ấ', U'â', 1}, {U'ẩ', U'â', 3}, {U'ẫ', U'â', 4}, {U'ậ', U'â', 5},
        {U'e', U'e', 0}, {U'è', U'e', 2}, {U'é', U'e', 1}, {U'ẻ', U'e', 3}, {U'ẽ', U'e', 4}, {U'ẹ', U'e', 5},
        {U'ê', U'ê', 0}, {U'ề', U'ê', 2}, {U'ế', U'ê', 1}, {U'ể', U'ê', 3}, {U'ễ', U'ê', 4}, {U'ệ', U'ê', 5},
        {U'i', U'i', 0}, {U'ì', U'i', 2}, {U'í', U'i', 1}, {U'ỉ', U'i', 3}, {U'ĩ', U'i', 4}, {U'ị', U'i', 5},
        {U'o', U'o', 0}, {U'ò', U'o', 2}, {U'ó', U'o', 1}, {U'ỏ', U'o', 3}, {U'õ', U'o', 4}, {U'ọ', U'o', 5},
        {U'ô', U'ô', 0}, {U'ồ', U'ô', 2}, {U'ố', U'ô', 1}, {U'ổ', U'ô', 3}, {U'ỗ', U'ô', 4}, {U'ộ', U'ô', 5},
        {U'ơ', U'ơ', 0}, {U'ờ', U'ơ', 2}, {U'ớ', U'ơ', 1}, {U'ở', U'ơ', 3}, {U'ỡ', U'ơ', 4}, {U'ợ', U'ơ', 5},
        {U'u', U'u', 0}, {U'ù', U'u', 2}, {U'ú', U'u', 1}, {U'ủ', U'u', 3}, {U'ũ', U'u', 4}, {U'ụ', U'u', 5},
        {U'ư', U'ư', 0}, {U'ừ', U'ư', 2}, {U'ứ', U'ư', 1}, {U'ử', U'ư', 3}, {U'ữ', U'ư', 4}, {U'ự', U'ư', 5},
        {U'y', U'y', 0}, {U'ỳ', U'y', 2}, {U'ý', U'y', 1}, {U'ỷ', U'y', 3}, {U'ỹ', U'y', 4}, {U'ỵ', U'y', 5},
        {U'A', U'A', 0}, {U'À', U'A', 2}, {U'Á', U'A', 1}, {U'Ả', U'A', 3}, {U'Ã', U'A', 4}, {U'Ạ', U'A', 5},
        {U'Ă', U'Ă', 0}, {U'Ằ', U'Ă', 2}, {U'Ắ', U'Ă', 1}, {U'Ẳ', U'Ă', 3}, {U'Ẵ', U'Ă', 4}, {U'Ặ', U'Ă', 5},
        {U'Â', U'Â', 0}, {U'Ầ', U'Â', 2}, {U'Ấ', U'Â', 1}, {U'Ẩ', U'Â', 3}, {U'Ẫ', U'Â', 4}, {U'Ậ', U'Â', 5},
        {U'E', U'E', 0}, {U'È', U'E', 2}, {U'É', U'E', 1}, {U'Ẻ', U'E', 3}, {U'Ẽ', U'E', 4}, {U'Ẹ', U'E', 5},
        {U'Ê', U'Ê', 0}, {U'Ề', U'Ê', 2}, {U'Ế', U'Ê', 1}, {U'Ể', U'Ê', 3}, {U'Ễ', U'Ê', 4}, {U'Ệ', U'Ê', 5},
        {U'I', U'I', 0}, {U'Ì', U'I', 2}, {U'Í', U'I', 1}, {U'Ỉ', U'I', 3}, {U'Ĩ', U'I', 4}, {U'Ị', U'I', 5},
        {U'O', U'O', 0}, {U'Ò', U'O', 2}, {U'Ó', U'O', 1}, {U'Ỏ', U'O', 3}, {U'Õ', U'O', 4}, {U'Ọ', U'O', 5},
        {U'Ô', U'Ô', 0}, {U'Ồ', U'Ô', 2}, {U'Ố', U'Ô', 1}, {U'Ổ', U'Ô', 3}, {U'Ỗ', U'Ô', 4}, {U'Ộ', U'Ô', 5},
        {U'Ơ', U'Ơ', 0}, {U'Ờ', U'Ơ', 2}, {U'Ớ', U'Ơ', 1}, {U'Ở', U'Ơ', 3}, {U'Ỡ', U'Ơ', 4}, {U'Ợ', U'Ơ', 5},
        {U'U', U'U', 0}, {U'Ù', U'U', 2}, {U'Ú', U'U', 1}, {U'Ủ', U'U', 3}, {U'Ũ', U'U', 4}, {U'Ụ', U'U', 5},
        {U'Ư', U'Ư', 0}, {U'Ừ', U'Ư', 2}, {U'Ứ', U'Ư', 1}, {U'Ử', U'Ư', 3}, {U'Ữ', U'Ư', 4}, {U'Ự', U'Ư', 5},
        {U'Y', U'Y', 0}, {U'Ỳ', U'Y', 2}, {U'Ý', U'Y', 1}, {U'Ỷ', U'Y', 3}, {U'Ỹ', U'Y', 4}, {U'Ỵ', U'Y', 5},
    };
    for (const auto& row : rows)
        if (row.stem == base && row.tone == tone) return row.composed;
    return stem;
}

inline char32_t vietMark(int tone) {
    switch (tone) {
    case 1: return 0x0301;
    case 2: return 0x0300;
    case 3: return 0x0309;
    case 4: return 0x0303;
    case 5: return 0x0323;
    default: return 0;
    }
}

inline int vietMarkTone(char32_t ch) {
    switch (ch) {
    case 0x0301: return 1;
    case 0x0300: return 2;
    case 0x0309: return 3;
    case 0x0303: return 4;
    case 0x0323: return 5;
    default: return 0;
    }
}

// Telex base letter of a vowel stem: ô/ơ → o, â/ă → a, ê → e, ư → u.
inline char32_t vietAscii(char32_t stem) {
    switch (stem) {
    case U'a': case U'ă': case U'â': return U'a';
    case U'A': case U'Ă': case U'Â': return U'A';
    case U'e': case U'ê': return U'e';
    case U'E': case U'Ê': return U'E';
    case U'i': return U'i';
    case U'I': return U'I';
    case U'o': case U'ô': case U'ơ': return U'o';
    case U'O': case U'Ô': case U'Ơ': return U'O';
    case U'u': case U'ư': return U'u';
    case U'U': case U'Ư': return U'U';
    case U'y': return U'y';
    case U'Y': return U'Y';
    default: return 0;
    }
}

// Circumflex U+0302, breve U+0306, horn U+031B. Zero when the stem has no shape.
inline char32_t vietShapeMark(char32_t stem) {
    switch (stem) {
    case U'â': case U'Â': case U'ê': case U'Ê': case U'ô': case U'Ô': return 0x0302;
    case U'ă': case U'Ă': return 0x0306;
    case U'ơ': case U'Ơ': case U'ư': case U'Ư': return 0x031B;
    default: return 0;
    }
}

inline char32_t vietApplyShape(char32_t ascii, char32_t mark) {
    const bool upper = ascii >= U'A' && ascii <= U'Z';
    if (mark == 0x0302) {
        if (ascii == U'a' || ascii == U'A') return upper ? U'Â' : U'â';
        if (ascii == U'e' || ascii == U'E') return upper ? U'Ê' : U'ê';
        if (ascii == U'o' || ascii == U'O') return upper ? U'Ô' : U'ô';
    } else if (mark == 0x0306) {
        if (ascii == U'a' || ascii == U'A') return upper ? U'Ă' : U'ă';
    } else if (mark == 0x031B) {
        if (ascii == U'o' || ascii == U'O') return upper ? U'Ơ' : U'ơ';
        if (ascii == U'u' || ascii == U'U') return upper ? U'Ư' : U'ư';
    }
    return 0;
}

// One visible letter. A base vowel plus its shape and tone marks is one cluster,
// so o + U+0302 compares equal to ô.
struct VietCluster { char32_t nfc = 0; int tone = -1; std::size_t count = 1; };

inline VietCluster readVietCluster(const std::u32string& text, std::size_t i) {
    VietCluster out;
    if (i >= text.size()) return out;
    out.nfc = text[i];
    char32_t stem = 0;
    const int tone = vietTone(text[i], stem);
    if (tone < 0) return out;
    if (tone > 0) {
        out.tone = tone;
        return out;
    }
    std::size_t count = 1;
    char32_t shaped = stem;
    if (i + 1 < text.size() && vietAscii(stem) == stem) {
        const auto applied = vietApplyShape(stem, text[i + 1]);
        if (applied) {
            shaped = applied;
            count = 2;
        }
    }
    int shapedTone = 0;
    if (i + count < text.size()) {
        const int mark = vietMarkTone(text[i + count]);
        if (mark) {
            shaped = vietCompose(shaped, mark);
            shapedTone = mark;
            ++count;
        }
    }
    out.nfc = shaped;
    out.tone = shapedTone;
    out.count = count;
    return out;
}

inline std::string nfcVietnamese(const std::string& text) {
    const auto value = codepoints(text);
    std::u32string out;
    for (std::size_t i = 0; i < value.size();) {
        const auto cluster = readVietCluster(value, i);
        out.push_back(cluster.nfc);
        i += cluster.count ? cluster.count : 1;
    }
    return utf8(out);
}

struct Snapshot {
    std::u32string text;
    unsigned cursor = 0;
    unsigned anchor = 0;
    bool operator==(const Snapshot& other) const {
        return cursor == other.cursor && anchor == other.anchor && text == other.text;
    }
    static std::optional<Snapshot> read(const fcitx::SurroundingText& source, bool allowSelection = false) {
        if (!source.isValid() || (!allowSelection && source.cursor() != source.anchor()) ||
            source.text().size() > 65536) return std::nullopt;
        return Snapshot{codepoints(source.text()), source.cursor(), source.anchor()};
    }
};

// A single serialized edit is outstanding at a time. Never rewrite the
// client's surrounding-text cache with our prediction and mistake it for ACK.
class DirectInput {
public:
    struct Edit { unsigned removed; std::string inserted; };
    struct Comparison {
        const char* reference = "none";
        bool comparable = false;
        bool textChanged = false, cursorChanged = false, anchorChanged = false;
    };
    enum class Observation { Ready, Waiting, Changed };
    bool pending() const { return expected_.has_value(); }
    bool requiresFreshUpdate() const { return requireFreshUpdate_; }
    // Pre-image while an edit is outstanding; the predicted text is expected().
    const Snapshot* confirmed() const { return confirmed_ ? &*confirmed_ : nullptr; }

    void clear() {
        expected_.reset(); confirmed_.reset(); echoes_.clear();
        requireFreshUpdate_ = false; allowStaleCache_ = false;
    }

    // Boundary/shortcut/Enter keys stay unaccepted (native Wayland delivery).
    // Treat the in-flight commitString/deleteSurroundingText as done so queued
    // composing keys can flush even when the client's surrounding-text cache lags.
    void assumeAcked() {
        if (!expected_) return;
        confirmed_ = expected_;
        expected_.reset();
        requireFreshUpdate_ = false;
        allowStaleCache_ = true;
    }

    Comparison compare(const std::optional<Snapshot>& snapshot) const {
        Comparison result;
        const auto& reference = expected_ ? expected_ : confirmed_;
        if (!reference) return result;
        result.reference = expected_ ? "expected" : "confirmed";
        if (!snapshot) return result;
        result.comparable = true;
        result.textChanged = snapshot->text != reference->text;
        result.cursorChanged = snapshot->cursor != reference->cursor;
        result.anchorChanged = snapshot->anchor != reference->anchor;
        return result;
    }

    // Read-only classification for diagnostics; never expose document content
    // or turn a predicted snapshot into an acknowledgement.
    const char* snapshotStatus(const std::optional<Snapshot>& snapshot) const {
        if (!snapshot) return "invalid";
        if (expected_) {
            if (*snapshot == *expected_) return "expected";
            for (const auto& echo : echoes_)
                if (echo == *snapshot) return "stale";
            return "unexpected";
        }
        if (!confirmed_) return "initial";
        if (*confirmed_ == *snapshot) return "current";
        for (const auto& echo : echoes_)
            if (echo == *snapshot) return "stale";
        return "changed";
    }

    Observation observe(const std::optional<Snapshot>& snapshot, bool fromUpdate = false) {
        if (!snapshot) return Observation::Changed;
        if (expected_) {
            if (*snapshot == *expected_) {
                // Returning to an earlier state is legitimate (Backspace,
                // tone changes), but a cache read alone must not ACK it.
                if (requireFreshUpdate_ && !fromUpdate) return Observation::Waiting;
                confirmed_ = expected_;
                expected_.reset();
                requireFreshUpdate_ = false;
                allowStaleCache_ = false;
                return Observation::Ready;
            }
            for (const auto& echo : echoes_)
                if (echo == *snapshot) return Observation::Waiting;
            return Observation::Changed;
        }
        if (confirmed_ && !(*confirmed_ == *snapshot)) {
            // An old notification arriving AFTER ACK is not proof the caret
            // moved. Keep ownership provisionally, but never authorize an edit
            // from this snapshot. The key path must revalidate current text.
            for (const auto& echo : echoes_)
                if (echo == *snapshot) return Observation::Waiting;
            // After assumeAcked(), the cache may still show the pre-edit view.
            // Trust confirmed_ for the next composing flush; a fresh Update
            // that still disagrees is a real context change.
            if (allowStaleCache_ && !fromUpdate) return Observation::Ready;
            allowStaleCache_ = false;
            return Observation::Changed;
        }
        allowStaleCache_ = false;
        confirmed_ = snapshot;
        return Observation::Ready;
    }

    std::optional<Edit> prepare(const std::string& before, const std::string& after) {
        if (!confirmed_ || pending()) return std::nullopt;
        const auto oldWord = codepoints(before);
        const auto newWord = codepoints(after);
        const auto cursor = confirmed_->cursor;
        if (cursor != confirmed_->anchor) {
            // Only a NEW user keystroke may replace the selection via the
            // client's ordinary commitString semantics. Never delete the
            // selected text through deleteSurroundingText or reuse an old word.
            if (!oldWord.empty()) return std::nullopt;
            const auto start = std::min(cursor, confirmed_->anchor);
            const auto length = std::max(cursor, confirmed_->anchor) - start;
            Snapshot next = *confirmed_;
            next.text.replace(start, length, newWord);
            next.cursor = next.anchor = start + static_cast<unsigned>(newWord.size());
            remember(*confirmed_);
            expected_ = std::move(next);
            return Edit{0, utf8(newWord)};
        }
        if (cursor < oldWord.size() ||
            confirmed_->text.substr(cursor - oldWord.size(), oldWord.size()) != oldWord)
            return std::nullopt;
        std::size_t common = 0;
        while (common < oldWord.size() && common < newWord.size() &&
               oldWord[common] == newWord[common]) ++common;
        const auto removed = static_cast<unsigned>(oldWord.size() - common);
        const auto added = newWord.substr(common);
        Snapshot intermediate = *confirmed_;
        intermediate.text.erase(cursor - removed, removed);
        intermediate.cursor -= removed;
        intermediate.anchor = intermediate.cursor;
        Snapshot next = intermediate;
        next.text.insert(next.cursor, added);
        next.cursor += static_cast<unsigned>(added.size());
        next.anchor = next.cursor;
        if (next == *confirmed_) return Edit{0, {}};
        // A repeated state needs a surrounding-text notification after this
        // edit, not merely a matching cached snapshot. This is still a
        // revisionless protocol: identical delayed notifications cannot be
        // distinguished absolutely, so serialize and validate every next key.
        requireFreshUpdate_ = false;
        for (const auto& echo : echoes_)
            if (next == echo) { requireFreshUpdate_ = true; break; }
        remember(*confirmed_);
        if (removed) remember(intermediate);
        expected_ = std::move(next);
        return Edit{removed, utf8(added)};
    }

private:
    std::optional<Snapshot> confirmed_, expected_;
    bool requireFreshUpdate_ = false;
    bool allowStaleCache_ = false;
    std::deque<Snapshot> echoes_;
    void remember(const Snapshot& value) {
        echoes_.push_back(value);
        if (echoes_.size() > 32) echoes_.pop_front();
    }
};
} // namespace openkey
