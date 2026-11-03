#pragma once
#include "FcitxStatus.h"
#include <chrono>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <thread>

namespace openkey {
// Pure filter; callers must serialize access. Times are monotonic sample times.
class StatusCache {
public:
    using Clock = std::chrono::steady_clock;
    explicit StatusCache(std::chrono::milliseconds ttl = std::chrono::seconds(2)) : ttl_(ttl) {}
    void update(InputStatus status, Clock::time_point now);
    InputStatus value(Clock::time_point now) const;
private:
    std::chrono::milliseconds ttl_;
    Clock::time_point sampled_{};
    bool sampled = false;
    InputStatus candidate_ = InputStatus::Unknown, published_ = InputStatus::Unknown;
    unsigned count_ = 0;
};

class InputMonitor {
public:
    explicit InputMonitor(std::function<InputStatus()> reader = readFcitxStatus,
        std::chrono::milliseconds interval = std::chrono::milliseconds(300),
        std::chrono::milliseconds ttl = std::chrono::seconds(2));
    ~InputMonitor();
    InputMonitor(const InputMonitor&) = delete;
    InputMonitor& operator=(const InputMonitor&) = delete;
    InputStatus snapshot() const;
private:
    void run();
    std::function<InputStatus()> reader_;
    std::chrono::milliseconds interval_;
    std::chrono::milliseconds maxSampleAge_;
    mutable std::mutex mutex_;
    std::condition_variable wake_;
    bool stopping_ = false;
    StatusCache cache_;
    std::thread worker_;
};
}
