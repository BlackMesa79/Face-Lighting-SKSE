#pragma once
#include <RE/Skyrim.h>

namespace ActorRuntime {
    inline bool IsDead(const RE::Actor* actor) {
        // Keep explicit dispatch, matching the pinned upstream Actor.cpp:
        // SE/AE (including 1.7.x) slot 0x99; VR slot 0x9A.
        // This also preserves the earlier SE regression protection.
        return REL::RelocateVirtual<decltype(&RE::Actor::IsDead)>(0x99, 0x9A, actor, true);
    }
    inline bool SafeForLight(RE::Actor* actor) {
        if (!actor || actor->IsDeleted() || actor->IsDisabled()) return false;
        // ActorState's base offset varies by runtime; a C++ base upcast uses
        // the build-time layout and can read unrelated memory in the game.
        const auto life = actor->AsActorState()->GetLifeState();
        return life != RE::ACTOR_LIFE_STATE::kDying && life != RE::ACTOR_LIFE_STATE::kDead &&
            life != RE::ACTOR_LIFE_STATE::kRecycle && !IsDead(actor);
    }
    inline bool Eligible(RE::Actor* actor) {
        return actor && !actor->IsPlayerRef() && SafeForLight(actor);
    }
    inline bool SneakHidden(RE::Actor* actor, bool enabled) {
        const auto player = RE::PlayerCharacter::GetSingleton();
        return enabled && player && player->IsSneaking() && actor &&
            (actor == player || actor->IsPlayerTeammate());
    }
}
