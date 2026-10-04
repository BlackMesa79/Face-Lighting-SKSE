#pragma once
#include "AmbientPolicy.h"
#include "AmbientPoll.h"
#include <cstdint>

// Independent dialogue gate. Unknown/bright startup stays off; only sustained
// darkness turns it on. A new speaker or settings change starts a new decision.
struct DialogueAmbientPolicy {
    AmbientPolicy policy;
    std::uint64_t subject = 0;
    int mode = 0, polling = 0;
    std::uint32_t cell = 0;
    float low = 0, high = 0, delay = 0, offset = 0;
    bool Update(double now, std::uint64_t target, int source, std::optional<float> sample,
        float onThreshold, float offThreshold, float duration, float compensation,
        bool actualLight, bool fading, bool gameplay, std::uint32_t cellID = 0, bool sampleDue = true, int pollMode = 0) {
        if (!target || !source) { *this = {}; return source == 0; }
        if (subject != target || cell != cellID || mode != source || polling != pollMode || low != onThreshold || high != offThreshold ||
            delay != duration || offset != compensation) {
            *this = {};
            subject = target; cell = cellID; mode = source; polling = pollMode; low = onThreshold; high = offThreshold; delay = duration; offset = compensation;
            // Step normally waits for cache settling on a physical light change.
            // No initial toggle has occurred, so begin the duration timer immediately.
            policy.observedLight = source == 2 && actualLight;
        }
        if (!gameplay || (source == 2 && fading)) policy.Suspend(now);
        if (!sampleDue) return policy.on;
        policy.Step(now, gameplay && !(source == 2 && fading) ? sample : std::nullopt,
            source == 2 && actualLight, source == 2 ? compensation : 0.0f,
            low, high, delay, gameplay, AmbientPoll::MaxDecisionGap(pollMode));
        return policy.on;
    }
};
