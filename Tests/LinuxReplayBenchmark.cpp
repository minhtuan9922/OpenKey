#include "Composition.h"
#include "ReplayCompositionReference.h"
#include <algorithm>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <vector>

int main() {
    using Clock = std::chrono::steady_clock;
    struct Case { const char* id; int mode; std::string input; };
    const std::vector<Case> cases{{"telex-word", vTelex, "coongj"}, {"vni-word", vVNI, "co6ng5"},
        {"bounded-long-word", vTelex, std::string(openkey::Composition::MaxEvents, 'a')}};
    std::cout << "{\"scope\":\"composition-context-vs-replay\",\"cases\":[" << std::fixed << std::setprecision(3);
    bool first = true;
    for (const auto* implementation : {"context", "replay-reference"}) for (const auto& test : cases) {
        std::vector<double> times;
        for (int repetition = 0; repetition < 500; ++repetition) {
            const auto measure = [&](auto word) {
            for (char key : test.input) {
                const auto begin = Clock::now();
                if (!word.append(key)) throw std::runtime_error("Benchmark word rejected");
                const auto end = Clock::now();
                if (repetition >= 20) times.push_back(std::chrono::duration<double, std::micro>(end - begin).count());
            }
            };
            if (std::string(implementation) == "context") measure(openkey::Composition(test.mode));
            else measure(openkey::test::ReplayReference(test.mode));
        }
        std::sort(times.begin(), times.end());
        auto percentile = [&](double fraction) { return times[static_cast<std::size_t>(fraction * (times.size() - 1))]; };
        if (!first) std::cout << ',';
        first = false;
        std::cout << "{\"id\":\"" << test.id << "\",\"implementation\":\"" << implementation << "\",\"samples\":" << times.size()
            << ",\"p50_us\":" << percentile(.5) << ",\"p95_us\":" << percentile(.95)
            << ",\"p99_us\":" << percentile(.99) << ",\"max_us\":" << times.back() << '}';
    }
    std::cout << "]}\n";
}
