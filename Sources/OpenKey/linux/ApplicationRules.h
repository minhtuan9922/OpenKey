#pragma once
#include "MacroTable.h"
#include <algorithm>
namespace openkey {
class ApplicationRules {
public:
    using Entries = MacroTable::Entries;
    static bool identifiable(const std::string& program) {
        if (program.empty() || program.size() > 128) return false;
        for (unsigned char ch : program)
            if (!((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
                  (ch >= '0' && ch <= '9') || ch == '.' || ch == '_' || ch == '-' || ch == '/' || ch == ':')) return false;
        auto name = program.substr(program.find_last_of("/:") == std::string::npos ? 0 : program.find_last_of("/:") + 1);
        std::transform(name.begin(), name.end(), name.begin(), [](unsigned char ch) { return ch >= 'A' && ch <= 'Z' ? ch + ('a' - 'A') : ch; });
        return !name.empty() && name != "gnome-shell" && name != "ibus" && name != "fcitx5" &&
            name != "unknown" && name != "wayland";
    }
    static bool validEntry(const std::string& program, const std::string& style) {
        return identifiable(program) && (style == "Preedit" || style == "Direct" || style == "CommitOnly" || style == "NativeDirect");
    }
    static bool parse(const std::string& encoded, Entries& entries) {
        Entries next;
        if (!MacroTable::parse(encoded, next, 128)) return false;
        for (const auto& [program, style] : next) if (!validEntry(program, style)) return false;
        entries = std::move(next);
        return true;
    }
};
}
