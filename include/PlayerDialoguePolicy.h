#pragma once
#include <optional>

// Dialogue edges change the player's saved switch, not per-frame visibility.
struct PlayerDialoguePolicy {
    bool wasOpen = false;
    bool temporaryConversation = false;
    void Reset() { wasOpen = temporaryConversation = false; }
    std::optional<bool> Update(bool open, bool enableOnStart, bool disableOnEnd, bool enabled, bool temporary = false) {
        if (open && temporary) temporaryConversation = true;
        const bool entering = open && !wasOpen;
        const bool leaving = !open && wasOpen;
        wasOpen = open;
        const bool suppress = temporary || temporaryConversation;
        if (!open) temporaryConversation = false;
        if (suppress) return std::nullopt;
        if (entering && enableOnStart && !enabled) return true;
        if (leaving && disableOnEnd && enabled) return false;
        return std::nullopt;
    }
};
