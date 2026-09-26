#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <optional>

// Float accumulation intentionally follows the game's per-light addition order.
struct ExclusionTotals {
    float all = 0, external = 0, player = 0, npc = 0;
    std::array<std::uint32_t, 3> count{};
    bool valid = true;
    void Add(unsigned source, float value) {
        if (source > 2 || !std::isfinite(value) || value < 0) { valid = false; return; }
        ++count[source];
        all += value;
        if (source == 0) external += value;
        else if (source == 1) player += value;
        else npc += value;
        if (!std::isfinite(all)) valid = false;
    }
    static bool Close(float a, float b) {
        return std::isfinite(a) && std::isfinite(b) &&
            std::abs(a - b) <= 0.00001f * (std::max)(1.0f, std::abs(b));
    }
    std::optional<float> Filter(float query, float finalRaw, float cachedRaw, float scale) const {
        if (!valid || !Close(all, query) || !Close(cachedRaw, finalRaw) ||
            !std::isfinite(finalRaw) || finalRaw < 0 || scale != 100.0f) return {};
        // Preserve the final query's environment/directional terms. No second engine call.
        const double value = double(finalRaw) + (double(external) - double(query)) * scale;
        if (!std::isfinite(value) || value < -0.01) return {};
        return static_cast<float>((std::max)(0.0, value));
    }
};
