#include "NpcLightManager.h"
#include <iostream>
#include <map>
#include <stdexcept>
#include <set>

struct Light {
    static inline int cleared = 0;
    void Clear() { ++cleared; }
};
void Check(bool value) { if (!value) throw std::runtime_error("NPC light manager failed"); }
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
        std::cout << "NPC deduplication, source fallback, fade reversal, capacity, invalidation and reset passed.\n";
    } catch (const std::exception& e) { std::cerr << e.what(); return 1; }
}
