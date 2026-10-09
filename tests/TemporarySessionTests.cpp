#include "TemporaryLightSession.h"
#include "NpcLightManager.h"
#include "ColorTemperature.h"
#include <iostream>
#include <stdexcept>
#include <limits>
#include <map>

namespace {
    void Check(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
    struct Light { static inline int cleared = 0; void Clear() { ++cleared; } };
}
int main() {
    using namespace FaceLightingAPI;
    try {
        TemporaryLightSession store;
        V2::BeginInfo begin; begin.world = 7;
        V2::Session session, second{99, 99};
        Check(store.Begin(begin, 7, 100, session) == Result::Ok && !store.Active(), "empty begin changed lighting");
        Check(store.Begin(begin, 7, 100, second) == Result::Blocked && second.id == 99, "concurrent session/output overwritten");
        V2::Target targets[4];
        for (unsigned i = 0; i < 4; ++i) targets[i].actor = {7, 10 + i, 100 + i};
        V2::UpdateInfo update; update.session = session; update.targets = targets; update.count = 2;
        auto valid = [](auto&) { return Result::Ok; };
        Check(store.Update(update, 7, 101, valid) == Result::Ok && store.Active(), "snapshot rejected");
        Check(!store.Rendering(true) && store.Rendering(false), "menu preview failed to suppress temporary requests");
        targets[0].light.intensity = 3;
        Check(store.Targets()[0].light.intensity == 1, "retained caller memory");
        targets[1].actor.formID = 100;
        Check(store.Update(update, 7, 102, valid) == Result::InvalidArgument && store.Targets()[0].light.intensity == 1,
            "duplicate snapshot partially committed");
        targets[1].actor.formID = 101;
        targets[1].light.red = std::numeric_limits<float>::quiet_NaN();
        Check(store.Update(update, 7, 102, valid) == Result::InvalidArgument, "NaN accepted");
        targets[1].light.red = 1;
        Check(store.Update(update, 7, 102, [](const auto& t) { return t.formID == 101 ? Result::NotLoaded : Result::Ok; }) == Result::NotLoaded &&
            store.Targets()[0].light.intensity == 1, "invalid actor partially committed");
        update.count = 5;
        Check(store.Update(update, 7, 102, valid) == Result::InvalidArgument, "target cap bypassed");
        update.count = 4;
        Check(store.Update(update, 7, 102, valid) == Result::Ok && store.Targets().size() == 4, "four participants rejected");
        update.paused = 1;
        Check(store.Update(update, 7, 103, valid) == Result::Ok && !store.Active(), "pause ignored");
        V2::SessionState state;
        Check(store.Query(session, 7, 104, state) == Result::Ok && state.paused && state.count == 4 && state.remainingSeconds == 4,
            "lease query inconsistent");
        Check(store.Renew(session, 7, 107) == Result::Ok, "paused renewal failed");
        Check(store.Query(session, 7, 111, state) == Result::Ok, "renewal failed to extend");
        Check(store.Query(session, 7, 112, state) == Result::StaleSession && !store.Exists(), "paused session did not expire");
        Check(store.Begin(begin, 7, 112, second) == Result::Ok && second.id != session.id, "session ID reused");
        Check(store.End(session, 7, 112) == Result::StaleSession && store.Exists(), "old handle ended newer session");
        Check(store.End(second, 8, 112) == Result::StaleSession && store.Exists(), "cross-save handle accepted");
        update.session = second; update.count = 2; update.paused = 0;
        Check(store.Update(update, 7, 112, valid) == Result::Ok, "second session update failed");
        store.Prune([](const auto& t) { return t.formID == 101; });
        Check(store.Targets().size() == 1 && store.Targets()[0].actor.formID == 101, "unsafe target not pruned");
        Check(store.End(second, 7, 113) == Result::Ok && !store.Exists(), "end failed");
        begin.leaseSeconds = 5;
        Check(store.Begin(begin, 7, 113, session) == Result::Ok, "restart failed");
        update.session = session; update.count = 0; update.targets = nullptr;
        Check(store.Update(update, 7, 113, valid) == Result::Ok && !store.Active() && store.Exists(), "empty snapshot failed to release ownership");
        store.Clear();
        Check(store.Check(session, 7, 113) == Result::StaleSession, "load reset retained session");
        begin.leaseSeconds = std::numeric_limits<float>::infinity();
        Check(store.Begin(begin, 7, 113, session) == Result::InvalidArgument, "infinite lease accepted");
        V2::LightParameters params;
        params.offsetSpace = static_cast<V2::OffsetSpace>(42);
        Check(!TemporaryLightSession::Valid(params), "unsupported coordinate space accepted");
        params = {}; params.intensity = -1;
        Check(!TemporaryLightSession::Valid(params), "negative intensity accepted");
        auto rgb = ColorTemperature::ConvertSRGB({0.5f, 0, 1}, true);
        Check(std::abs(rgb.r - 0.214041f) < 0.00001f && rgb.g == 0 && rgb.b == 1, "sRGB conversion incorrect");

        // External participants override native dialogue and do not consume the
        // follower budget. Disabled requests fade out and never recreate empty lights.
        NpcLightManager<int, int, Light> manager{66};
        std::map<int, int> rendered;
        auto frame = [&] {
            rendered.clear();
            manager.Update(0.2f, [](int) { return true; }, [&](Light&, int actor, int value, float) { rendered[actor] = value; }, 1, true);
        };
        manager.BeginFrame();
        manager.Submit(100, NpcLightSource::Dialogue, 1, false, 0);
        for (int i = 100; i < 104; ++i) manager.Submit(i, NpcLightSource::External, 2, true, 0.2f);
        frame(); frame();
        Check(manager.Size() == 4 && rendered.at(100) == 2, "external priority/capacity incorrect");
        manager.BeginFrame(); manager.Submit(100, NpcLightSource::External, 9, true, 0.2f, false);
        frame();
        Check(rendered.at(100) == 2, "fade-out discarded last emitted parameters");
        frame();
        Check(manager.Size() == 0, "disabled request never released");
        frame(); frame();
        Check(manager.Size() == 0 && rendered.empty(), "disabled request recreated an invisible light");
        manager.BeginFrame(); manager.Submit(101, NpcLightSource::External, 2, false, 0); frame();
        manager.DropSource(NpcLightSource::External);
        Check(manager.Size() == 0, "session cleanup retained scene light");
        manager.BeginFrame(); manager.Submit(100, NpcLightSource::Dialogue, 1, false, 0); frame();
        Check(manager.Size() == 1 && rendered.at(100) == 1, "native source failed to restore");
        // Empty/disabled external target vetoes native fallback without allocating.
        manager.Clear(); manager.BeginFrame();
        manager.Submit(100, NpcLightSource::Dialogue, 1, false, 0);
        manager.Submit(100, NpcLightSource::External, 2, true, 0.2f, false);
        frame(); frame();
        Check(manager.Size() == 0 && rendered.empty(), "disabled external target allowed native fallback");
        std::cout << "V2 leases, snapshots, atomic validation, pause/expiry, RGB and renderer arbitration passed.\n";
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
