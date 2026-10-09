#pragma once
#include "FaceLightingAPIV2.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

// Main-thread lease and copied requests. Independent of engine allocation/serialization.
class TemporaryLightSession {
    using Result = FaceLightingAPI::Result;
    using Session = FaceLightingAPI::V2::Session;
    using Target = FaceLightingAPI::V2::Target;
    Session token{};
    std::uint64_t nextID = 0;
    double deadline = 0;
    float lease = 5;
    bool paused = false;
    std::vector<Target> targets;
public:
    static bool Valid(const FaceLightingAPI::V2::LightParameters& p) {
        using namespace FaceLightingAPI::V2;
        const auto range = [](float v, float lo, float hi) { return std::isfinite(v) && v >= lo && v <= hi; };
        return p.structSize >= sizeof(p) && !p.reserved && p.enabled <= 1 &&
            (p.colorMode == ColorMode::Temperature || p.colorMode == ColorMode::SRGB) &&
            (p.offsetSpace == OffsetSpace::ActorHeading || p.offsetSpace == OffsetSpace::HeadBone) &&
            range(p.intensity, 0, 5) && range(p.radius, 10, 500) && range(p.temperature, 2000, 10000) &&
            range(p.red, 0, 1) && range(p.green, 0, 1) && range(p.blue, 0, 1) &&
            range(p.offsetX, -150, 150) && range(p.offsetY, -150, 150) && range(p.offsetZ, -150, 150) &&
            range(p.transitionSeconds, 0, 3);
    }
    void Clear() { token = {}; targets.clear(); paused = false; deadline = 0; }
    bool Expire(double now) { if (token.id && now >= deadline) { Clear(); return true; } return false; }
    bool Exists() const { return token.id != 0; }
    bool Active() const { return Exists() && !paused && !targets.empty(); }
    bool Rendering(bool preview) const { return Active() && !preview; }
    const std::vector<Target>& Targets() const { return targets; }
    Result Check(Session value, std::uint64_t world, double now) {
        Expire(now);
        if (value.world != world) return Result::StaleSession;
        return value.id && value.id == token.id && value.world == token.world ? Result::Ok : Result::StaleSession;
    }
    Result Begin(const FaceLightingAPI::V2::BeginInfo& info, std::uint64_t world, double now, Session& output) {
        if (info.structSize < sizeof(info) || info.reserved || info.padding ||
            !std::isfinite(info.leaseSeconds) || info.leaseSeconds < 1 || info.leaseSeconds > 30) return Result::InvalidArgument;
        if (info.world != world) return Result::StaleSession;
        Expire(now);
        if (Exists()) return Result::Blocked;
        if (nextID == std::numeric_limits<std::uint64_t>::max()) return Result::InternalError;
        token = {world, ++nextID}; lease = info.leaseSeconds; deadline = now + lease;
        output = token;
        return Result::Ok;
    }
    template <class ValidateActor>
    Result Update(const FaceLightingAPI::V2::UpdateInfo& info, std::uint64_t world, double now, ValidateActor validate) {
        if (info.structSize < sizeof(info) || info.reserved || info.paused > 1 ||
            info.count > FaceLightingAPI::V2::maxTargets || (info.count && !info.targets)) return Result::InvalidArgument;
        if (const auto result = Check(info.session, world, now); result != Result::Ok) return result;
        std::vector<Target> staged;
        staged.reserve(info.count);
        for (std::uint32_t i = 0; i < info.count; ++i) {
            const auto& target = info.targets[i];
            if (!Valid(target.light)) return Result::InvalidArgument;
            if (target.actor.session != world) return Result::StaleSession;
            if (!target.actor.formID || !target.actor.handle) return Result::InvalidTarget;
            if (std::any_of(staged.begin(), staged.end(), [&](const auto& t) { return t.actor.formID == target.actor.formID; }))
                return Result::InvalidArgument;
            if (const auto result = validate(target.actor); result != Result::Ok) return result;
            staged.push_back(target);
        }
        targets.swap(staged); paused = info.paused != 0; deadline = now + lease;
        return Result::Ok;
    }
    Result Renew(Session value, std::uint64_t world, double now) {
        if (const auto result = Check(value, world, now); result != Result::Ok) return result;
        deadline = now + lease; return Result::Ok;
    }
    Result End(Session value, std::uint64_t world, double now) {
        if (const auto result = Check(value, world, now); result != Result::Ok) return result;
        Clear(); return Result::Ok;
    }
    Result Query(Session value, std::uint64_t world, double now, FaceLightingAPI::V2::SessionState& output) {
        if (const auto result = Check(value, world, now); result != Result::Ok) return result;
        FaceLightingAPI::V2::SessionState result;
        result.session = token; result.paused = paused; result.count = static_cast<std::uint32_t>(targets.size());
        result.remainingSeconds = static_cast<float>(std::max(0.0, deadline - now));
        output = result; return Result::Ok;
    }
    template <class ValidActor> void Prune(ValidActor valid) {
        std::erase_if(targets, [&](const auto& target) { return !valid(target.actor); });
    }
};
