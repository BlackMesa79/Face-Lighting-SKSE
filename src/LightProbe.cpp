#include <SKSE/SKSE.h>
#include "LightProbe.h"
#include "ConfigMenu.h"
#include "CSLighting.h"
#include "AmbientPolicy.h"
#include "AmbientPoll.h"
#include "LightExclusionProbe.h"
#include "ActorRuntime.h"
#include <chrono>
#include <cmath>
#include <mutex>

namespace {
    static_assert(offsetof(RE::HighProcessData, lightLevel) == 0x3A8);
    std::mutex mutex;
    std::string latest;
    std::chrono::steady_clock::time_point next{}, start{};
    std::uint64_t sequence = 0;
    bool running = false;
    AmbientPolicy policy;
    std::optional<Settings::Values> previousSettings;
    RE::FormID previousCell = 0;
    bool previousFirstPerson = false;
    float previousOpacity = 0;
    std::optional<float> compensatedEnvironment, filteredEnvironment;
}
void LightProbe::Reset() {
    std::scoped_lock lock(mutex);
    if (running) SKSE::log::info("[LightProbe] END samples={}", sequence);
    running = false; next = {}; latest.clear(); sequence = 0;
    policy = {}; previousSettings.reset(); previousCell = 0;
    previousOpacity = 0; compensatedEnvironment.reset(); filteredEnvironment.reset();
}
bool LightProbe::Update(RE::Actor* player, const Settings::Values& settings, bool playerLight, std::size_t npcEntries, float playerOpacity, bool sampleDue) {
    if (!settings.lightDiagnostics && !settings.exclusionDiagnostics && settings.ambientMode == 0 && settings.dialogueAmbientMode == 0) { Reset(); return true; }
    const auto ui = RE::UI::GetSingleton();
    std::scoped_lock lock(mutex);
    compensatedEnvironment.reset(); filteredEnvironment.reset();
    const auto now = std::chrono::steady_clock::now();
    const double seconds = std::chrono::duration<double>(now.time_since_epoch()).count();
    const auto cell = player ? player->GetParentCell() : nullptr;
    const auto camera = RE::PlayerCamera::GetSingleton();
    const bool firstPerson = camera && camera->IsInFirstPerson();
    const auto cellID = cell ? cell->GetFormID() : 0;
    if (!previousSettings || *previousSettings != settings || previousCell != cellID || previousFirstPerson != firstPerson) {
        policy = {}; policy.Suspend(seconds);
        previousSettings = settings; previousCell = cellID; previousFirstPerson = firstPerson;
    }
    const bool filteredMode = settings.ambientMode == 3;
    const bool automatic = settings.ambientMode == 2 || filteredMode;
    const auto allows = [&] { return !automatic || policy.on; };
    if (!player || !ui || ConfigMenu::IsOpen() || ui->GameIsPaused() || ui->IsMenuOpen(RE::Console::MENU_NAME)) {
        policy.Suspend(seconds);
        return allows();
    }
    const bool eligible = ActorRuntime::SafeForLight(player) && settings.enabled && settings.intensity > 0 && camera &&
        (!firstPerson || settings.firstPersonLight) && !(settings.hideWhileSneaking && player->IsSneaking()) &&
        cell && cell->IsAttached() && player->Get3D(false);
    if (!eligible) { policy.on = false; policy.Suspend(seconds); }
    // Transitional raw values do not correspond to the full-strength calibration.
    const bool fading = (playerOpacity > 0 && playerOpacity < 1) || playerOpacity != previousOpacity;
    previousOpacity = playerOpacity;
    if (fading) policy.Suspend(seconds);
    if (!sampleDue) return allows();
    const bool wasOn = policy.on;
    std::optional<float> raw;
    const auto process = player->GetActorRuntimeData().currentProcess;
    if (process && process->InHighProcess() && process->high && std::isfinite(process->high->lightLevel))
        raw = process->high->lightLevel;
    const bool sampleEligible = ActorRuntime::SafeForLight(player) && cell && cell->IsAttached() && player->Get3D(false);
    if (sampleEligible) {
        filteredEnvironment = LightExclusionProbe::ReadFiltered(player);
        if (raw && *raw >= 0 && !fading) compensatedEnvironment = std::max(0.0f, *raw - (playerLight ? settings.ambientCompensation : 0.0f));
    }
    const auto input = filteredMode ? filteredEnvironment : raw;
    policy.Step(seconds, input, playerLight, !filteredMode && settings.ambientMode != 0 ? settings.ambientCompensation : 0.0f,
        settings.ambientOnThreshold, settings.ambientOffThreshold, settings.ambientDelay,
        eligible && automatic, AmbientPoll::MaxDecisionGap(settings.ambientPollMode));
    if (wasOn != policy.on) SKSE::log::info("[Ambient] player={} source={} raw={} compensation={} estimated={}",
        policy.on, filteredMode ? "filtered" : "compensation", raw.value_or(0), policy.compensation, policy.estimated);
    if (!settings.lightDiagnostics || now < next) return allows();
    next = now + std::chrono::seconds(1);
    if (!running) {
        running = true; start = now;
        SKSE::log::info("[LightProbe] BEGIN player samples; raw HighProcessData.lightLevel; cached value freshness unknown; ambientMode={}", settings.ambientMode);
    }
    std::string value = "unavailable:no_high_process";
    if (process && process->InHighProcess() && process->high) {
        const float raw = process->high->lightLevel;
        value = std::isfinite(raw) ? std::format("{:.6f}", raw) : "unavailable:non_finite";
    }
    const auto pos = player->GetPosition();
    latest = std::format("sample={} t={:.1f}s raw={} cell={:08X} interior={} pos=({:.1f},{:.1f},{:.1f}) firstPerson={} sneaking={} playerEnabled={} playerLight={} playerIntensity={:.3f} npcEntries={} dialogue={} selected={} follower={} csMode={}",
        ++sequence, std::chrono::duration<double>(now - start).count(), value,
        cell ? cell->GetFormID() : 0, cell && cell->IsInteriorCell(), pos.x, pos.y, pos.z,
        camera && camera->IsInFirstPerson(), player->IsSneaking(), settings.enabled, playerLight, settings.intensity,
        npcEntries, settings.dialogue.enabled, settings.selected.enabled, settings.follower.enabled, settings.csMode);
    latest += std::format(" ambientMode={} compensation={:.3f} estimated={} autoOn={} eligible={} settling={} low={:.1f} high={:.1f} delay={:.1f}",
        settings.ambientMode, policy.compensation, input ? std::format("{:.3f}", policy.estimated) : "unavailable",
        policy.on, eligible, seconds < policy.ready, settings.ambientOnThreshold, settings.ambientOffThreshold, settings.ambientDelay);
    latest += std::format(" pollSeconds={:.1f} sourceRefresh=engine_cache movementRequiredByMod=false",
        AmbientPoll::Interval(settings.ambientPollMode));
    latest += std::format(" playerOpacity={:.3f} fading={} estimateUsable={} source={}", playerOpacity, fading,
        input.has_value() && !fading && seconds >= policy.ready, filteredMode ? "filtered" : "compensation");
    SKSE::log::info("[LightProbe] {}", latest);
    SKSE::log::info("[LightProbe] config sample={} firstPersonAllowed={} hideWhileSneaking={} csActive={} inverse={} globalLinear={} dialogueIntensity={:.3f} selectedIntensity={:.3f} followerIntensity={:.3f}",
        sequence, settings.firstPersonLight, settings.hideWhileSneaking, CSLighting::Available(settings.csMode),
        settings.csInverseSquare, settings.csGlobalLinear, settings.dialogue.intensity, settings.selected.intensity, settings.follower.intensity);
    return allows();
}
std::optional<float> LightProbe::Environment(int mode) {
    std::scoped_lock lock(mutex);
    return mode == 3 ? filteredEnvironment : mode == 2 ? compensatedEnvironment : std::nullopt;
}
std::string LightProbe::Snapshot() { std::scoped_lock lock(mutex); return latest; }
void LightProbe::Mark() {
    std::scoped_lock lock(mutex);
    SKSE::log::info("[LightProbe] MARK latest gameplay sample: {}", latest.empty() ? "none" : latest);
}
