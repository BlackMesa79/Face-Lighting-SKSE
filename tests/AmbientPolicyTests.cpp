#include "AmbientPolicy.h"
#include "AmbientSample.h"
#include "AmbientPoll.h"
#include <iostream>
#include <limits>
#include <stdexcept>

void Check(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
int main() {
    try {
        AmbientPolicy p;
        double time = 0;
        auto run = [&](int frames, std::optional<float> raw, bool light, bool active = true) {
            for (int i = 0; i < frames; ++i) {
                p.Step(time, raw, light, 80, 30, 50, 2, active);
                time += 0.1;
            }
        };
        run(35, 22, false);
        Check(!p.on, "startup stabilization plus sustained darkness required");
        run(15, 22, false);
        Check(p.on && p.estimated == 22 && p.compensation == 0, "darkness turns on without subtracting absent light");
        run(100, 102, true);
        Check(p.on && p.estimated == 22 && p.compensation == 80, "own light does not trigger off");
        run(100, 120, true);
        Check(p.on, "hysteresis band preserves on");
        run(10, 165, true);
        run(10, 102, true);
        Check(p.on, "short bright transient does not turn off");
        run(30, 165, true);
        Check(!p.on, "sustained environmental brightness turns off");
        run(100, 85, false);
        Check(!p.on && p.estimated == 85, "bright state stays off after compensation stops");
        run(100, 40, false);
        Check(!p.on, "hysteresis band preserves off");
        run(15, 22, false);
        run(1, std::nullopt, false);
        run(10, 22, false);
        Check(!p.on, "invalid sample breaks pending darkness");
        run(100, std::numeric_limits<float>::quiet_NaN(), false);
        Check(!p.on && !p.valid, "NaN never interpreted as darkness");
        run(100, 22, false, false);
        Check(!p.on, "observe mode never controls light");
        run(10, 22, false);
        time += 30;
        run(20, 22, false);
        Check(!p.on, "pause gap does not complete pending timer");
        run(30, 22, false);
        Check(p.on, "resumed sustained darkness eventually turns on");
        run(1, 10, true, false);
        Check(p.estimated == 0, "overcompensation is clamped to zero");
        p = {};
        Check(!p.on && !p.pending && !p.observedLight, "load reset discards prior scene state");
        // A long fade-out in a bright environment may temporarily look dark after full compensation.
        // Suspend throughout the transition so it cannot reverse an automatic off decision.
        for (int i = 0; i < 50; ++i) {
            p.Suspend(time);
            p.Step(time, 40.0f, true, 80, 30, 50, 0.5f, true);
            time += 0.1;
        }
        Check(!p.on && !p.pending, "fade-out cannot trigger a false dark reversal");
        run(50, 85, false);
        Check(!p.on, "bright state remains off after fade and cache settlement");
        AmbientSample sample{22.0f, 10.0, 42};
        Check(sample.Read(10.9, 42) == 22.0f, "fresh same-cell exclusion sample accepted");
        Check(!sample.Read(11.01, 42) && !sample.Read(9.9, 42), "expired and future samples rejected");
        Check(!sample.Read(10.1, 43) && !sample.Read(10.1, 0), "cell changes invalidate readings");
        sample.value = std::numeric_limits<float>::quiet_NaN();
        Check(!sample.Read(10.1, 42), "nonfinite filtered value rejected");
        sample = {};
        Check(!sample.Read(10.1, 42), "reset or rejected sample is unavailable, not darkness");
        p = {};
        time = 0;
        auto filteredRun = [&](int frames, float value, bool actualLight, bool refresh = true) {
            for (int i = 0; i < frames; ++i) {
                if (refresh && i % 5 == 0) sample = {value, time, 42};
                p.Step(time, sample.Read(time, 42), actualLight, 0, 30, 50, 2, true);
                time += 0.1;
            }
        };
        filteredRun(50, 22, false);
        Check(p.on, "half-second exclusion samples sustain dark decision");
        filteredRun(80, 22, true);
        Check(p.on && p.compensation == 0 && p.estimated == 22, "filtered values never subtract legacy compensation");
        filteredRun(5, 90, true);
        filteredRun(40, 90, true, false);
        Check(p.on && !p.valid && !p.pending, "single bright sample expires without completing delay");
        filteredRun(40, 90, true);
        Check(!p.on, "sustained fresh external brightness turns off");
        filteredRun(80, 90, false);
        Check(!p.on, "external brightness remains after own light removal");
        filteredRun(10, 22, false);
        sample = {}; p.Suspend(time); time += 30;
        filteredRun(30, 22, false);
        Check(!p.on, "resume must rebuild sustained darkness after settling");
        filteredRun(20, 22, false);
        Check(p.on, "fresh darkness after resume eventually enables light");
        // Simulate frame updates without any position/movement signal. Polling
        // only advances on its schedule, and each cadence can complete a duration.
        Check(AmbientPoll::Interval(0) == 1.0 && AmbientPoll::Interval(1) == 0.5 && AmbientPoll::Interval(2) == 0.2,
            "polling option intervals wrong");
        for (int mode = 0; mode <= 2; ++mode) {
            AmbientPoll clock;
            AmbientPolicy periodic;
            int evaluations = 0;
            for (int frame = 0; frame <= 200; ++frame) {
                const double now = frame * 0.05;
                if (!clock.Due(now, true, mode)) continue;
                ++evaluations;
                periodic.Step(now, 22.0f, false, 0, 30, 50, 2, true, AmbientPoll::MaxDecisionGap(mode));
            }
            Check(periodic.on, "scheduled darkness never enabled (normal ticks mistaken for pause)");
            Check(evaluations <= int(10 / AmbientPoll::Interval(mode)) + 1 && evaluations >= 9 / AmbientPoll::Interval(mode),
                "poll count not limited to selected frequency");
            for (int frame = 201; frame <= 400; ++frame) {
                const double now = frame * 0.05;
                if (clock.Due(now, true, mode))
                    periodic.Step(now, 90.0f, false, 0, 30, 50, 2, true, AmbientPoll::MaxDecisionGap(mode));
            }
            Check(!periodic.on, "stationary bright sample failed to disable under scheduled cadence");
            periodic = {};
            periodic.Step(0, 22.0f, false, 0, 30, 50, 2, true, AmbientPoll::MaxDecisionGap(mode));
            periodic.Step(10, 22.0f, false, 0, 30, 50, 2, true, AmbientPoll::MaxDecisionGap(mode));
            Check(!periodic.on && !periodic.pending, "long gap incorrectly completed dark timer");
        }
        AmbientPoll clock;
        Check(clock.Due(0, true) && !clock.Due(0.99, true) && clock.Due(1, true), "one-second cadence wrong");
        Check(clock.Due(1.1, true, 2) && !clock.Due(1.2, true, 2) && clock.Due(1.31, true, 2),
            "changed option did not reset schedule");
        Check(!clock.Due(2, false, 2) && clock.Due(2.01, true, 2), "pause/resume schedule wrong");
        Check(clock.Due(100, true, 2) && !clock.Due(100, true, 2), "stall caused catch-up burst");
        Check(clock.Due(0, true, 2), "backwards clock failed to reset schedule");
        Check(!clock.Due(std::numeric_limits<double>::quiet_NaN(), true) && clock.Due(0, true),
            "invalid time corrupted schedule");
        std::cout << "Ambient compensation, hysteresis, delays, pause and invalid samples passed.\n";
        return 0;
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
