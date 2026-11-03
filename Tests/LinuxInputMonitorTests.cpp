#include "InputMonitor.h"
#include <atomic>
#include <chrono>
#include <iostream>
#include <stdexcept>
#include <thread>

namespace {
using namespace std::chrono_literals;
using openkey::InputStatus;
void require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
template<class Predicate> void await(Predicate predicate) {
    const auto deadline = std::chrono::steady_clock::now() + 2s;
    while (!predicate()) {
        require(std::chrono::steady_clock::now() < deadline, "Monitor did not reach expected state");
        std::this_thread::sleep_for(2ms);
    }
}
}
int main() {
    using namespace std::chrono_literals;
    const auto start = openkey::StatusCache::Clock::now();
    openkey::StatusCache cache;
    require(cache.value(start) == InputStatus::Unknown, "Initial state must be unknown");
    cache.update(InputStatus::Telex, start);
    require(cache.value(start) == InputStatus::Unknown, "One sample must not publish V");
    cache.update(InputStatus::Telex, start + 300ms);
    require(cache.value(start + 300ms) == InputStatus::Telex, "Two stable samples must publish V");
    cache.update(InputStatus::English, start + 600ms);
    require(cache.value(start + 600ms) == InputStatus::Unknown, "A changed candidate must invalidate old V");
    cache.update(InputStatus::Telex, start + 900ms);
    require(cache.value(start + 900ms) == InputStatus::Unknown, "Flapping must not publish a single sample");
    cache.update(InputStatus::English, start + 1200ms);
    cache.update(InputStatus::English, start + 1500ms);
    require(cache.value(start + 1500ms) == InputStatus::English, "Stable English must publish E");
    cache.update(InputStatus::Unavailable, start + 1600ms);
    require(cache.value(start + 1600ms) == InputStatus::Unavailable, "Bus loss must invalidate E immediately");
    cache.update(InputStatus::Telex, start + 1700ms);
    require(cache.value(start + 1700ms) == InputStatus::Unknown, "Recovery needs two new samples");
    cache.update(InputStatus::Telex, start + 1800ms);
    require(cache.value(start + 3800ms) == InputStatus::Unknown, "Expired sample must not publish stale V");
    cache.update(InputStatus::Telex, start + 3900ms);
    require(cache.value(start + 3900ms) == InputStatus::Unknown, "Long gap resets debounce history");
    cache.update(InputStatus::Unknown, start + 4000ms);
    require(cache.value(start + 4000ms) == InputStatus::Unknown, "Unknown sample must invalidate cache");
    cache.update(InputStatus::Telex, start + 4100ms);
    cache.update(InputStatus::Telex, start + 4200ms);
    cache.update(InputStatus::Vni, start + 4300ms);
    require(cache.value(start + 4300ms) == InputStatus::Unknown, "Telex-to-VNI transition must clear the old selection");
    cache.update(InputStatus::Vni, start + 4400ms);
    require(cache.value(start + 4400ms) == InputStatus::Vni, "Two VNI samples must publish N");
    require(cache.value(start + 6400ms) == InputStatus::Unknown, "Expired VNI must clear selection");

    std::atomic<InputStatus> sampled{InputStatus::Telex};
    std::atomic<unsigned> calls{0};
    {
        openkey::InputMonitor monitor([&] { ++calls; return sampled.load(); }, 5ms);
        await([&] { return monitor.snapshot() == InputStatus::Telex; });
        sampled = InputStatus::Vni;
        await([&] { return monitor.snapshot() == InputStatus::Vni; });
        sampled = InputStatus::English;
        await([&] { return monitor.snapshot() == InputStatus::English; });
        sampled = InputStatus::Unavailable;
        await([&] { return monitor.snapshot() == InputStatus::Unavailable; });
        sampled = InputStatus::Other;
        await([&] { return monitor.snapshot() == InputStatus::Other; });
    }
    const auto stoppedCalls = calls.load();
    std::this_thread::sleep_for(15ms);
    require(calls == stoppedCalls, "Destroying monitor must stop its reader");

    std::atomic<bool> block{true}, entered{false};
    std::atomic<unsigned> slowCalls{0};
    openkey::InputMonitor slow([&] {
        if (++slowCalls <= 2) return InputStatus::Telex;
        entered = true;
        while (block) std::this_thread::sleep_for(1ms);
        return InputStatus::English;
    }, 5ms, 40ms);
    struct Release { std::atomic<bool>& gate; ~Release() { gate = false; } } release{block};
    await([&] { return entered.load(); });
    await([&] { return slow.snapshot() == InputStatus::Unknown; });
    block = false;
    await([&] { return slow.snapshot() == InputStatus::English; });

    std::atomic<unsigned> failures{0};
    openkey::InputMonitor failing([&]() -> InputStatus { ++failures; throw std::runtime_error("Test failure"); }, 5ms);
    await([&] { return failures.load() > 0; });
    require(failing.snapshot() == InputStatus::Unknown, "Reader exceptions must not escape into service");
    std::cout << "Monitor debounce, expiry, async transitions, shutdown and stalled-reader cache passed\n";
}
