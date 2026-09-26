#include <SKSE/SKSE.h>
#include <RE/Skyrim.h>
#include "ActorRuntime.h"
#include <array>
#include <iostream>
#include <stdexcept>

namespace {
    bool dead = false;
    bool wrongSlot = false;
    bool Dead(const RE::Actor*, bool notEssential) {
        if (!notEssential) throw std::runtime_error("Wrong IsDead argument");
        return dead;
    }
    bool Unexpected(const RE::Actor*, bool) { wrongSlot = true; return true; }
    __declspec(noinline) bool Query(const RE::Actor* actor) { return ActorRuntime::IsDead(actor); }
}

int main() {
    // ABI fixture: use the SE/AE engine's vtable layout without loading the game.
    std::array<std::uintptr_t, 0x200> vtable;
    vtable.fill(reinterpret_cast<std::uintptr_t>(&Unexpected));
    vtable[0x99] = reinterpret_cast<std::uintptr_t>(&Dead);
    alignas(RE::Actor) std::array<std::byte, sizeof(RE::Actor)> storage{};
    *reinterpret_cast<std::uintptr_t**>(storage.data()) = vtable.data();
    const auto actor = reinterpret_cast<const RE::Actor*>(storage.data());
    if (Query(actor) || wrongSlot) {
        std::cerr << "Living NPC rejected: IsDead did not dispatch to SE/AE slot 0x99.\n";
        return 1;
    }
    dead = true;
    if (!Query(actor) || wrongSlot) return 1;
    auto* mutableActor = const_cast<RE::Actor*>(actor);
    if (ActorRuntime::SafeForLight(nullptr) || ActorRuntime::SafeForLight(mutableActor)) return 1;
    dead = false;
    for (auto life : {RE::ACTOR_LIFE_STATE::kDying, RE::ACTOR_LIFE_STATE::kDead, RE::ACTOR_LIFE_STATE::kRecycle}) {
        mutableActor->actorState1.lifeState = life;
        if (ActorRuntime::SafeForLight(mutableActor)) return 1;
    }
    mutableActor->actorState1.lifeState = RE::ACTOR_LIFE_STATE::kAlive;
    if (!ActorRuntime::SafeForLight(mutableActor)) return 1;
    std::cout << "Actor death check dispatches to SE/AE slot 0x99 for living and dead NPCs.\n";
}
