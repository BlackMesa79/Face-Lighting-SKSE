#pragma once
#include <RE/Skyrim.h>
#include <string>
#include <vector>
#include "SelectedNPCRecord.h"
namespace SelectedNPCs {
    inline constexpr std::size_t limit = SelectedNPCRecord::limit;
    struct Row { RE::FormID id; std::string name; bool enabled; bool loaded; };
    struct View { std::vector<Row> rows; std::string crosshair, console; bool active = false; };
    bool Install();
    void SetActive(bool active);
    void CaptureTargets();
    View Snapshot();
    void AddTarget(bool console);
    void Remove(RE::FormID id);
    void SetEnabled(RE::FormID id, bool enabled);
    // Called on the game thread; never loads actors or scans the world.
    std::vector<RE::ActorHandle> Update();
}
