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
        // Recruitment, follower priority, dialogue override, and dismissal use one light.
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
        Check(manager.Size() == 1 && rendered.at(10).first == 30);
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
        Check(rendered.contains(3) && rendered.contains(1) && !rendered.contains(2));
        // Dialogue suppresses other NPCs without changing their producer preferences.
        manager.Protect(1); tick();
        Check(manager.Size() == 1 && rendered.contains(3));
        manager.BeginFrame();
        manager.Submit(1, NpcLightSource::Follower, 1, false, 0);
        manager.Submit(2, NpcLightSource::Selected, 2, false, 0);
        manager.Protect(1); tick();
        Check(manager.Size() == 1 && rendered.contains(1));
        manager.BeginFrame(); manager.Submit(1, NpcLightSource::Follower, 1, false, 0);
        manager.Protect(1); tick();
        Check(rendered.contains(1));
        // A dialogue/follower overlap consumes no secondary slot.
        manager.BeginFrame();
        manager.Submit(2, NpcLightSource::Follower, 2, false, 0);
        manager.Submit(2, NpcLightSource::Dialogue, 3, false, 0);
        manager.Protect(0); tick();
        Check(manager.Size() == 1 && rendered.at(2).first == 3);
        manager.BeginFrame(); manager.Protect(0); tick();
        Check(manager.Size() == 0);
        // Secondary lights suspend during dialogue and restore when it ends.
        NpcLightManager<int, int, Light> party{8};
        auto partyFrame = [&](int target) {
            rendered.clear(); party.BeginFrame();
            for (int i = 1; i <= 6; ++i) party.Submit(i, NpcLightSource::Follower, i, true, 1);
            if (target) party.Submit(target, NpcLightSource::Dialogue, target, true, 1);
            party.Update(0.1f, [](int actor) { return actor != 1; },
                [&](Light&, int actor, int parameter, float opacity) { rendered[actor] = {parameter, opacity}; }, 4);
            for (int i = 2; i <= 5; ++i) Check(rendered.contains(i) == !target);
            Check(!rendered.contains(1) && !rendered.contains(6));
            Check(party.Size() == (target ? 1 : 4));
            if (target) Check(rendered.contains(target));
        };
        partyFrame(0); partyFrame(100); partyFrame(101); partyFrame(0);
        // An open dialogue with no valid speaker still pauses all secondary lights.
        party.BeginFrame(); party.Submit(2, NpcLightSource::Follower, 2, true, 1);
        party.Update(0.1f, [](int) { return true; }, [](auto&...) {}, 4, true);
        Check(party.Size() == 0);
        partyFrame(0);

        // Runtime budget changes must reclaim live/fading entries, retain priority,
        // allow more than four lights and preserve dialogue exclusivity at any limit.
        NpcLightManager<int, int, Light> configurable{66};
        auto budgetFrame = [&](std::size_t limit, int speaker = 0, bool dialogueOpen = false) {
            rendered.clear(); configurable.BeginFrame();
            for (int i = 1; i <= 32; ++i)
                configurable.Submit(i, NpcLightSource::Selected, i, true, 1);
            for (int i = 33; i <= 40; ++i)
                configurable.Submit(i, NpcLightSource::Follower, i, true, 1);
            if (speaker) configurable.Submit(speaker, NpcLightSource::Dialogue, speaker, true, 1);
            configurable.Update(0.1f, [](int) { return true; },
                [&](Light&, int actor, int parameter, float opacity) { rendered[actor] = {parameter, opacity}; },
                limit, dialogueOpen);
        };
        budgetFrame(4);
        Check(configurable.Size() == 4 && rendered.contains(33) && !rendered.contains(1));
        budgetFrame(12);
        Check(configurable.Size() == 12 && rendered.contains(40) && rendered.contains(4) && !rendered.contains(5));
        budgetFrame(1);
        Check(configurable.Size() == 1 && rendered.contains(33));
        budgetFrame(32);
        Check(configurable.Size() == 32 && rendered.contains(24) && !rendered.contains(25));
        budgetFrame(32, 100, true);
        Check(configurable.Size() == 1 && rendered.contains(100));
        budgetFrame(32, 0, true);
        Check(configurable.Size() == 0);
        budgetFrame(12);
        Check(configurable.Size() == 12 && rendered.contains(40) && rendered.contains(4));
        // Let fade-in advance before testing a live outgoing fade.
        budgetFrame(12);
        // A reduced budget also counts outgoing fades instead of leaving extra lights alive.
        configurable.BeginFrame(); rendered.clear();
        configurable.Update(0.1f, [](int) { return true; },
            [&](Light&, int actor, int parameter, float opacity) { rendered[actor] = {parameter, opacity}; }, 1);
        Check(configurable.Size() == 1 && rendered.size() == 1);

        // Environment suppression retains only the current dialogue source for its
        // configured fade; no follower/selected fallback or old speaker survives.
        NpcLightManager<int, int, Light> ambientDialogue{8};
        const int currentSpeaker = 77;
        ambientDialogue.BeginFrame();
        ambientDialogue.Submit(currentSpeaker, NpcLightSource::Dialogue, 77, true, 0.3f);
        ambientDialogue.Update(0.1f, [](int) { return true; }, [](auto&...) {}, 4, true);
        ambientDialogue.Update(0.1f, [](int) { return true; }, [](auto&...) {}, 4, true);
        Check(ambientDialogue.Opacity(currentSpeaker) > 0);
        ambientDialogue.BeginFrame(); rendered.clear();
        ambientDialogue.Update(0.1f, [](int) { return true; },
            [&](Light&, int actor, int parameter, float opacity) { rendered[actor] = {parameter, opacity}; },
            4, true, &currentSpeaker);
        Check(ambientDialogue.Size() == 1 && rendered.contains(currentSpeaker));
        for (int i = 0; i < 5; ++i)
            ambientDialogue.Update(0.1f, [](int) { return true; }, [](auto&...) {}, 4, true, &currentSpeaker);
        Check(ambientDialogue.Size() == 0);
        ambientDialogue.BeginFrame();
        ambientDialogue.Submit(99, NpcLightSource::Dialogue, 99, false, 0);
        ambientDialogue.Update(0.1f, [](int) { return true; }, [](auto&...) {}, 4, true);
        ambientDialogue.BeginFrame();
        ambientDialogue.Update(0.1f, [](int) { return true; }, [](auto&...) {}, 4, true, &currentSpeaker);
        Check(ambientDialogue.Size() == 0); // Previous speaker must not fade beside new bright speaker.
        ambientDialogue.BeginFrame();
        ambientDialogue.Submit(currentSpeaker, NpcLightSource::Follower, 77, true, 1);
        ambientDialogue.Update(0.1f, [](int) { return true; }, [](auto&...) {}, 4);
        ambientDialogue.BeginFrame();
        ambientDialogue.Update(0.1f, [](int) { return true; }, [](auto&...) {}, 4, true, &currentSpeaker);
        Check(ambientDialogue.Size() == 0); // A prior personal source cannot be kept as a dialogue fade.
        ambientDialogue.BeginFrame();
        ambientDialogue.Submit(currentSpeaker, NpcLightSource::Follower, 77, false, 0);
        ambientDialogue.Submit(2, NpcLightSource::Selected, 2, false, 0);
        ambientDialogue.Update(0.1f, [](int) { return true; }, [](auto&...) {}, 4);
        Check(ambientDialogue.Size() == 2); // Personal sources recover after dialogue, preferences intact.

        std::cout << "NPC source priority, preemption, protection budget, restoration and lifecycle passed.\n";
    } catch (const std::exception& e) { std::cerr << e.what(); return 1; }
}
