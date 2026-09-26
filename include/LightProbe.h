#pragma once
#include <RE/Skyrim.h>
#include "Settings.h"
#include <string>
namespace LightProbe {
    void Reset();
    // Called before light changes; returned gate only applies to the player light.
    bool Update(RE::Actor* player, const Settings::Values& settings, bool playerLight, std::size_t npcEntries, float playerOpacity = 1.0f);
    std::string Snapshot();
    void Mark();
}
