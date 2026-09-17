#include "CSLighting.h"
#include "LightTransition.h"
#include "ColorTemperature.h"
#include "Hotkeys.h"
#include <iostream>
#include <stdexcept>
#include <algorithm>

namespace {
    void Check(bool value, const char* message) {
        if (!value) throw std::runtime_error(message);
    }
}

int main() {
    try {
        static_assert(Hotkeys::PadMatch(266, 0, 266, false));
        static_assert(Hotkeys::PadMatch(266, 274, 266, true));
        static_assert(!Hotkeys::PadMatch(266, 274, 266, false));
        static_assert(!Hotkeys::PadMatch(266, 274, 274, true));
        static_assert(!Hotkeys::PadMatch(0, 274, 266, true));
        static_assert(!Hotkeys::PadMatch(266, 266, 266, true));
        for (int code = 266; code <= 281; ++code)
            Check(Hotkeys::ValidPadCode(code) && Hotkeys::PadCode(Hotkeys::PadIndex(code)) == code,
                "all gamepad controls round-trip through menu");
        static_assert(Hotkeys::MatchesModifier(0, 0));
        static_assert(!Hotkeys::MatchesModifier(0, Hotkeys::ModifierMask(42)));
        static_assert(Hotkeys::MatchesModifier(1, Hotkeys::ModifierMask(42)));
        static_assert(Hotkeys::MatchesModifier(1, Hotkeys::ModifierMask(54)));
        static_assert(Hotkeys::MatchesModifier(2, Hotkeys::ModifierMask(29) | Hotkeys::ModifierMask(157)));
        static_assert(Hotkeys::MatchesModifier(3, Hotkeys::ModifierMask(184)));
        static_assert(!Hotkeys::MatchesModifier(1, 0));
        static_assert(!Hotkeys::MatchesModifier(1, Hotkeys::ModifierMask(42) | Hotkeys::ModifierMask(29)));
        for (float intensity : {0.00001f, 0.01f, 0.1f, 1.0f, 5.0f}) {
            for (float radius : {10.0f, 40.0f, 100.0f, 252.0f, 500.0f}) {
                const float fade = CSLighting::InverseFade(intensity);
                const auto result = CSLighting::InverseRange(fade, true, radius);
                Check(std::isfinite(result.cutoff) && result.cutoff > 0 && result.cutoff != 1,
                    "manual cutoff is valid and avoids CS sentinel");
                const float csRadius = std::sqrt(CSLighting::scaledUnitsSquared *
                    ((2 * (fade * 4) - result.cutoff * CSLighting::sourceSize * CSLighting::sourceSize) / (2 * result.cutoff)));
                Check(std::abs(csRadius - radius) < 0.01f, "CS maintains manual radius across intensities");
                Check(result.radius == csRadius, "manual UI and renderer ranges agree");
            }
        }
        const float sentinelFade = (10000.0f / CSLighting::scaledUnitsSquared +
            CSLighting::sourceSize * CSLighting::sourceSize / 2) / 4;
        const auto sentinel = CSLighting::InverseRange(sentinelFade, true, 100);
        Check(sentinel.cutoff != 1 && std::abs(sentinel.radius - 100) < 0.01f, "cutoff one sentinel avoided");
        for (bool loaded : {false, true}) for (bool recognized : {false, true}) {
            Check(CSLighting::ModeEnabled(0, loaded, recognized) == (loaded && recognized), "automatic requires loaded recognized CS");
            Check(CSLighting::ModeEnabled(1, loaded, recognized) == loaded, "manual bypasses recognition only");
            Check(!CSLighting::ModeEnabled(2, loaded, recognized), "disabled never writes CS protocol");
        }
        static_assert(Hotkeys::Index(59) == 27);
        static_assert(Hotkeys::FromIndex(0, 59) == 0);
        static_assert(Hotkeys::FromIndex(27, 59) == 59);
        static_assert(Hotkeys::FromIndex(Hotkeys::Index(38), 59) == 38);
        static_assert(Hotkeys::Index(255) == Hotkeys::codes.size());
        static_assert(Hotkeys::FromIndex(Hotkeys::Index(255), 255) == 255);
        static_assert(Hotkeys::FromIndex(Hotkeys::Index(88), 0) == 88); // F12
        static_assert(Hotkeys::FromIndex(Hotkeys::Index(210), 0) == 210); // Insert
        static_assert(Hotkeys::FromIndex(Hotkeys::Index(156), 0) == 156); // Num Enter
        for (std::size_t i = 0; i < Hotkeys::codes.size(); ++i) {
            Check(Hotkeys::Index(Hotkeys::codes[i]) == i, "keyboard codes unique and menu round-trip");
            Check(Hotkeys::FromIndex(static_cast<int>(i), 255) == Hotkeys::codes[i], "keyboard selection round-trip");
        }
        for (bool available : {false, true})
            for (bool global : {false, true})
                for (bool inverse : {false, true})
                    for (bool flag : {false, true}) {
                        const bool useLinear = ColorTemperature::UseLinearColor(available, global, inverse, flag);
                        const auto color = ColorTemperature::Get(3200, useLinear);
                        if (!available || !global || (!inverse && !flag))
                            Check(std::abs(color.g - 0.7226f) < 0.001f, "nonlinear path preserves encoded color");
                        else Check(std::abs(color.g - 0.48085f) < 0.001f, "confirmed linear path converts color");
                    }
        for (bool linear : {false, true}) {
            const auto white = ColorTemperature::Get(6500, linear);
            Check(white.r == 1 && white.g == 1 && white.b == 1, "default temperature preserves white");
            const auto warm = ColorTemperature::Get(2000, linear);
            const auto cool = ColorTemperature::Get(10000, linear);
            Check(warm.r > warm.b && cool.b > cool.r, "warm and cool temperature direction");
            for (float k : {0.0f, 2000.0f, 6500.0f, 10000.0f, 20000.0f}) {
                const auto rgb = ColorTemperature::Get(k, linear);
                for (float c : {rgb.r, rgb.g, rgb.b})
                    Check(std::isfinite(c) && c >= 0 && c <= 1, "color output finite and bounded");
            }
        }
        LightTransition fade;
        fade.Update(true, true, 0.3f, 0);
        fade.Update(true, true, 0.3f, 0.15f);
        Check(std::abs(fade.value - 0.5f) < 0.001f, "fade reaches midpoint");
        fade.Update(false, true, 0.3f, 0.01f);
        Check(std::abs(fade.value - 0.5f) < 0.001f, "reversing fade preserves brightness");
        fade.Update(false, true, 0.3f, 0.3f);
        Check(fade.value == 0, "fade out completes exactly for cleanup");
        fade.Update(true, false, 0.3f, 0);
        Check(fade.value == 1, "disabled transition switches on immediately");
        fade.Update(false, true, 0, 0);
        Check(fade.value == 0, "zero duration switches off immediately");
        Check(CSLighting::SupportsVersion(1, 6, 0), "1.6 source family");
        Check(CSLighting::SupportsVersion(1, 8, 4), "public 1.8.4 source family");
        Check(!CSLighting::SupportsVersion(1, 9, 0) && !CSLighting::SupportsVersion(2, 6, 0), "unknown versions not accepted");
        const std::uint32_t external = (1u << 9) | (1u << 24) | 1u;
        auto bits = CSLighting::Flags(external, true, true);
        Check((bits & external) == external, "preserve CS disabled/culling/portal bits");
        bits = CSLighting::Flags(bits, false, false);
        Check(bits == (external | CSLighting::initialized), "switch modes back without erasing external bits");
        bits = CSLighting::Flags(external, true, false);
        Check((bits & CSLighting::linear) != 0 && (bits & external) == external,
            "inverse enforces linear even with an old INI's linear preference off");
        Check(CSLighting::Flags(bits, false, true) == (external | CSLighting::initialized | CSLighting::linear),
            "regular mode retains its linear preference");
        const float balancedFade = CSLighting::InverseFade(1.0f);
        const float balancedRadius = CSLighting::InverseRange(balancedFade).radius;
        const float distance = std::sqrt(1625.0f);
        const float t = std::clamp((balancedRadius - distance) /
            (balancedRadius * std::clamp(252.0f / balancedRadius, 0.0f, 1.0f)), 0.0f, 1.0f);
        const float inverseContribution = balancedFade * 4.0f * 3920.0f /
            (1625.0f + 3920.0f) * t * t * (3.0f - 2.0f * t);
        Check(std::abs(inverseContribution - 0.8375f) < 0.0001f,
            "calibrated CS attenuation matches linear regular reference including range fadeout");
        Check(CSLighting::InverseFade(0) == 0 && CSLighting::InverseFade(5) > balancedFade,
            "calibration preserves off and increasing intensity");
        const auto normal = CSLighting::InverseRange(1.0f);
        Check(std::abs(normal.radius - std::sqrt(309680.0f)) < 0.01f, "reference radius at intensity 1");
        Check(CSLighting::InverseRange(2).radius > normal.radius, "higher intensity expands inverse range");
        // The CS formula can yield zero or NaN near its minimum intensity.
        const float boundary = 0.05f * CSLighting::sourceSize * CSLighting::sourceSize / 8.0f;
        for (const float fade : {0.0f, 0.00001f, 0.01f, boundary,
                 std::nextafter(boundary, 0.0f), std::nextafter(boundary, 1.0f), 1.0f, 5.0f}) {
            const auto result = CSLighting::InverseRange(fade);
            const float squared = CSLighting::scaledUnitsSquared *
                ((2 * (fade * 4) - result.cutoff * CSLighting::sourceSize * CSLighting::sourceSize) / (2 * result.cutoff));
            const float csResult = squared < 0 ? 1.0f : std::sqrt(squared);
            Check(std::isfinite(csResult) && csResult > 0, "CS receives a positive finite radius, including boundary");
            Check(result.radius == csResult, "UI range and CS calculation agree");
        }
        std::cout << "CS protocol flags, falloff, low-intensity boundaries and version checks passed.\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
