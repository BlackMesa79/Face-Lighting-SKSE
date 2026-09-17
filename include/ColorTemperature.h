#pragma once
#include <algorithm>
#include <cmath>

namespace ColorTemperature {
    constexpr bool UseLinearColor(bool csAvailable, bool globalLinear, bool inverse, bool linearFlag) {
        return csAvailable && globalLinear && (inverse || linearFlag);
    }
    struct RGB { float r, g, b; };
    // Approximation based on Tanner Helland's temperature-to-sRGB fit:
    // https://tannerhelland.com/2012/09/18/convert-temperature-rgb-algorithm-code.html
    inline RGB Approximate(float kelvin) {
        const float t = kelvin / 100.0f;
        const auto unit = [](float x) { return std::clamp(x / 255.0f, 0.0f, 1.0f); };
        return {unit(t <= 66 ? 255 : 329.698727446f * std::pow(t - 60, -0.1332047592f)),
            unit(t <= 66 ? 99.4708025861f * std::log(t) - 161.1195681661f :
                288.1221695283f * std::pow(t - 60, -0.0755148492f)),
            unit(t >= 66 ? 255 : 138.5177312231f * std::log(t - 10) - 305.0447927307f)};
    }
    inline RGB Get(float kelvin, bool linear) {
        kelvin = std::isfinite(kelvin) ? std::clamp(kelvin, 2000.0f, 10000.0f) : 6500.0f;
        auto rgb = Approximate(kelvin);
        const auto white = Approximate(6500);
        // Neutralize the default to exactly the previous white light.
        const auto channel = [linear](float c, float w) {
            c = std::clamp(c / w, 0.0f, 1.0f);
            return linear ? (c <= 0.04045f ? c / 12.92f : std::pow((c + 0.055f) / 1.055f, 2.4f)) : c;
        };
        return {channel(rgb.r, white.r), channel(rgb.g, white.g), channel(rgb.b, white.b)};
    }
}
