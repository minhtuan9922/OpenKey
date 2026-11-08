#pragma once
#include "DataType.h"
#include <memory>

namespace openkey {
// Independent Linux typing state. Owns preferences, hook output, spelling state,
// word/undo buffers and scratch variables; no global preference writes or replay.
// Callers still confine engine/table access to the Fcitx event thread.
class EngineContext {
public:
    struct Options {
        int inputType = vTelex;
        bool checkSpelling = true;
        bool modernOrthography = true;
        bool quickTelex = false;
        bool freeMark = false;
        bool allowConsonantZFWJ = false;
        bool quickStartConsonant = false;
        bool quickEndConsonant = false;
        bool restoreWrongSpelling = false;
    };
    EngineContext();
    explicit EngineContext(Options options);
    ~EngineContext();
    EngineContext(const EngineContext&);
    EngineContext& operator=(const EngineContext&);
    EngineContext(EngineContext&&) noexcept;
    EngineContext& operator=(EngineContext&&) noexcept;
    const vKeyHookState& process(Uint16 code, bool caps = false);
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
