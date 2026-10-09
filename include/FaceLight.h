#pragma once
#include <RE/Skyrim.h>
#include <cstdint>
#include "FaceLightingAPIV2.h"

namespace FaceLight {
    void Install();
    void SetGameActive(bool active);
    // UI/render callbacks queue work rather than touching the scene graph.
    void RequestUpdate();
    struct Context { std::uint64_t session; bool ready; bool gameThread; };
    Context GetContext();
    std::uint32_t RuntimeFlags(RE::Actor* actor); // Main-thread query, no scene mutation.
    FaceLightingAPI::Result BeginTemporary(const FaceLightingAPI::V2::BeginInfo&, FaceLightingAPI::V2::Session&);
    FaceLightingAPI::Result UpdateTemporary(const FaceLightingAPI::V2::UpdateInfo&);
    FaceLightingAPI::Result RenewTemporary(const FaceLightingAPI::V2::Session&);
    FaceLightingAPI::Result EndTemporary(const FaceLightingAPI::V2::Session&);
    FaceLightingAPI::Result QueryTemporary(const FaceLightingAPI::V2::Session&, FaceLightingAPI::V2::SessionState&);
}
