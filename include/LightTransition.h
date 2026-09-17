#pragma once
#include <algorithm>

// A reversible envelope: restarting a transition starts at the current value.
struct LightTransition {
    float value = 0, start = 0, target = 0, elapsed = 0, duration = 0;
    void Update(bool visible, bool enabled, float seconds, float delta) {
        const float desired = visible ? 1.0f : 0.0f;
        if (!enabled || seconds <= 0) {
            value = start = target = desired;
            elapsed = duration = 0;
            return;
        }
        if (target != desired || duration != seconds) {
            start = value;
            target = desired;
            elapsed = 0;
            duration = seconds;
        } else {
            elapsed += std::max(delta, 0.0f);
        }
        const float t = std::clamp(elapsed / duration, 0.0f, 1.0f);
        value = start + (target - start) * t * t * (3 - 2 * t);
    }
};
