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

    void clear() { expected_.reset(); confirmed_.reset(); echoes_.clear(); requireFreshUpdate_ = false; }

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
            return Observation::Changed;
        }
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
    std::deque<Snapshot> echoes_;
    void remember(const Snapshot& value) {
        echoes_.push_back(value);
        if (echoes_.size() > 32) echoes_.pop_front();
    }
};
} // namespace openkey
