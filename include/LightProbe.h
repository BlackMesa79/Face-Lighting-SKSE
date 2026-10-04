#pragma once
#include <RE/Skyrim.h>
#include "Settings.h"
#include <string>
#include <optional>
namespace LightProbe {
    void Reset();
    // Called before light changes; returned gate only applies to the player light.
    // Safety/fade guards run each frame, samples/decisions only when sampleDue.
    bool Update(RE::Actor* player, const Settings::Values& settings, bool playerLight, std::size_t npcEntries, float playerOpacity = 1.0f, bool sampleDue = true);
    // Latest player-position sample from Update; unavailable during paused/invalid gameplay.
    std::optional<float> Environment(int mode);
    std::string Snapshot();
    void Mark();
}
