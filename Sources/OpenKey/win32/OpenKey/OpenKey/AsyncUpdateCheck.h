#pragma once
#include "UpdateCheckService.h"
#include <atomic>
#include <functional>
#include <memory>
#include <thread>
namespace SetupUpdateService { struct Package; }

// UI owns this coordinator. Workers own only shared state, never a dialog/app.
class AsyncUpdateCheck {
public:
    struct Outcome {
        UpdateCheckService::Result result = UpdateCheckService::Result::DownloadFailed;
        std::string version;
        bool setupDownload = false;
        std::shared_ptr<SetupUpdateService::Package> installer;
    };
private:
    struct State {
        std::atomic<bool> ready{ false };
        Outcome outcome;
    };
    std::shared_ptr<State> state;
public:
    bool Busy() const { return bool(state); }
    void* Token() const { return state.get(); }
    template<class Work, class Launch> bool Start(Work work, Launch launch) {
        if (state) return false;
        try {
            auto pending = std::make_shared<State>();
            auto run = [pending, work]() mutable {
                try { pending->outcome = work(); }
                catch (...) { pending->outcome = Outcome{}; }
                pending->ready.store(true, std::memory_order_release);
            };
            if (!launch(std::function<void()>(run))) return false;
            state = std::move(pending);
            return true;
        } catch (...) { return false; }
    }
    template<class Work> bool Start(Work work) {
        return Start(work, [](std::function<void()> run) {
            std::thread worker(std::move(run));
            worker.detach();
            return true;
        });
    }
    bool Poll(Outcome& output) {
        if (!state || !state->ready.load(std::memory_order_acquire)) return false;
        output = std::move(state->outcome);
        state.reset();
        return true;
    }
    void Abandon() { state.reset(); }
};
