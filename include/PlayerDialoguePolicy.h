#pragma once
#include <optional>

// Dialogue edges change the player's saved switch, not per-frame visibility.
struct PlayerDialoguePolicy {
    bool wasOpen = false;
    void Reset() { wasOpen = false; }
    std::optional<bool> Update(bool open, bool enableOnStart, bool disableOnEnd, bool enabled) {
        const bool entering = open && !wasOpen;
        const bool leaving = !open && wasOpen;
        wasOpen = open;
        if (entering && enableOnStart && !enabled) return true;
        if (leaving && disableOnEnd && enabled) return false;
        return std::nullopt;
    }
};
