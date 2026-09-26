#pragma once
#include <algorithm>
#include <cmath>
#include <optional>

// Pure control policy. Times are monotonic seconds; unknown samples never mean dark.
struct AmbientPolicy {
    bool on = false;
    std::optional<double> pending;
    double ready = 0;
    std::optional<bool> observedLight;
    float compensation = 0, estimated = 0;
    bool valid = false;
    std::optional<double> lastTick;

    void Suspend(double now) { pending.reset(); ready = now + 2.0; valid = false; }
    void Step(double now, std::optional<float> raw, bool actualLight, float offset,
        float low, float high, float delay, bool active) {
        // A long gap may be a pause/loading interval, not sustained darkness.
        if (lastTick && now - *lastTick > 0.5) Suspend(now);
        lastTick = now;
        if (!observedLight || *observedLight != actualLight) {
            observedLight = actualLight;
            Suspend(now); // Let the engine cache catch up after light creation/removal.
        }
        compensation = actualLight ? offset : 0.0f;
        valid = raw && std::isfinite(*raw);
        if (valid) estimated = std::max(0.0f, *raw - compensation);
        if (!active || !valid || now < ready) { pending.reset(); return; }
        const bool request = on ? estimated > high : estimated < low;
        if (!request) { pending.reset(); return; }
        if (!pending) pending = now;
        if (now - *pending >= delay) { on = !on; Suspend(now); }
    }
};
