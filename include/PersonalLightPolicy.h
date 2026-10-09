#pragma once
#include "FaceLightingAPI.h"
#include "SelectedNPCRecord.h"

namespace PersonalLightPolicy {
    inline bool Enabled(const std::vector<SelectedNPCRecord::Record>& rows, std::uint32_t id,
        bool follower, bool followerEnabled, bool followerGroup, bool selectedGroup) {
        return (follower && followerEnabled && followerGroup) ||
            (selectedGroup && std::any_of(rows.begin(), rows.end(), [id](const auto& row) {
                return row.id == id && row.enabled;
            }));
    }
    // Caller serializes source preferences. Stage all allocating changes before committing.
    template <class SetFollower>
    FaceLightingAPI::Result Set(std::vector<SelectedNPCRecord::Record>& rows, std::uint32_t id,
        bool follower, bool followerEnabled, bool enabled, bool followerGroup, bool selectedGroup, SetFollower setFollower) {
        using FaceLightingAPI::Result;
        if (!id || id == 0x14) return Result::InvalidTarget;
        if (enabled && !(follower ? followerGroup : selectedGroup)) return Result::SourceDisabled;
        auto staged = rows;
        auto row = std::find_if(staged.begin(), staged.end(), [id](const auto& r) { return r.id == id; });
        const bool selected = row != staged.end();
        bool changed = selected && (row->enabled != static_cast<unsigned>(enabled));
        if (selected) row->enabled = enabled;
        else if (enabled && !follower) {
            if (staged.size() >= SelectedNPCRecord::limit) return Result::ListFull;
            staged.push_back({id, 1}); changed = true;
        }
        if (follower || selected || enabled) {
            changed |= followerEnabled != enabled;
            if (!setFollower(id, enabled)) return Result::ListFull;
        }
        rows.swap(staged);
        return changed ? Result::Ok : Result::NoChange;
    }
}
