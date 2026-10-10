#pragma once
#include <cstdint>

// UI-thread-only state, independent of XAML for lifecycle regression tests.
class UpdateRequestState {
    uint64_t epoch = 0, active = 0;
    bool loaded = false;
public:
    void Loaded() { loaded = true; ++epoch; }
    void Unloaded() { loaded = false; ++epoch; }
    bool IsLoaded() const { return loaded; }
    // The caller holds the ticket until download, confirmation and launch finish.
    bool Busy() const { return active != 0; }
    // Link launches share the InfoBar with updates: newest action owns its result.
    // Do not release an in-flight update job or allow a second download.
    bool BeginStatus(uint64_t& ticket) {
        if (!loaded) return false;
        ticket = ++epoch;
        return true;
    }
    bool Begin(uint64_t& ticket) {
        if (!loaded || active) return false;
        ticket = ++epoch;
        active = ticket;
        return true;
    }
    bool Current(uint64_t ticket) const { return loaded && ticket == epoch; }
    bool Finish(uint64_t ticket) {
        if (active != ticket) return false;
        active = 0;
        return Current(ticket);
    }
};
