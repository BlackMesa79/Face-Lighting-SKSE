#include <SKSE/SKSE.h>
#include <RE/Skyrim.h>
#include "FaceLightingAPI.h"
#include "FaceLightingAPIV2.h"
#include "LightExclusionProbe.h"
#include "PublicAPIPolicy.h"
#include "FaceLight.h"
#include "ActorRuntime.h"
#include "Settings.h"
#include "ConfigMenu.h"
#include "Followers.h"
#include "SelectedNPCs.h"
#include <bit>
#include <algorithm>
#include <vector>
#include <cmath>

namespace {
    using namespace FaceLightingAPI;
    template <class Function> Result Boundary(Function function) noexcept {
        try { return function(); } catch (...) { return Result::InternalError; }
    }
    template <class T> bool ValidOutput(T* output) { return output && output->structSize >= sizeof(T); }
    Result Ready() {
        const auto context = FaceLight::GetContext();
        if (!context.gameThread) return Result::WrongThread;
        return context.ready ? Result::Ok : Result::NotReady;
    }
    std::uint64_t Mix(std::uint64_t hash, std::uint64_t value) {
        for (unsigned i = 0; i < 8; ++i) { hash ^= (value >> (i * 8)) & 255; hash *= 1099511628211ull; }
        return hash;
    }
    std::uint64_t ConfigRevision() {
        const auto values = Settings::Get();
        auto hash = Mix(14695981039346656037ull, FaceLight::GetContext().session);
        for (const auto value : {values.enabled, values.follower.enabled, values.selected.enabled,
            values.dialogue.enabled, values.firstPersonLight, values.hideWhileSneaking}) hash = Mix(hash, value);
        for (const auto value : {values.intensity, values.follower.intensity, values.selected.intensity, values.dialogue.intensity})
            hash = Mix(hash, std::bit_cast<std::uint32_t>(value));
        hash = Mix(hash, values.ambientMode);
        hash = Mix(hash, values.ambientPollMode);
        hash = Mix(hash, values.npcLightLimit);
        hash = Mix(hash, values.dialogueAmbientMode);
        for (const auto value : {values.ambientCompensation, values.dialogueAmbientCompensation, values.dialogueAmbientOnThreshold,
            values.dialogueAmbientOffThreshold, values.dialogueAmbientDelay})
            hash = Mix(hash, std::bit_cast<std::uint32_t>(value));
        return hash ? hash : 1;
    }
    Result Resolve(const Token& token, RE::NiPointer<RE::Actor>& actor) {
        auto result = PublicAPIPolicy::ValidateToken(token, FaceLight::GetContext().session);
        if (result != Result::Ok) return result;
        RE::NiPointer<RE::TESObjectREFR> reference;
        RE::LookupReferenceByHandle(token.handle, reference);
        const auto resolved = reference ? reference->As<RE::Actor>() : nullptr;
        if (!resolved || resolved->GetFormID() != token.formID) return Result::InvalidTarget;
        actor.reset(resolved);
        return Result::Ok;
    }
    ActorState Describe(RE::Actor* actor) {
        ActorState result;
        result.target = {FaceLight::GetContext().session, actor->GetHandle().native_handle(), actor->GetFormID()};
        PublicAPIPolicy::CopyName(result.name, actor->GetName() ? actor->GetName() : "");
        const auto values = Settings::Get();
        result.flags = FaceLight::RuntimeFlags(actor);
        auto revision = ConfigRevision();
        if (actor->IsPlayerRef()) {
            if (values.enabled) result.flags |= Enabled;
            result.flags |= GroupEnabled;
        } else {
            const bool follower = actor->IsPlayerTeammate();
            const auto selected = SelectedNPCs::PersonalEnabled(actor->GetFormID());
            const bool followerPreference = Followers::PersonalEnabled(actor->GetFormID());
            if (follower) result.flags |= Follower;
            if (selected.has_value()) result.flags |= Selected;
            if (follower && followerPreference) result.flags |= FollowerEnabled;
            if (selected.value_or(false)) result.flags |= SelectedEnabled;
            if (values.follower.enabled) result.flags |= FollowerGroupEnabled;
            if (values.selected.enabled) result.flags |= SelectedGroupEnabled;
            if ((follower && followerPreference) || selected.value_or(false)) result.flags |= Enabled;
            if (follower ? values.follower.enabled : values.selected.enabled) result.flags |= GroupEnabled;
            revision = Mix(revision, actor->GetFormID());
            revision = Mix(revision, follower);
            revision = Mix(revision, followerPreference);
            revision = Mix(revision, selected ? (*selected ? 2 : 1) : 0);
        }
        result.revision = revision ? revision : 1;
        return result;
    }
    Result ContextQuery(Context* output) noexcept { return Boundary([&] {
        if (!ValidOutput(output)) return Result::InvalidArgument;
        const auto context = FaceLight::GetContext();
        if (!context.gameThread) return Result::WrongThread;
        Context result;
        result.session = context.session; result.ready = context.ready;
        result.revision = ConfigRevision();
        const auto values = Settings::Get();
        result.playerEnabled = values.enabled; result.followerGroupEnabled = values.follower.enabled;
        *output = result;
        return Result::Ok;
    }); }
    Result PlayerQuery(ActorState* output) noexcept { return Boundary([&] {
        if (!ValidOutput(output)) return Result::InvalidArgument;
        if (const auto result = Ready(); result != Result::Ok) return result;
        const auto player = RE::PlayerCharacter::GetSingleton();
        if (!player) return Result::NotReady;
        *output = Describe(player);
        return Result::Ok;
    }); }
    Result ActorQuery(const Token* token, ActorState* output) noexcept { return Boundary([&] {
        if (!token || !ValidOutput(output)) return Result::InvalidArgument;
        if (const auto result = Ready(); result != Result::Ok) return result;
        RE::NiPointer<RE::Actor> actor;
        if (const auto result = Resolve(*token, actor); result != Result::Ok) return result;
        if (actor->IsPlayerRef()) return Result::InvalidTarget;
        *output = Describe(actor.get());
        return Result::Ok;
    }); }
    Result Capture(ActorState* output) noexcept { return Boundary([&] {
        if (!ValidOutput(output)) return Result::InvalidArgument;
        if (const auto result = Ready(); result != Result::Ok) return result;
        const auto pick = RE::CrosshairPickData::GetSingleton();
        const auto reference = pick ? pick->GetActiveTarget().get() : RE::NiPointer<RE::TESObjectREFR>{};
        const auto actor = reference ? reference->As<RE::Actor>() : nullptr;
        if (!ActorRuntime::Eligible(actor)) return Result::InvalidTarget;
        *output = Describe(actor);
        return Result::Ok;
    }); }
    Result FollowersQuery(FollowerPage* output) noexcept { return Boundary([&] {
        if (!ValidOutput(output) || output->reserved || output->capacity > 4096 || (output->capacity && !output->rows))
            return Result::InvalidArgument;
        if (const auto result = Ready(); result != Result::Ok) return result;
        if (output->session != FaceLight::GetContext().session) return Result::StaleSession;
        const auto rows = Followers::Snapshot();
        std::vector<ActorState> snapshot;
        snapshot.reserve(rows.size());
        auto revision = ConfigRevision();
        for (const auto& row : rows) {
            const auto actor = RE::TESForm::LookupByID<RE::Actor>(row.id);
            ActorState state;
            if (actor) state = Describe(actor);
            else {
                state.target = {output->session, 0, row.id}; state.flags = Unsafe | Follower;
                if (row.enabled) state.flags |= Enabled | FollowerEnabled;
                state.revision = Mix(ConfigRevision(), row.id);
                PublicAPIPolicy::CopyName(state.name, row.name);
            }
            revision = Mix(revision, state.target.formID);
            revision = Mix(revision, state.target.handle);
            revision = Mix(revision, state.revision);
            revision = Mix(revision, state.flags & (Loaded | Unsafe | Follower));
            for (const unsigned char c : state.name) revision = Mix(revision, c);
            snapshot.push_back(state);
        }
        if (!revision) revision = 1;
        if (output->revision && output->revision != revision) return Result::StaleRevision;
        if (output->offset > rows.size()) return Result::InvalidArgument;
        const auto count = std::min<std::size_t>(output->capacity, rows.size() - output->offset);
        for (std::size_t i = 0; i < count; ++i) if (!ValidOutput(&output->rows[i])) return Result::InvalidArgument;
        // Publish only after validating every buffer and staging the complete snapshot.
        if (count) std::copy_n(snapshot.begin() + output->offset, count, output->rows);
        output->revision = revision; output->total = static_cast<std::uint32_t>(rows.size());
        output->count = static_cast<std::uint32_t>(count);
        return Result::Ok;
    }); }
    Result Execute(const Request* request) noexcept { return Boundary([&] {
        if (!request || request->structSize < sizeof(Request) || request->enabled > 1 || request->reserved || !request->revision)
            return Result::InvalidArgument;
        if (const auto result = Ready(); result != Result::Ok) return result;
        if (request->target.session != FaceLight::GetContext().session) return Result::StaleSession;
        if (ConfigMenu::IsOpen() || Settings::HasPreview()) return Result::BusyPreview;
        const auto ui = RE::UI::GetSingleton();
        if (!ui || ui->GameIsPaused() || ui->IsMenuOpen(RE::Console::MENU_NAME) ||
            ui->IsMenuOpen(RE::LoadingMenu::MENU_NAME) || ui->IsMenuOpen(RE::MainMenu::MENU_NAME)) return Result::Blocked;
        Result result;
        if (request->command == Command::SetActor) {
            RE::NiPointer<RE::Actor> actor;
            if (const auto resolved = Resolve(request->target, actor); resolved != Result::Ok) return resolved;
            if (!ActorRuntime::Eligible(actor.get())) return Result::InvalidTarget;
            const auto state = Describe(actor.get());
            if (state.revision != request->revision) return Result::StaleRevision;
            if (!(state.flags & Loaded)) return Result::NotLoaded;
            result = SelectedNPCs::SetPersonalNow(actor.get(), request->enabled != 0);
        } else if (request->command == Command::SetPlayer || request->command == Command::SetFollowerGroup) {
            if (ConfigRevision() != request->revision) return Result::StaleRevision;
            auto values = Settings::Get();
            auto& enabled = request->command == Command::SetPlayer ? values.enabled : values.follower.enabled;
            if (enabled == (request->enabled != 0)) return Result::NoChange;
            enabled = request->enabled != 0;
            result = Settings::Save(values) ? Result::Ok : Result::SaveFailed;
        } else return Result::InvalidArgument;
        if (result == Result::Ok) FaceLight::RequestUpdate();
        return result;
    }); }
    const Interface api{sizeof(Interface), version, PlayerControl | ActorControl | FollowerList | StatusQuery, 0,
        ContextQuery, Capture, PlayerQuery, ActorQuery, FollowersQuery, Execute};

