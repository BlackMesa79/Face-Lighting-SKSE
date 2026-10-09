// Minimal client lifecycle example, compiled by Face Lighting's ABI test target.
// Include only FaceLightingAPI.h and FaceLightingAPIV2.h in a client project.
#include "FaceLightingAPIV2.h"
#include <Windows.h>
#include <array>
#include <span>

namespace CCCFaceLightingExample {
    using namespace FaceLightingAPI;
    const V2::Interface* Discover() noexcept {
        const auto module = GetModuleHandleW(L"FaceLighting.dll");
        if (!module) return nullptr;
        const auto getAPI = reinterpret_cast<V2::GetAPI>(GetProcAddress(module, exportName));
        if (!getAPI) return nullptr;
        const auto api = getAPI(V2::version);
        return api && api->apiVersion == V2::version && api->structSize >= sizeof(V2::Interface) &&
            (api->capabilities & V2::TemporarySessions) && api->v1 && api->v1->apiVersion == 1 ? api : nullptr;
    }
    // Every method except Discover must run on the game thread. Do not queue
    // stale actor pointers; queue FormIDs, then resolve and validate here.
    class Client {
        const V2::Interface* api = nullptr;
        V2::Session session{};
    public:
        void SetAPI(const V2::Interface* value) { api = value; }
        Result Begin() {
            if (!api) return Result::NotReady;
            if (session.id) return Result::NoChange;
            Context context;
            const auto result = api->v1->GetContext(&context);
            if (result != Result::Ok) return result;
            if (!context.ready) return Result::NotReady;
            V2::BeginInfo info; info.world = context.session; info.leaseSeconds = 5;
            return api->BeginSession(&info, &session);
        }
        Result SetParticipants(std::span<const std::uint32_t> formIDs, bool paused = false) {
            if (!api || !session.id) return Result::NotReady;
            if (formIDs.size() > V2::maxTargets) return Result::InvalidArgument;
            std::array<V2::Target, V2::maxTargets> targets{};
            for (std::size_t i = 0; i < formIDs.size(); ++i) {
                ActorState actor;
                const auto result = api->ResolveActor(formIDs[i], &actor);
                if (result != Result::Ok) return result;
                targets[i].actor = actor.target;
                // Example defaults. CCC may choose per-participant values instead.
                targets[i].light.intensity = 0.8f;
                targets[i].light.colorMode = V2::ColorMode::Temperature;
                targets[i].light.temperature = 5500;
                targets[i].light.transitionSeconds = 0.2f;
            }
            V2::UpdateInfo info;
            info.session = session; info.count = static_cast<std::uint32_t>(formIDs.size());
            info.targets = targets.data(); info.paused = paused;
            const auto result = api->UpdateSession(&info);
            if (result == Result::StaleSession) session = {};
            return result;
        }
        // Call on a game-thread timer about once per second, including while
        // paused. QuerySession does NOT renew. Rebuild after StaleSession.
        Result Heartbeat() {
            if (!api || !session.id) return Result::NotReady;
            const auto result = api->RenewSession(&session);
            if (result == Result::StaleSession) session = {};
            return result;
        }
        Result End() {
            if (!api || !session.id) return Result::NoChange;
            const auto result = api->EndSession(&session);
            if (result == Result::Ok || result == Result::StaleSession || result == Result::NotReady) session = {};
            return result;
        }
    };
}
