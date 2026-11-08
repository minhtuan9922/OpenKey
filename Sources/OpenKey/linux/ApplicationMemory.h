#pragma once
#include "ApplicationRules.h"

namespace openkey {
// Stores only exact frontend program IDs and five supported input method names.
class ApplicationMemory {
public:
    static constexpr std::size_t MaxApplications = 128;
    static bool supported(const std::string& method) {
        return method == "keyboard-us" || method == "openkey-telex" || method == "openkey-vni" ||
            method == "openkey-simple-telex1" || method == "openkey-simple-telex2";
    }
    bool load(const std::string& encoded) {
        MacroTable::Entries next;
        if (!MacroTable::parse(encoded, next, 128) || next.size() > MaxApplications) return false;
        for (const auto& [program, method] : next)
            if (!ApplicationRules::identifiable(program) || !supported(method)) return false;
        entries_ = std::move(next);
        return true;
    }
    std::string encode() const { return MacroTable::encode(entries_); }
    std::string lookup(const std::string& program) const {
        if (!ApplicationRules::identifiable(program)) return {};
        const auto it = entries_.find(program);
        return it == entries_.end() ? std::string() : it->second;
    }
    bool remember(const std::string& program, const std::string& method) {
        if (!ApplicationRules::identifiable(program)) return false;
        if (!supported(method)) return entries_.erase(program) != 0;
        auto it = entries_.find(program);
        if (it != entries_.end() && it->second == method) return false;
        if (it == entries_.end() && entries_.size() >= MaxApplications) return false;
        entries_[program] = method;
        return true;
    }
private:
    MacroTable::Entries entries_;
};
}
