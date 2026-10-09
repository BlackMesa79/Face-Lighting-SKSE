#include <SKSE/SKSE.h>
#include <RE/Skyrim.h>
#include "ActorRuntime.h"
#include <array>
#include <iostream>
#include <stdexcept>
#include <cstring>

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
    struct Layout { REL::Version version; std::size_t state; };
    const Layout layouts[] = {
        {{1, 5, 97, 0}, 0xB8}, {{1, 6, 353, 0}, 0xB8},
        {{1, 6, 629, 0}, 0xC0}, {{1, 6, 1170, 0}, 0xC0},
        {{1, 7, 99, 0}, 0xC0}, {{1, 7, 104, 0}, 0xC0}
    };
    for (const auto& layout : layouts) {
        if (!REL::Module::mock(layout.version)) return 1;
        std::array<std::uintptr_t, 0x200> vtable;
        vtable.fill(reinterpret_cast<std::uintptr_t>(&Unexpected));
        vtable[0x99] = reinterpret_cast<std::uintptr_t>(&Dead);
        // Multi-runtime C++ Actor is only a partial layout. Reserve the native
        // object footprint rather than writing runtime fields past sizeof(Actor).
        alignas(RE::Actor) std::array<std::byte, 0x300> storage{};
        *reinterpret_cast<std::uintptr_t**>(storage.data()) = vtable.data();
        auto* actor = reinterpret_cast<RE::Actor*>(storage.data());
        // Write native byte offsets, never the same C++ base fields being tested.
        const auto writeLife = [&](std::size_t offset, std::uint32_t life) {
            const std::uint32_t flags = life << 21;
            std::memcpy(storage.data() + offset, &flags, sizeof(flags));
        };
        dead = false; wrongSlot = false;
        if (Query(actor) || wrongSlot || ActorRuntime::SafeForLight(nullptr)) return 1;
        if (reinterpret_cast<const std::byte*>(actor->AsActorState()) != storage.data() + layout.state) return 1;
        // Poison both the old erroneous address and the other runtime's address.
        // Living actors must still pass even when those bytes look dead.
        for (const auto poison : {1u, 2u, 5u, 15u}) {
            writeLife(0xA8, poison);
            writeLife(layout.state == 0xB8 ? 0xC8 : 0xC0, poison);
            writeLife(layout.state + 8, 0);
            if (!ActorRuntime::SafeForLight(actor)) {
                std::cerr << "Living actor rejected for native offset " << layout.state << '\n';
                return 1;
            }
        }
        writeLife(0xA8, 0);
        for (const auto life : {1u, 2u, 5u}) {
            writeLife(layout.state + 8, life);
            if (ActorRuntime::SafeForLight(actor)) return 1;
        }
        writeLife(layout.state + 8, 0);
        dead = true;
        if (!Query(actor) || ActorRuntime::SafeForLight(actor) || wrongSlot) return 1;
    }
    std::cout << "SE 1.5.97, AE 1.6.353/629/1170 and 1.7.99/104 native life-state layouts, poisoned offsets and death dispatch passed.\n";
}
