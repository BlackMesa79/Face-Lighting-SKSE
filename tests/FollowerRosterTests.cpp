#include "FollowerRoster.h"
#include "FollowerPreferences.h"
#include <iostream>
#include <set>
#include <stdexcept>

int main() {
    try {
        std::vector<int> roster;
        std::set<int> teammates{1, 2};
        auto update = [&](std::initializer_list<int> candidates) {
            FollowerRoster::Reconcile(roster, std::span<const int>(candidates.begin(), candidates.size()),
                [&](int key) { return teammates.contains(key); });
        };
        auto check = [&](std::vector<int> expected) {
            if (roster != expected) throw std::runtime_error("Follower roster lifecycle failed");
        };
        update({1, 1, 3}); check({1}); // recruit once; ordinary NPC excluded
        update({2}); check({1, 2}); // retain distant/waiting teammate, discover new recruit
        teammates.erase(1);
        update({1, 2}); check({2}); // dismissal, death, or invalid handle
        teammates.insert(1);
        update({1}); check({2, 1}); // re-recruit
        roster.clear(); // load/new-game reset: never carry a runtime roster across saves
        teammates = {7};
        update({1, 2, 7, 7}); check({7});
        teammates.clear();
        update({}); check({});
        FollowerPreferences prefs;
        if (!prefs.Enabled(7) || !prefs.Set(7, false) || prefs.Enabled(7)) throw std::runtime_error("Follower toggle failed");
        // Automatic rediscovery cannot overwrite a saved opt-out.
        teammates = {7}; update({7}); check({7});
        roster.clear(); update({7});
        if (prefs.Enabled(7)) throw std::runtime_error("Rejoin reset opt-out");
        FollowerPreferences restored;
        const std::vector<std::uint32_t> saved{0, 0x14, 7, 7, 8, 9, 10};
        restored.Restore(saved, [](auto id, auto& remapped) {
            remapped = id == 9 ? 0x14 : id == 10 ? 0 : id + 100;
            return id != 8;
        });
        if (restored.disabled != std::vector<std::uint32_t>{107}) throw std::runtime_error("Preference form remapping failed");
        if (!restored.Set(107, true) || !restored.disabled.empty()) throw std::runtime_error("Preference re-enable failed");
        prefs.disabled.clear();
        if (!prefs.Enabled(7)) throw std::runtime_error("Save revert failed");
        if (!FollowerPreferences::ValidLength(0) || FollowerPreferences::ValidLength(3) ||
            FollowerPreferences::ValidLength(4096 * 4 + 4)) throw std::runtime_error("Invalid preference record accepted");
        std::cout << "Follower recruitment, deduplication, waiting, dismissal, rejoin and save reset passed.\n";
    } catch (const std::exception& error) { std::cerr << error.what(); return 1; }
}
