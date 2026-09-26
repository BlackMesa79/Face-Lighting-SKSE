#pragma once
#include <cmath>
#include <cstdint>
#include <optional>

// A cached reading is usable for at most one second, and only in its original cell.
struct AmbientSample {
    std::optional<float> value;
    double time = 0;
    std::uint32_t cell = 0;
    std::optional<float> Read(double now, std::uint32_t currentCell) const {
        if (!value || !std::isfinite(*value) || *value < 0 || !cell || cell != currentCell ||
            !std::isfinite(now) || !std::isfinite(time) || now < time || now - time > 1.0) return {};
        return value;
    }
};
