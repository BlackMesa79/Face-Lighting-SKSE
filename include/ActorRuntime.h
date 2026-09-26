#pragma once
#include <RE/Skyrim.h>

namespace ActorRuntime {
    inline bool IsDead(const RE::Actor* actor) {
        // CommonLibVR's TESObjectREFR currently declares an extra Unk_8C slot
        // even in flat builds. Native actor->IsDead() consequently dispatches
        // past the SE/AE engine's 0x99 slot. Use the explicit indices from
        // CommonLibVR/src/RE/A/Actor.cpp instead of the C++ declaration order.
        return REL::RelocateVirtual<decltype(&RE::Actor::IsDead)>(0x99, 0x9A, actor, true);
    }
    inline bool SafeForLight(RE::Actor* actor) {
        if (!actor || actor->IsDeleted() || actor->IsDisabled()) return false;
        const auto life = actor->GetLifeState();
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
