#include "NpcLightManager.h"
#include <iostream>
#include <map>
#include <stdexcept>
#include <set>
#include <source_location>
#include <string>

struct Light {
    static inline int cleared = 0;
    void Clear() { ++cleared; }
};
void Check(bool value, std::source_location at = std::source_location::current()) {
    if (!value) throw std::runtime_error("NPC light manager failed at line " + std::to_string(at.line()));
}
int main() {
    try {
        NpcLightManager<int, int, Light> manager{2};
        std::map<int, std::pair<int, float>> rendered;
        std::set<int> invalid;
        auto tick = [&](float delta = 1.0f) {
            rendered.clear();
            manager.Update(delta, [&](int actor) { return !invalid.contains(actor); },
                [&](Light&, int actor, int parameter, float opacity) { rendered[actor] = {parameter, opacity}; });
        };
        manager.BeginFrame();
        manager.Submit(1, NpcLightSource::Selected, 10, false, 0);
        manager.Submit(1, NpcLightSource::Dialogue, 20, false, 0);
        manager.Submit(1, NpcLightSource::Dialogue, 21, false, 0);
        tick();
        Check(manager.Size() == 1 && rendered.at(1).first == 21);
        const int before = Light::cleared;
        manager.BeginFrame();
        manager.Submit(1, NpcLightSource::Selected, 10, false, 0);
        tick();
        Check(rendered.at(1).first == 10 && rendered.at(1).second == 1 && Light::cleared == before);
        manager.BeginFrame();
        tick();
        Check(manager.Size() == 0 && Light::cleared == before + 1);

        // Reverse an outgoing fade without adding a second light for the actor.
        manager.BeginFrame();
        manager.Submit(1, NpcLightSource::Dialogue, 1, true, 1);
        tick(); tick();
        Check(rendered.at(1).second == 1);
        manager.BeginFrame(); tick(); tick(0.5f);
        Check(rendered.at(1).second > 0 && rendered.at(1).second < 1);
        const auto opacity = rendered.at(1).second;
        manager.BeginFrame();
        manager.Submit(1, NpcLightSource::Dialogue, 1, true, 1);
        tick(0);
        Check(manager.Size() == 1 && rendered.at(1).second == opacity);
        tick();
        Check(rendered.at(1).second == 1);

        // Rapid target changes stay bounded and evict the oldest outgoing entry.
        manager.BeginFrame(); manager.Submit(2, NpcLightSource::Dialogue, 2, true, 1); tick(); tick(0.2f);
        manager.BeginFrame(); manager.Submit(3, NpcLightSource::Dialogue, 3, true, 1); tick();
        Check(manager.Size() == 2 && !rendered.contains(1) && rendered.contains(3));
        invalid.insert(3); tick();
        Check(!rendered.contains(3));
        manager.RefreshSource(NpcLightSource::Dialogue, 9, false, 0);
        manager.BeginFrame(); tick();
        Check(manager.Size() == 0);

        manager.BeginFrame(); manager.Submit(4, NpcLightSource::Follower, 4, false, 0); tick();
        manager.Clear(); tick();
        Check(manager.Size() == 0 && rendered.empty());
        NpcLightManager<int, int, Light> disabled{0};
        disabled.Submit(1, NpcLightSource::Dialogue, 1, false, 0);
        disabled.Update(1, [](int) { return true; }, [](auto&...) { throw std::runtime_error("capacity ignored"); });
        Check(disabled.Size() == 0);
        NpcLightManager<int, int, Light> crowd{34};
        auto frame = [&](int target) {
            crowd.BeginFrame();
            for (int i = 1; i <= 32; ++i) crowd.Submit(i, NpcLightSource::Selected, i, true, 1);
            crowd.Submit(target, NpcLightSource::Dialogue, target, true, 1);
            crowd.Update(0.1f, [](int) { return true; }, [](auto&...) {});
        };
        frame(100); frame(100); frame(101); frame(101); frame(102); frame(102);
        Check(crowd.Size() == 34);
        frame(1); // dialogue and selected overlap, oldest outgoing dialogue removed
        Check(crowd.Size() == 33);
        crowd.Clear();
        // Recruitment, selected override, dialogue override, and dismissal use one light.
        manager.Clear();
        const int baseline = Light::cleared;
        manager.BeginFrame(); manager.Submit(10, NpcLightSource::Follower, 30, false, 0); tick();
        Check(manager.Size() == 1 && rendered.at(10).first == 30);
        manager.BeginFrame();
        manager.Submit(10, NpcLightSource::Follower, 30, false, 0);
        manager.Submit(10, NpcLightSource::Selected, 40, false, 0);
        manager.Submit(10, NpcLightSource::Dialogue, 50, false, 0); tick();
        Check(manager.Size() == 1 && rendered.at(10).first == 50);
        manager.BeginFrame();
        manager.Submit(10, NpcLightSource::Follower, 30, false, 0);
        manager.Submit(10, NpcLightSource::Selected, 40, false, 0); tick();
        Check(manager.Size() == 1 && rendered.at(10).first == 40);
        manager.BeginFrame(); manager.Submit(10, NpcLightSource::Follower, 30, false, 0); tick();
        Check(rendered.at(10).first == 30 && Light::cleared == baseline);
        manager.BeginFrame(); tick();
        Check(manager.Size() == 0 && Light::cleared == baseline + 1);
        // A hard visibility veto removes even a higher-priority dialogue override immediately.
        manager.BeginFrame();
        manager.Submit(20, NpcLightSource::Follower, 30, true, 10);
        manager.Submit(20, NpcLightSource::Selected, 40, true, 10);
        manager.Submit(20, NpcLightSource::Dialogue, 50, true, 10);
        tick(0.1f);
        invalid.insert(20); tick(0);
        Check(manager.Size() == 0 && !rendered.contains(20));
        invalid.erase(20);
        manager.BeginFrame(); manager.Submit(20, NpcLightSource::Follower, 30, false, 0); tick();
        Check(manager.Size() == 1 && rendered.at(20).first == 30);
        manager.Clear();
        // A new dialogue must preempt active lower-priority entries at capacity.
        invalid.clear();
        manager.BeginFrame();
        manager.Submit(1, NpcLightSource::Follower, 1, false, 0);
        manager.Submit(2, NpcLightSource::Selected, 2, false, 0); tick();
        manager.Submit(3, NpcLightSource::Dialogue, 3, false, 0); tick();
        Check(rendered.contains(3) && rendered.contains(2) && !rendered.contains(1));
        // Protection hard-releases secondary lights, including outgoing fades.
        manager.Protect(1, [](int, int) { return true; }); tick();
        Check(manager.Size() == 1 && rendered.contains(3));
        manager.BeginFrame();
        manager.Submit(1, NpcLightSource::Follower, 1, false, 0);
        manager.Submit(2, NpcLightSource::Selected, 2, false, 0);
        manager.Protect(1, [](int, int) { return true; }); tick();
        Check(manager.Size() == 1 && rendered.contains(2));
        // Spatial veto does not change the producer's preference; next frame restores it.
        manager.Protect(1, [](int, int) { return false; }); tick();
        Check(manager.Size() == 0);
        manager.BeginFrame(); manager.Submit(2, NpcLightSource::Selected, 2, false, 0);
        manager.Protect(1, [](int, int) { return true; }); tick();
        Check(rendered.contains(2));
        // Dialogue wins even when its follower request fails the spatial test.
        manager.BeginFrame();
        manager.Submit(2, NpcLightSource::Follower, 2, false, 0);
        manager.Submit(2, NpcLightSource::Dialogue, 3, false, 0);
        manager.Protect(0, [](int, int) { return false; }); tick();
        Check(manager.Size() == 1 && rendered.at(2).first == 3);
        manager.BeginFrame(); manager.Protect(0, [](int, int) { return true; }); tick();
        Check(manager.Size() == 0);
        std::cout << "NPC source priority, preemption, protection budget, restoration and lifecycle passed.\n";
    } catch (const std::exception& e) { std::cerr << e.what(); return 1; }
}
