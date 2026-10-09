#pragma once
// Copyright (C) 2026 BlackMesa79. SPDX-License-Identifier: GPL-3.0-only
#include "FaceLightingAPI.h"

namespace FaceLightingAPI::V2 {
    inline constexpr std::uint32_t version = 2;
    inline constexpr std::uint32_t maxTargets = 4;
    enum Capability : std::uint32_t { TemporarySessions = 1, RGBColor = 2, EnvironmentQuery = 4 };
    enum class ColorMode : std::uint32_t { Temperature, SRGB };
    enum class OffsetSpace : std::uint32_t { ActorHeading, HeadBone };
    struct Session { std::uint64_t world = 0, id = 0; };
    struct BeginInfo {
        std::uint32_t structSize = sizeof(BeginInfo), reserved = 0;
        std::uint64_t world = 0;
        float leaseSeconds = 5.0f; // [1, 30], monotonic wall time, including pauses.
        std::uint32_t padding = 0;
    };
    struct LightParameters {
        std::uint32_t structSize = sizeof(LightParameters), enabled = 1;
        ColorMode colorMode = ColorMode::Temperature;
        OffsetSpace offsetSpace = OffsetSpace::ActorHeading;
        float intensity = 1.0f, radius = 100.0f, temperature = 6500.0f;
        float red = 1.0f, green = 1.0f, blue = 1.0f; // sRGB [0,1], converted if user CS linear settings require it.
        float offsetX = 0.0f, offsetY = 40.0f, offsetZ = 5.0f;
        float transitionSeconds = 0.2f; // [0, 3]; zero disables fading.
        std::uint32_t reserved = 0;
    };
    struct Target {
        Token actor;
        LightParameters light;
    };
    struct UpdateInfo {
        std::uint32_t structSize = sizeof(UpdateInfo), count = 0;
        Session session;
        std::uint32_t paused = 0, reserved = 0;
        const Target* targets = nullptr; // Full replacement; caller memory copied before return.
    };
    struct SessionState {
        std::uint32_t structSize = sizeof(SessionState), paused = 0;
        Session session;
        std::uint32_t count = 0, reserved = 0;
        float remainingSeconds = 0.0f;
        std::uint32_t padding = 0;
    };
    enum EnvironmentFlag : std::uint32_t { RawValid = 1, FilteredValid = 2 };
    struct Environment {
        std::uint32_t structSize = sizeof(Environment), flags = 0;
        std::uint64_t world = 0;
        float raw = 0, filtered = 0; // Player-position samples; raw freshness unknown.
        float x = 0, y = 0, z = 0;
        std::uint32_t cellID = 0;
    };
    struct Interface {
        std::uint32_t structSize, apiVersion, capabilities, reserved;
        const FaceLightingAPI::Interface* v1; // Original V1 table; never extended in place.
        Result (*ResolveActor)(std::uint32_t formID, ActorState*) noexcept; // Includes player; never loads cells.
        Result (*BeginSession)(const BeginInfo*, Session*) noexcept;
        Result (*UpdateSession)(const UpdateInfo*) noexcept;
        Result (*RenewSession)(const Session*) noexcept;
        Result (*EndSession)(const Session*) noexcept;
        Result (*QuerySession)(const Session*, SessionState*) noexcept;
        Result (*GetEnvironment)(Environment*) noexcept;
    };
    using GetAPI = const Interface* (*)(std::uint32_t requestedVersion) noexcept;
    static_assert(sizeof(Session) == 16 && sizeof(BeginInfo) == 24);
    static_assert(sizeof(LightParameters) == 60 && sizeof(Target) == 80);
    static_assert(sizeof(UpdateInfo) == 40 && sizeof(SessionState) == 40 && sizeof(Environment) == 40);
    static_assert(sizeof(Interface) == 80 && std::is_standard_layout_v<Interface>);
    static_assert(std::is_trivially_copyable_v<Target>);
}
