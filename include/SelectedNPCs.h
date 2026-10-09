#pragma once
#include <RE/Skyrim.h>
#include <string>
#include <vector>
#include "SelectedNPCRecord.h"
#include "FaceLightingAPI.h"
#include <optional>
namespace SelectedNPCs {
    inline constexpr std::size_t limit = SelectedNPCRecord::limit;
    struct Row { RE::FormID id; std::string name; bool enabled; bool loaded; };
    struct View { std::vector<Row> rows; std::string crosshair, console; bool active = false; };
    bool Install();
    void SetActive(bool active);
    void CaptureTargets();
    View Snapshot();
    void AddTarget(bool console);
    void ToggleCrosshairTarget();
    void Remove(RE::FormID id);
    void SetEnabled(RE::FormID id, bool enabled);
    void SetFollowerEnabled(RE::FormID id, bool enabled);
    std::optional<bool> PersonalEnabled(RE::FormID id);
    FaceLightingAPI::Result SetPersonalNow(RE::Actor* actor, bool enabled);
    // Called on the game thread; never loads actors or scans the world.
    std::vector<RE::ActorHandle> Update();
}
