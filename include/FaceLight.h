#pragma once
#include <RE/Skyrim.h>
#include <cstdint>

namespace FaceLight {
    void Install();
    void SetGameActive(bool active);
    // UI/render callbacks queue work rather than touching the scene graph.
    void RequestUpdate();
    struct Context { std::uint64_t session; bool ready; bool gameThread; };
    Context GetContext();
    std::uint32_t RuntimeFlags(RE::Actor* actor); // Main-thread query, no scene mutation.
}
