#pragma once
#include <RE/Skyrim.h>
#include <string>
#include <optional>
namespace LightExclusionProbe {
    void Install();
    void Register(RE::NiLight* light, bool player);
    void Unregister(RE::NiLight* light);
    void Reset();
    void Update(bool enabled, bool gameplay, bool diagnostics);
    std::optional<float> ReadFiltered(RE::Actor* player);
    std::string Snapshot();
}
