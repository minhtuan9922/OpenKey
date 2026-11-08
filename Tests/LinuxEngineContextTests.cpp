#include "Composition.h"
#include "ReplayCompositionReference.h"
#include <array>
#include <iostream>
#include <random>
#include <stdexcept>

void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
int main() {
    using openkey::Composition;
    using openkey::test::ReplayReference;
    std::mt19937 random(7319);
    constexpr auto symbols = "abcdefghijklmnopqrstuvwxyz0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    std::array<int, 12> globals{};
    const std::array<int*, 12> preferences{&vInputType, &vQuickTelex, &vCodeTable, &vUseMacro,
        &vRestoreIfWrongSpelling, &vCheckSpelling, &vUseModernOrthography, &vFreeMark,
        &vUpperCaseFirstChar, &vAllowConsonantZFWJ, &vQuickStartConsonant, &vQuickEndConsonant};
    for (int mode : {vTelex, vVNI, vSimpleTelex1, vSimpleTelex2}) {
        for (bool spelling : {false, true}) for (bool modern : {false, true}) for (bool quick : {false, true}) {
            Composition word(mode, {spelling, modern, quick});
            ReplayReference reference(mode, {spelling, modern, quick});
            Composition neighbor(mode == vVNI ? vTelex : vVNI, {!spelling, !modern, !quick});
            for (int step = 0; step < 2000; ++step) {
                const bool reset = word.full() || (random() % 67 == 0);
                const bool backspace = random() % 5 == 0;
                const char symbol = symbols[random() % (sizeof("abcdefghijklmnopqrstuvwxyz0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ") - 1)];
                bool expected = true;
                if (reset) reference.clear();
                else if (backspace) expected = reference.backspace();
                else expected = reference.append(symbol);
                // The incremental context must neither consume nor overwrite globals,
                // even when another engine user has unrelated/invalid preferences.
                for (std::size_t i = 0; i < preferences.size(); ++i) *preferences[i] = globals[i] = static_cast<int>(random() % 97) + 7;
                if (reset) word.clear();
                else if (backspace) require(word.backspace() == expected, "Backspace acceptance differs");
                else require(word.append(symbol) == expected, "Append acceptance differs");
                for (std::size_t i = 0; i < preferences.size(); ++i) require(*preferences[i] == globals[i], "Context wrote a global preference");
                require(word.text() == reference.text(), "Incremental/replay differential mismatch");
                if (neighbor.full()) neighbor.clear();
                neighbor.append('a'); neighbor.append(mode == vVNI ? 's' : '1');
                require(word.text() == reference.text(), "Another context changed existing text");
                if (step % 101 == 0) {
                    auto copy = word;
                    const auto before = word.text();
                    if (!copy.full()) copy.append('b');
                    require(word.text() == before, "Copy shared mutable engine state");
                    word.setOptions({spelling, modern, quick});
                    require(word.text() == reference.text(), "Options replay mismatch");
                }
            }
        }
    }
    // Returning to legacy calls after a context must preserve the legacy API.
    for (auto* preference : preferences) *preference = 0;
    vInputType = vTelex; vCodeTable = 0; vUseMacro = 0; vCheckSpelling = 1;
    vUseModernOrthography = 1; vQuickTelex = 0;
    auto* legacy = static_cast<vKeyHookState*>(vKeyInit()); startNewSession();
    vKeyHandleEvent(Keyboard, KeyDown, KEY_A);
    openkey::EngineContext context;
    context.process(KEY_O); context.process(KEY_O);
    vKeyHandleEvent(Keyboard, KeyDown, KEY_S);
    require(legacy->code == vWillProcess, "Context corrupted the legacy API state");
    require((legacy->charData[0] & CHAR_MASK) == 0xe1, "Legacy vowel changed after independent context");
    std::cout << "Engine context: 64000 differential events, 4 modes, options, copy, interleave and globals passed\n";
}
