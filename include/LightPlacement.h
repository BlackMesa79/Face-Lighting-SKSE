#pragma once
#include <RE/N/NiTransform.h>
#include <cmath>

namespace LightPlacement {
    // Skyrim heading is clockwise from +Y. X = right, Y = forward, Z = up.
    inline RE::NiPoint3 WorldOffset(float heading, const RE::NiPoint3& offset) {
        const auto sine = std::sin(heading);
        const auto cosine = std::cos(heading);
        return {cosine * offset.x + sine * offset.y,
            -sine * offset.x + cosine * offset.y, offset.z};
    }

    // Caller validates the head scale before inversion.
    inline RE::NiPoint3 HeadLocalOffset(const RE::NiTransform& head, const RE::NiPoint3& offset) {
        return head.rotate.Transpose() * (offset / head.scale);
    }

    // Bone mode uses the head's local axes, including animation pitch and roll.
    // Exclude skeleton scale so configured offsets remain in game units.
    inline RE::NiPoint3 LocalOffset(const RE::NiTransform& head, float heading,
        const RE::NiPoint3& offset, bool followHeadRotation) {
        return followHeadRotation ? offset / head.scale : HeadLocalOffset(head, WorldOffset(heading, offset));
    }
}
