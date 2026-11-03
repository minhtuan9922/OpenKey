#include "InputMonitor.h"
#include <algorithm>

namespace openkey {
void StatusCache::update(InputStatus status, Clock::time_point now) {
    if (!sampled || now - sampled_ >= ttl_) count_ = 0;
    sampled = true;
    sampled_ = now;
    if (status == InputStatus::Unknown || status == InputStatus::Unavailable) {
        count_ = 0;
        candidate_ = InputStatus::Unknown;
        published_ = status; // Never keep displaying T/N/E on an invalid sample.
        return;
    }
    if (!count_ || candidate_ != status) {
        candidate_ = status;
        count_ = 1;
        published_ = InputStatus::Unknown;
    } else {
        count_ = 2;
        published_ = status;
    }
}
InputStatus StatusCache::value(Clock::time_point now) const {
    return !sampled || now - sampled_ >= ttl_ ? InputStatus::Unknown : published_;
}
InputMonitor::InputMonitor(std::function<InputStatus()> reader, std::chrono::milliseconds interval,
                           std::chrono::milliseconds ttl)
    : reader_(std::move(reader)), interval_(std::max(interval, std::chrono::milliseconds(5))),
      maxSampleAge_(std::max(ttl, std::chrono::milliseconds(5))), cache_(maxSampleAge_) {
    worker_ = std::thread([this] { run(); });
}
InputMonitor::~InputMonitor() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        stopping_ = true;
    }
    wake_.notify_all();
    worker_.join();
}
void InputMonitor::run() {
    for (;;) {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (stopping_) return;
        }
        // Never hold the cache mutex across bus I/O. Even a stalled reader must
        // not stall status/quit IPC or prevent expiry of its last sample.
        InputStatus status = InputStatus::Unknown;
        const auto started = StatusCache::Clock::now();
        try { status = reader_(); } catch (...) { /* Publish Unknown, not stale V/E. */ }
        const auto completed = StatusCache::Clock::now();
        if (completed - started >= maxSampleAge_) status = InputStatus::Unknown;
        std::unique_lock<std::mutex> lock(mutex_);
        if (stopping_) return;
        cache_.update(status, completed);
        if (wake_.wait_for(lock, interval_, [this] { return stopping_; })) return;
    }
}
InputStatus InputMonitor::snapshot() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return cache_.value(StatusCache::Clock::now());
}
}
