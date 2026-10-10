// Reuse the same standalone engine globals and run correctness checks before timing.
// No Windows hooks, clipboard operations, registry or UI are linked into this program.
#define main RunEngineRegression
#include "EngineRegressionTests.cpp"
#undef main
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <iomanip>
#include <vector>

int main() {
    if (RunEngineRegression() != 0) return 1;
    const char* telex[] = { "tieengs", "vieetj", "dduowngf", "tuowngr", "hoas", "quas" };
    const char* vni[] = { "tie6ng1", "vie6t5", "duo72ng", "tuo73ng", "hoa1", "qua1" };
    struct Scenario { const char* name; int inputType; int modern; const char* const* words; };
    const Scenario scenarios[] = {
        { "telex-modern", vTelex, 1, telex }, { "telex-old", vTelex, 0, telex },
        { "vni-modern", vVNI, 1, vni }, { "vni-old", vVNI, 0, vni }
    };
    constexpr int rounds = 100;
    constexpr int cycles = 200;
    volatile unsigned long long checksum = 0;
    std::cout << std::fixed << std::setprecision(3);
    std::cout << "BENCH_JSON [";
    for (size_t scenarioIndex = 0; scenarioIndex < sizeof(scenarios) / sizeof(scenarios[0]); ++scenarioIndex) {
        const auto& scenario = scenarios[scenarioIndex];
        vInputType = scenario.inputType;
        vUseModernOrthography = scenario.modern;
        size_t eventsPerCycle = 0;
        for (size_t word = 0; word < 6; ++word) eventsPerCycle += std::strlen(scenario.words[word]) + 1;
        auto runBatch = [&](int iterations) {
            for (int cycle = 0; cycle < iterations; ++cycle) {
                for (size_t word = 0; word < 6; ++word) {
                    for (const char* key = scenario.words[word]; *key; ++key) {
                        const Uint16 code = *key >= 'a' && *key <= 'z' ? *key - 'a' + KEY_A : *key;
                        vKeyHandleEvent(Keyboard, KeyDown, code);
                    }
                    vKeyHandleEvent(Keyboard, KeyDown, KEY_SPACE);
                }
            }
        };
        vKeyInit();
        startNewSession();
        runBatch(20); // Warm-up; not part of samples.
        std::vector<double> samples;
        samples.reserve(rounds);
        for (int round = 0; round < rounds; ++round) {
            auto* state = static_cast<vKeyHookState*>(vKeyInit());
            startNewSession();
            const auto begin = std::chrono::steady_clock::now();
            runBatch(cycles);
            const auto end = std::chrono::steady_clock::now();
            const double elapsed = std::chrono::duration<double, std::micro>(end - begin).count();
            samples.push_back(elapsed / static_cast<double>(cycles * eventsPerCycle));
            checksum += state->code + state->backspaceCount + state->newCharCount;
        }
        double mean = 0;
        for (const auto sample : samples) mean += sample / rounds;
        std::sort(samples.begin(), samples.end());
        auto percentile = [&](double fraction) { return samples[static_cast<size_t>(std::ceil(fraction * rounds)) - 1]; };
        if (scenarioIndex) std::cout << ',';
        std::cout << "{\"scenario\":\"" << scenario.name << "\",\"events\":" << rounds * cycles * eventsPerCycle
            << ",\"rounds\":" << rounds << ",\"mean_us_per_event\":" << mean
            << ",\"p50_batch_us_per_event\":" << percentile(0.50)
            << ",\"p95_batch_us_per_event\":" << percentile(0.95)
            << ",\"p99_batch_us_per_event\":" << percentile(0.99) << '}';
    }
    std::cout << "]\n";
    std::cout << "Checksum: " << checksum << '\n';
    return 0;
}
