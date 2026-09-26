#pragma once
#include <algorithm>
#include <span>
#include <vector>

namespace FollowerRoster {
    // Process discovery is incomplete for distant waiting teammates. Retain a
    // previously discovered member until its handle or teammate status expires.
    template <class Key, class IsMember>
    void Reconcile(std::vector<Key>& members, std::span<const Key> candidates, IsMember isMember) {
        std::erase_if(members, [&](const auto& key) { return !isMember(key); });
        for (const auto& key : candidates)
            if (std::find(members.begin(), members.end(), key) == members.end() && isMember(key))
                members.push_back(key);
    }
}
