#pragma once
#include <cmath>
#include <optional>

// One shared schedule for player and dialogue decisions. Scene fades and safety
// guards keep their normal frame cadence; this never asks the engine to recalculate.
struct AmbientPoll {
    static constexpr double Interval(int mode) { return mode == 1 ? 0.5 : mode == 2 ? 0.2 : 1.0; }
    // Scheduled ticks must not look like a pause. A much longer gap restarts
    // sustained-condition timers instead of counting missing observations.
    static constexpr double MaxDecisionGap(int mode) { return Interval(mode) * 2.5; }
    int mode = -1;
    std::optional<double> next, previous;
    bool Due(double now, bool active, int selectedMode = 0) {
        if (!active || !std::isfinite(now)) { *this = {}; return false; }
        if (mode != selectedMode || (previous && now < *previous)) next.reset();
        mode = selectedMode;
        previous = now;
        if (next && now < *next) return false;
        next = now + Interval(mode); // Skip missed ticks: no catch-up loop after a stall.
        return true;
    }
};
