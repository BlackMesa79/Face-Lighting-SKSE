#pragma once
#include <RE/Skyrim.h>
#include <SKSE/SKSE.h>
#include <string>
#include <vector>
#include <functional>

namespace Followers {
    inline constexpr std::size_t lightLimit = 32;
    struct Row { RE::FormID id; std::string name; bool loaded; bool enabled; };
    // Game-thread discovery; snapshots are safe for the configuration UI.
    void Reset();
    std::vector<RE::ActorHandle> Update();
    std::vector<Row> Snapshot();
    void SetEnabled(RE::FormID id, bool enabled);
    bool PersonalEnabled(RE::FormID id);
    bool SetPersonalNow(RE::FormID id, bool enabled); // Main thread; does not queue.
    // Stage allocating changes before a fallible settings write, then publish preferences.
    bool SetPersonalWithCommit(RE::FormID id, bool enabled, const std::function<bool()>& beforeCommit);
    void RevertPreferences();
    void Save(SKSE::SerializationInterface* api);
    bool LoadRecord(SKSE::SerializationInterface* api, std::uint32_t type, std::uint32_t version, std::uint32_t length);
}