    Result TemporaryWriteReady() {
        if (const auto result = Ready(); result != Result::Ok) return result;
        if (ConfigMenu::IsOpen() || Settings::HasPreview()) return Result::BusyPreview;
        const auto ui = RE::UI::GetSingleton();
        return !ui || ui->GameIsPaused() || ui->IsMenuOpen(RE::Console::MENU_NAME) ||
            ui->IsMenuOpen(RE::LoadingMenu::MENU_NAME) || ui->IsMenuOpen(RE::MainMenu::MENU_NAME) ? Result::Blocked : Result::Ok;
    }
    Result ResolveActorV2(std::uint32_t id, ActorState* output) noexcept { return Boundary([&] {
        if (!id || !ValidOutput(output)) return Result::InvalidArgument;
        if (const auto result = Ready(); result != Result::Ok) return result;
        const auto actor = RE::TESForm::LookupByID<RE::Actor>(id);
        if (!ActorRuntime::SafeForLight(actor)) return Result::InvalidTarget;
        *output = Describe(actor); return Result::Ok;
    }); }
    Result BeginV2(const V2::BeginInfo* info, V2::Session* output) noexcept { return Boundary([&] {
        if (!info || !output || info->structSize < sizeof(*info) || info->reserved || info->padding ||
            !std::isfinite(info->leaseSeconds) || info->leaseSeconds < 1 || info->leaseSeconds > 30) return Result::InvalidArgument;
        if (const auto result = TemporaryWriteReady(); result != Result::Ok) return result;
        return FaceLight::BeginTemporary(*info, *output);
    }); }
    Result UpdateV2(const V2::UpdateInfo* info) noexcept { return Boundary([&] {
        if (!info || info->structSize < sizeof(*info) || info->reserved || info->paused > 1 ||
            info->count > V2::maxTargets || (info->count && !info->targets)) return Result::InvalidArgument;
        if (const auto result = info->paused ? Ready() : TemporaryWriteReady(); result != Result::Ok) return result;
        return FaceLight::UpdateTemporary(*info);
    }); }
    Result RenewV2(const V2::Session* session) noexcept { return Boundary([&] {
        if (!session) return Result::InvalidArgument;
        if (const auto result = Ready(); result != Result::Ok) return result;
        return FaceLight::RenewTemporary(*session);
    }); }
    Result EndV2(const V2::Session* session) noexcept { return Boundary([&] {
        if (!session) return Result::InvalidArgument;
        if (const auto result = Ready(); result != Result::Ok) return result;
        return FaceLight::EndTemporary(*session);
    }); }
    Result QueryV2(const V2::Session* session, V2::SessionState* output) noexcept { return Boundary([&] {
        if (!session || !ValidOutput(output)) return Result::InvalidArgument;
        if (const auto result = Ready(); result != Result::Ok) return result;
        return FaceLight::QueryTemporary(*session, *output);
    }); }
    Result EnvironmentV2(V2::Environment* output) noexcept { return Boundary([&] {
        if (!ValidOutput(output)) return Result::InvalidArgument;
        if (const auto result = Ready(); result != Result::Ok) return result;
        const auto ui = RE::UI::GetSingleton();
        if (!ui || ui->GameIsPaused() || ConfigMenu::IsOpen() || Settings::HasPreview() || ui->IsMenuOpen(RE::Console::MENU_NAME) ||
            ui->IsMenuOpen(RE::LoadingMenu::MENU_NAME) || ui->IsMenuOpen(RE::MainMenu::MENU_NAME)) return Result::Blocked;
        const auto player = RE::PlayerCharacter::GetSingleton();
        if (!ActorRuntime::SafeForLight(player)) return Result::InvalidTarget;
        const auto cell = player->GetParentCell();
        if (!cell || !cell->IsAttached() || !player->Get3D(false)) return Result::NotLoaded;
        V2::Environment result;
        result.world = FaceLight::GetContext().session; result.cellID = cell->GetFormID();
        const auto position = player->GetPosition(); result.x = position.x; result.y = position.y; result.z = position.z;
        const auto process = player->GetActorRuntimeData().currentProcess;
        if (process && process->InHighProcess() && process->high && std::isfinite(process->high->lightLevel) && process->high->lightLevel >= 0) {
            result.flags |= V2::RawValid; result.raw = process->high->lightLevel;
        }
        if (const auto filtered = LightExclusionProbe::ReadFiltered(player); filtered && std::isfinite(*filtered) && *filtered >= 0) {
            result.flags |= V2::FilteredValid; result.filtered = *filtered;
        }
        *output = result; return Result::Ok;
    }); }
    const V2::Interface apiV2{sizeof(V2::Interface), V2::version, V2::TemporarySessions | V2::RGBColor | V2::EnvironmentQuery, 0,
        &api, ResolveActorV2, BeginV2, UpdateV2, RenewV2, EndV2, QueryV2, EnvironmentV2};
}

extern "C" __declspec(dllexport) const FaceLightingAPI::Interface* FaceLighting_GetAPI(std::uint32_t requestedVersion) noexcept {
    if (requestedVersion == FaceLightingAPI::version) return &api;
    if (requestedVersion == FaceLightingAPI::V2::version) return reinterpret_cast<const FaceLightingAPI::Interface*>(&apiV2);
    return nullptr;
}
