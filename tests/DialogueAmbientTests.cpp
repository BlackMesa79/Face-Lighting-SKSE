#include "DialogueAmbientPolicy.h"
#include <iostream>
#include <limits>
#include <stdexcept>

void Check(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
int main() {
    try {
        DialogueAmbientPolicy gate;
        double time = 0;
        auto run = [&](int frames, std::optional<float> sample, std::uint64_t actor = 1, int mode = 3,
            bool light = false, bool fading = false, bool gameplay = true) {
            bool allowed = false;
            for (int i = 0; i < frames; ++i) {
                allowed = gate.Update(time, actor, mode, sample, 30, 50, 2, 80, light, fading, gameplay);
                time += 0.1;
            }
            return allowed;
        };
        Check(!run(60, 90.0f), "bright dialogue startup flashed a light");
        Check(!run(10, 22.0f), "short darkness enabled a dialogue light");
        Check(run(15, 22.0f), "sustained darkness did not enable dialogue light");
        Check(run(50, 22.0f, 1, 3, true, true), "live exclusion should ignore its own fade/contribution");
        Check(run(60, 40.0f), "hysteresis band did not retain enabled light");
        Check(run(10, 90.0f), "short brightness disabled light");
        Check(!run(20, 90.0f), "sustained brightness did not disable light");
        Check(!run(10, 22.0f), "settling interval bypassed");
        Check(!run(1, std::nullopt), "missing sample treated as darkness");
        Check(!run(60, std::numeric_limits<float>::quiet_NaN()), "nonfinite sample treated as darkness");
        Check(run(70, 22.0f), "fresh darkness failed to restore light");
        Check(run(50, std::nullopt), "unavailable input changed existing decision");
        Check(!run(1, 90.0f, 2), "speaker change reused prior dark decision");
        Check(!run(60, 40.0f, 2), "new conversation in hysteresis band should start off");
        Check(!run(80, 22.0f, 2, 3, false, false, false), "paused interval enabled light");
        Check(!run(15, 22.0f, 2), "resume counted paused duration");
        Check(run(40, 22.0f, 2), "resumed fresh darkness failed to enable");
        time += 30;
        Check(run(1, 90.0f, 2), "pause gap completed bright timer");
        Check(!run(50, 90.0f, 2), "brightness failed after long-gap resettling");
        Check(run(1, 90.0f, 2, 0), "disabled mode vetoed manual dialogue light");
        Check(gate.subject == 0, "disabled mode retained old target");
        Check(!run(1, 22.0f, 0), "missing speaker enabled dialogue light");
        Check(!run(15, 22.0f, 3, 2), "fixed-mode initial duration bypassed");
        Check(run(10, 22.0f, 3, 2), "fixed-mode darkness failed to enable");
        Check(run(70, 102.0f, 3, 2, true), "fixed compensation did not remove dialogue light contribution");
        Check(gate.policy.estimated == 22, "incorrect fixed compensated estimate");
        Check(run(50, 22.0f, 3, 2, true, true), "fixed-mode fade caused reverse decision");
        Check(!run(80, 170.0f, 3, 2, true), "fixed-mode brightness failed to disable");
        Check(!gate.Update(time, 3, 2, 22.0f, 10, 60, 1, 70, false, false, true),
            "changed thresholds reused prior configuration state");
        Check(!gate.Update(time, 3, 3, 22.0f, 30, 50, 2, 80, false, false, true, 42),
            "new cell inherited a prior decision");
        for (int i = 0; i < 25; ++i) {
            time += 0.1;
            gate.Update(time, 3, 3, 22.0f, 30, 50, 2, 80, false, false, true, 42);
        }
        Check(gate.policy.on, "cell darkness failed to enable light");
        Check(!gate.Update(time, 3, 3, 90.0f, 30, 50, 2, 80, false, false, true, 43),
            "cell transition retained another environment's dark decision");
        // Both automatic sources share the same cadence; no player movement is
        // needed by the policy when fresh brightness readings are supplied.
        for (int mode = 0; mode <= 2; ++mode) {
            DialogueAmbientPolicy scheduled;
            AmbientPoll polling;
            bool allowed = false;
            for (int frame = 0; frame <= 140; ++frame) {
                const double now = frame * 0.05;
                const bool due = polling.Due(now, true, mode);
                allowed = scheduled.Update(now, 1, 3, due ? std::optional<float>{22.0f} : std::nullopt,
                    30, 50, 2, 80, false, false, true, 42, due, mode);
            }
            Check(allowed, "dialogue darkness failed at selected polling cadence");
            for (int frame = 141; frame <= 280; ++frame) {
                const double now = frame * 0.05;
                const bool due = polling.Due(now, true, mode);
                allowed = scheduled.Update(now, 1, 3, due ? std::optional<float>{90.0f} : std::nullopt,
                    30, 50, 2, 80, false, false, true, 42, due, mode);
            }
            Check(!allowed, "dialogue brightness failed at selected polling cadence");
            scheduled = {}; polling = {};
            for (int frame = 0; frame < 20; ++frame) {
                const double now = frame * 0.05;
                const bool due = polling.Due(now, true, mode);
                scheduled.Update(now, 1, 3, 22.0f, 30, 50, 2, 80, false, false, true, 42, due, mode);
            }
            scheduled.Update(1, 1, 3, std::nullopt, 30, 50, 2, 80, false, false, false, 42, false, mode);
            allowed = scheduled.Update(10, 1, 3, 22.0f, 30, 50, 2, 80, false, false, true, 42, true, mode);
            Check(!allowed && !scheduled.policy.pending, "pause completed duration between scheduled polls");
        }
        DialogueAmbientPolicy skipped;
        skipped.policy.on = true; skipped.subject = 1; skipped.mode = 3; skipped.cell = 42;
        skipped.low = 30; skipped.high = 50; skipped.delay = 2; skipped.offset = 80;
        Check(skipped.Update(0, 1, 3, std::nullopt, 30, 50, 2, 80, false, false, true, 42, false),
            "non-poll frame changed decision using empty sample");
        Check(!skipped.Update(0.1, 2, 3, std::nullopt, 30, 50, 2, 80, false, false, true, 42, false),
            "speaker safety reset waited for next poll");
        std::cout << "Dialogue ambient startup, hysteresis, independent sources, compensation, fade, pause and speaker reset passed.\n";
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
