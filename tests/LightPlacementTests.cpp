#include <SKSE/SKSE.h>
#include "LightPlacement.h"
#include <iostream>
#include <numbers>

namespace {
    bool Near(const RE::NiPoint3& a, const RE::NiPoint3& b) {
        return std::abs(a.x - b.x) < 0.001f && std::abs(a.y - b.y) < 0.001f && std::abs(a.z - b.z) < 0.001f;
    }
}

int main() {
    constexpr auto pi = std::numbers::pi_v<float>;
    if (!Near(LightPlacement::WorldOffset(0, {10, 40, 5}), {10, 40, 5})) return 1;
    if (!Near(LightPlacement::WorldOffset(pi / 2, {10, 40, 5}), {40, -10, 5})) return 2;
    if (!Near(LightPlacement::WorldOffset(pi, {10, 40, 5}), {-10, -40, 5})) return 3;
    if (!Near(LightPlacement::WorldOffset(-pi / 2, {10, 40, 5}), {-40, 10, 5})) return 4;
    // Reconstruct world coordinates through rotated/scaled head bones. The
    // resulting offset must stay in game units independent of skeleton scale.
    for (const float scale : {0.5f, 1.0f, 2.0f}) {
        RE::NiTransform head;
        head.translate = {1000.0f, -200.0f, 80.0f};
        head.scale = scale;
        head.rotate.SetEulerAnglesXYZ(0.3f, -0.4f, 0.8f);
        const RE::NiPoint3 offset{-12.0f, 40.0f, 8.0f};
        if (!Near(head * LightPlacement::HeadLocalOffset(head, offset), head.translate + offset)) return 5;
        if (!Near(head * LightPlacement::LocalOffset(head, 0, offset, true),
            head.translate + head.rotate * offset)) return 6;
        if (!Near(head * LightPlacement::LocalOffset(head, pi / 2, offset, false),
            head.translate + LightPlacement::WorldOffset(pi / 2, offset))) return 7;
        // Animation turns the head 180 degrees while actor heading remains zero.
        head.rotate.SetEulerAnglesXYZ(0, 0, pi);
        if (!Near(head * LightPlacement::LocalOffset(head, 0, {0, 40, 5}, true),
            head.translate + RE::NiPoint3{0, -40, 5})) return 8;
        if (!Near(head * LightPlacement::LocalOffset(head, 0, {0, 40, 5}, false),
            head.translate + RE::NiPoint3{0, 40, 5})) return 9;
    }
    std::cout << "Placement: headings, animated bone rotation, legacy mode and scale independence passed.\n";
}
