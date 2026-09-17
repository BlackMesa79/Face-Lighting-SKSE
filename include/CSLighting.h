#pragma once
#include <string_view>
#include <bit>
#include <cmath>
#include <cstdint>
#include <limits>
#include <numbers>
#include <string>

namespace CSLighting {
    // Shared protocol checked against CS v1.6.0, public v1.8.4 and Jiaye dev.
    // Fixed commits and the distinction from the installed AIO are in docs/CS-compatibility.md.
    inline constexpr std::uint32_t initialized = 1u << 8;
    inline constexpr std::uint32_t inverseSquare = 1u << 10;
    inline constexpr std::uint32_t linear = 1u << 11;
    inline constexpr float sourceSize = std::numbers::sqrt2_v<float>;
    inline constexpr float scaledUnitsSquared = 0.8f * 70.0f * 70.0f;

    // Match unit-intensity, linear regular lighting at the default head offset
    // (0, 40, 5), radius 100. This is a fixed reference, not exposure tracking.
    // Legacy CS point-light multipliers and postprocessing are not included.
    inline constexpr float referenceDistanceSquared = 40.0f * 40.0f + 5.0f * 5.0f;
    inline constexpr float inverseIntensityScale =
        (1.0f - referenceDistanceSquared / (100.0f * 100.0f)) *
        (referenceDistanceSquared + scaledUnitsSquared * sourceSize * sourceSize / 2.0f) /
        (4.0f * scaledUnitsSquared);

    constexpr float InverseFade(float intensity) { return intensity * inverseIntensityScale; }

    constexpr bool SupportsVersion(unsigned major, unsigned minor, unsigned patch) {
        return major == 1 && ((minor == 6 && patch == 0) || (minor == 8 && patch == 4));
    }

    struct Range {
        float radius;
        float cutoff;
    };
    inline Range InverseRange(float fade, bool manual = false, float radius = 100.0f) {
        float cutoff = 0.05f;
        if (manual && fade > 0) {
            // Solve CS's radius formula for cutoff, retaining the chosen intensity.
            cutoff = static_cast<float>((4.0 * fade) /
                (static_cast<double>(radius) * radius / scaledUnitsSquared +
                    static_cast<double>(sourceSize) * sourceSize / 2.0));
            // CS treats exactly 1 as a request for its default cutoff.
            if (cutoff == 1.0f) cutoff = std::nextafter(1.0f, 0.0f);
        }
        const auto squared = [&](float threshold) {
            return scaledUnitsSquared * ((2.0f * (fade * 4.0f) - threshold * sourceSize * sourceSize) / (2.0f * threshold));
        };
        // CS falls back to radius 1 for a negative radicand, but not zero.
        // Avoid its subsequent division by zero at the exact boundary.
        // One ULP is insufficient because the products may round back to equality.
        if (squared(cutoff) == 0.0f) cutoff = 0.0501f;
        const float value = squared(cutoff);
        return {value < 0.0f ? 1.0f : std::sqrt(value), cutoff};
    }

    inline std::uint32_t Flags(std::uint32_t existing, bool inverse, bool linearIntensity) {
        return (existing & ~(inverseSquare | linear)) | initialized |
            (inverse ? inverseSquare : 0u) | ((inverse || linearIntensity) ? linear : 0u);
    }

    void Detect();
    constexpr bool ModeEnabled(int mode, bool loaded, bool recognized) {
        return loaded && (mode == 1 || (mode == 0 && recognized));
    }
    bool Available(int mode = 0);
    const char* Status(std::string_view language = "en", int mode = 0);
    std::string Diagnostics(std::string_view language = "en");
}


