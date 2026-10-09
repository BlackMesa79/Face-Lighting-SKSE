#pragma once
#include <algorithm>
#include <cstdint>
#include <span>
#include <vector>

struct FollowerPreferences {
    static constexpr std::size_t limit = 4096;
    std::vector<std::uint32_t> disabled;
    bool Enabled(std::uint32_t id) const { return std::find(disabled.begin(), disabled.end(), id) == disabled.end(); }
    bool Set(std::uint32_t id, bool enabled) {
        if (!id || id == 0x14) return false;
        if (enabled) std::erase(disabled, id);
        else if (Enabled(id)) {
            if (disabled.size() >= limit) return false;
            disabled.push_back(id);
        }
        return true;
    }
    template <class Commit>
    bool SetWithCommit(std::uint32_t id, bool enabled, Commit beforeCommit) {
        auto staged = *this;
        if (!staged.Set(id, enabled) || !beforeCommit()) return false;
        disabled.swap(staged.disabled);
        return true;
    }
    static bool ValidLength(std::uint32_t bytes) { return bytes % sizeof(std::uint32_t) == 0 && bytes <= limit * sizeof(std::uint32_t); }
    template <class Resolver> void Restore(std::span<const std::uint32_t> ids, Resolver resolve) {
        for (auto id : ids) {
            if (!id || id == 0x14) continue;
            std::uint32_t remapped = 0;
            if (resolve(id, remapped)) Set(remapped, false);
        }
    }
};
