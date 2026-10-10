#pragma once

namespace OpenKeyUISettingsDependencies
{
    struct State
    {
        bool available;
        bool warnRetainedSelection;
    };

    // Presentation only. Never clears a dependent preference or writes settings.
    constexpr State Evaluate(bool prerequisiteEnabled, bool dependentSelected) noexcept
    {
        return { prerequisiteEnabled, !prerequisiteEnabled && dependentSelected };
    }
}
