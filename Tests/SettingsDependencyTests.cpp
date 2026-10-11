#include <iostream>
#include "../Sources/OpenKey/win32/OpenKey/OpenKey.UI/SettingsDependencyState.h"

int main()
{
    int passed = 0;
    // All Chromium combinations, including a retained selection while locked.
    for (int prerequisite = 0; prerequisite < 2; ++prerequisite)
        for (int selected = 0; selected < 2; ++selected)
        {
            const auto state = OpenKeyUISettingsDependencies::Evaluate(prerequisite != 0, selected != 0);
            if (state.available != (prerequisite != 0) ||
                state.warnRetainedSelection != (prerequisite == 0 && selected != 0)) return 1;
            ++passed;
        }
    // All advanced spelling combinations: either retained toggle warrants notice.
    for (int spelling = 0; spelling < 2; ++spelling)
        for (int consonants = 0; consonants < 2; ++consonants)
            for (int temporary = 0; temporary < 2; ++temporary)
            {
                const auto state = OpenKeyUISettingsDependencies::Evaluate(
                    spelling != 0, consonants != 0 || temporary != 0);
                if (state.available != (spelling != 0) ||
                    state.warnRetainedSelection != (spelling == 0 && (consonants != 0 || temporary != 0))) return 1;
                ++passed;
            }
    // Prerequisite off/on does not consume or clear the stored selection.
    const bool retained = true;
    const auto locked = OpenKeyUISettingsDependencies::Evaluate(false, retained);
    const auto unlocked = OpenKeyUISettingsDependencies::Evaluate(true, retained);
    if (!retained || !locked.warnRetainedSelection || unlocked.warnRetainedSelection || !unlocked.available) return 1;
    ++passed;
    std::cout << "Settings dependencies: " << passed << "/13 passed; presentation only, no registry/UI/hook.\n";
}
