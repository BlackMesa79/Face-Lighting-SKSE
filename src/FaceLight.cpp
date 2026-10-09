#include <SKSE/SKSE.h>
#include <RE/Skyrim.h>
#include <atomic>
#include <cmath>
#include "FaceLight.h"
#include "Settings.h"
#include "LightPlacement.h"
#include "CSLighting.h"
#include "ColorTemperature.h"
#include <optional>
#include <chrono>
#include <vector>
#include <memory>
#include "LightTransition.h"
#include "PlayerDialoguePolicy.h"
#include "LightInstance.h"
#include "NpcLightManager.h"
#include "SelectedNPCs.h"
#include "ActorRuntime.h"
#include "Followers.h"
#include "LightProbe.h"
#include "LightExclusionProbe.h"
#include "ConfigMenu.h"
#include "FaceLightingAPI.h"
#include "DialogueAmbientPolicy.h"
#include "AmbientPoll.h"
#include "TemporaryLightSession.h"
#include <Windows.h>

namespace {
    using FaceLightingAPI::Result;
    double Seconds() {
        return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
    }
    Result ResolveTemporary(const FaceLightingAPI::Token& token, std::uint64_t world, RE::NiPointer<RE::Actor>& actor) {
        if (token.session != world) return Result::StaleSession;
        if (!token.handle || !token.formID) return Result::InvalidTarget;
        RE::NiPointer<RE::TESObjectREFR> reference;
        RE::LookupReferenceByHandle(token.handle, reference);
        const auto resolved = reference ? reference->As<RE::Actor>() : nullptr;
        if (!resolved || resolved->GetFormID() != token.formID || !ActorRuntime::SafeForLight(resolved)) return Result::InvalidTarget;
        const auto cell = resolved->GetParentCell();
        const auto root = resolved->Get3D(false);
        static const RE::BSFixedString headName("NPC Head [Head]");
        const auto object = root ? root->GetObjectByName(headName) : nullptr;
        const auto head = object ? object->AsNode() : nullptr;
        if (!cell || !cell->IsAttached() || !root || !head || !std::isfinite(head->world.scale) || head->world.scale <= 0.0001f)
            return Result::NotLoaded;
        actor.reset(resolved);
        return Result::Ok;
    }
    struct RenderParameters {
        Settings::Values settings;
        std::optional<ColorTemperature::RGB> color;
        RenderParameters(const Settings::Values& values) : settings(values) {}
    };
    RenderParameters TemporaryParameters(Settings::Values values, const FaceLightingAPI::V2::LightParameters& p) {
        values.enabled = true;
        values.intensity = p.intensity;
        values.radius = values.inverseRadius = p.radius;
        values.manualRange = true;
        values.temperature = p.temperature;
        values.followHeadRotation = p.offsetSpace == FaceLightingAPI::V2::OffsetSpace::HeadBone;
        values.offsetX = p.offsetX; values.offsetY = p.offsetY; values.offsetZ = p.offsetZ;
        RenderParameters result{values};
        if (p.colorMode == FaceLightingAPI::V2::ColorMode::SRGB) result.color = {p.red, p.green, p.blue};
        return result;
    }
    struct State {
        LightInstance playerLight;
        LightTransition playerFade;
        PlayerDialoguePolicy playerDialogue;
        NpcLightManager<RE::ActorHandle, RenderParameters, LightInstance> npcLights{SelectedNPCs::limit + Followers::lightLimit + 2};
        TemporaryLightSession temporary;
        std::optional<RenderParameters> lastPlayerParameters;
        bool lastPlayerTemporary = false, wasExternalActive = false;
        std::atomic<bool> gameActive = false, loading = false, taskPending = false, resetRequested = false;
        std::atomic<std::uint64_t> session{1};
        std::atomic<DWORD> gameThread{0};
        bool ambientAllowed = true, dialogueAmbientAllowed = true;
        DialogueAmbientPolicy dialogueAmbient;
        AmbientPoll ambientPoll;
        RE::ActorHandle dialogueSubject;
        float previousDialogueOpacity = 0;
        std::chrono::steady_clock::time_point lastUpdate{};

        void ReleaseTemporary() {
            npcLights.DropSource(NpcLightSource::External);
            if (lastPlayerTemporary) {
                playerLight.Clear("temporary session released"); playerFade = {}; lastPlayerParameters.reset();
            }
            lastPlayerTemporary = false; wasExternalActive = false;
        }

        void Clear() {
            playerLight.Clear();
            playerFade = {};
            lastPlayerParameters.reset();
            temporary.Clear();
            lastPlayerTemporary = wasExternalActive = false;
            LightExclusionProbe::Reset();
            playerDialogue.Reset();
            ambientPoll = {};
            dialogueAmbient = {}; dialogueSubject = {}; dialogueAmbientAllowed = true; previousDialogueOpacity = 0;
            npcLights.Clear();
            Followers::Reset();
            LightProbe::Reset();
            lastUpdate = {};
        }

        void Update() {
            if (resetRequested.exchange(false)) Clear();
            const auto player = RE::PlayerCharacter::GetSingleton();
            const auto ui = RE::UI::GetSingleton();
            if (!gameActive || loading || !player || !ui || ui->IsMenuOpen(RE::MainMenu::MENU_NAME) ||
                ui->IsMenuOpen(RE::LoadingMenu::MENU_NAME)) {
                Clear();
                return;
            }
            const bool dialogueOpen = ui->IsMenuOpen(RE::DialogueMenu::MENU_NAME);
            temporary.Expire(Seconds());
            temporary.Prune([&](const auto& token) {
                RE::NiPointer<RE::Actor> actor;
                return ResolveTemporary(token, session.load(), actor) == Result::Ok;
            });
            const bool externalActive = temporary.Rendering(ConfigMenu::IsOpen() || Settings::HasPreview());
            if (!externalActive && wasExternalActive) ReleaseTemporary();
            wasExternalActive = externalActive;
            const auto saved = Settings::Get();
            if (const auto enabled = playerDialogue.Update(dialogueOpen, saved.enablePlayerOnDialogue,
                saved.disablePlayerAfterDialogue, saved.enabled, temporary.Exists())) Settings::SetPlayerEnabled(*enabled);
            const auto settings = Settings::GetActive();
            const auto now = std::chrono::steady_clock::now();
            const float delta = lastUpdate == std::chrono::steady_clock::time_point{} ? 0.0f :
                std::chrono::duration<float>(now - lastUpdate).count();
            lastUpdate = now;
            const auto camera = RE::PlayerCamera::GetSingleton();
            const double seconds = std::chrono::duration<double>(now.time_since_epoch()).count();
            const bool gameplay = !ConfigMenu::IsOpen() && !ui->GameIsPaused() && !ui->IsMenuOpen(RE::Console::MENU_NAME);
            const bool sampleDue = ambientPoll.Due(seconds, gameplay && (settings.ambientMode != 0 ||
                settings.dialogueAmbientMode != 0 || settings.lightDiagnostics || settings.exclusionDiagnostics), settings.ambientPollMode);
            LightExclusionProbe::Update(settings.exclusionDiagnostics || settings.ambientMode == 3 || settings.dialogueAmbientMode == 3,
                gameplay, settings.exclusionDiagnostics);
            const bool ambientAllows = LightProbe::Update(player, settings, playerLight.rendererLight.get() != nullptr,
                npcLights.Size(), playerLight.rendererLight ? playerFade.value : 0.0f, sampleDue);
            ambientAllowed = ambientAllows;
            const bool viewAllowed = ActorRuntime::SafeForLight(player) &&
                !ActorRuntime::SneakHidden(player, settings.hideWhileSneaking) &&
                camera && (!camera->IsInFirstPerson() || settings.firstPersonLight);
            if (viewAllowed) {
                bool wanted = settings.enabled && ambientAllows && settings.intensity > 0 &&
                    !(settings.hideWhileSneaking && player->IsSneaking());
                RenderParameters renderParameters{settings};
                bool transition = settings.playerTransition;
                float duration = settings.playerDuration;
                bool playerTemporary = false;
                if (externalActive) for (const auto& target : temporary.Targets()) if (target.actor.formID == player->GetFormID()) {
                    playerTemporary = true;
                    renderParameters = TemporaryParameters(settings, target.light);
                    wanted = target.light.enabled && target.light.intensity > 0;
                    transition = target.light.transitionSeconds > 0; duration = target.light.transitionSeconds;
                }
                if (!playerTemporary && lastPlayerTemporary) {
                    playerLight.Clear("temporary player target removed"); playerFade = {}; lastPlayerParameters.reset();
                }
                lastPlayerTemporary = playerTemporary;
                if (wanted) {
                    lastPlayerParameters = renderParameters;
                } else if (playerTemporary && lastPlayerParameters) {
                    // Keep the last emitted RGB/intensity until an outgoing fade finishes.
                    renderParameters = *lastPlayerParameters;
                }
                playerFade.Update(wanted, transition, duration,
                    ui->GameIsPaused() ? 0.0f : std::clamp(delta, 0.0f, 0.1f));
                // Manual off requests fade-out; the instance must stay enabled until opacity reaches zero.
                renderParameters.settings.enabled = true;
                playerLight.Update(player, renderParameters.settings, playerFade.value, camera->IsInFirstPerson(), renderParameters.color);
                if (!wanted && playerFade.value <= 0) lastPlayerParameters.reset();
            } else { playerLight.Clear(); playerFade = {}; lastPlayerParameters.reset(); }

            RE::ActorHandle target;
            const auto topics = RE::MenuTopicManager::GetSingleton();
            if (settings.dialogue.enabled && settings.dialogue.intensity > 0 && topics &&
                ui->IsMenuOpen(RE::DialogueMenu::MENU_NAME)) {
                const auto reference = topics->speaker.get();
                if (const auto actor = reference ? reference->As<RE::Actor>() : nullptr;
                    actor && actor != player) target = actor->GetHandle();
            }
            const auto existingDialogue = target ? npcLights.Find(target) : nullptr;
            const bool actualDialogueLight = existingDialogue && existingDialogue->rendererLight;
            const float dialogueOpacity = target ? npcLights.Opacity(target) : 0.0f;
            if (dialogueSubject != target) { previousDialogueOpacity = dialogueOpacity; }
            dialogueSubject = target;
            const bool dialogueFading = (dialogueOpacity > 0 && dialogueOpacity < 1) || dialogueOpacity != previousDialogueOpacity;
            previousDialogueOpacity = dialogueOpacity;
            const bool allowedBefore = dialogueAmbientAllowed;
            dialogueAmbientAllowed = dialogueAmbient.Update(seconds, target.native_handle(), settings.dialogueAmbientMode,
                LightProbe::Environment(settings.dialogueAmbientMode), settings.dialogueAmbientOnThreshold,
                settings.dialogueAmbientOffThreshold, settings.dialogueAmbientDelay, settings.dialogueAmbientCompensation,
                actualDialogueLight, dialogueFading, gameplay,
                player->GetParentCell() ? player->GetParentCell()->GetFormID() : 0, sampleDue, settings.ambientPollMode);
            if (const auto actor = target.get(); actor && settings.dialogueAmbientMode && allowedBefore != dialogueAmbientAllowed)
                SKSE::log::info("[Ambient] dialogue={:08X} enabled={} mode={} estimated={} valid={}",
                    actor->GetFormID(), dialogueAmbientAllowed, settings.dialogueAmbientMode,
                    dialogueAmbient.policy.estimated, dialogueAmbient.policy.valid);
            const auto convert = [&](const Settings::Dialogue& d) {
                auto lightSettings = settings;

                lightSettings.enabled = true;
                lightSettings.radius = d.radius;
                lightSettings.manualRange = d.manualRange;
                lightSettings.inverseRadius = d.inverseRadius;
                lightSettings.intensity = d.intensity;
                lightSettings.temperature = d.temperature;
                lightSettings.offsetX = d.offsetX;
                lightSettings.offsetY = d.offsetY;
                lightSettings.offsetZ = d.offsetZ;
                lightSettings.followHeadRotation = d.followHeadRotation;
                lightSettings.csInverseSquare = d.csInverseSquare;
                lightSettings.csLinear = d.csLinear;
                return lightSettings;
            };
            const auto& d = settings.dialogue;
            const auto lightSettings = convert(d);
            npcLights.BeginFrame();
            const auto followerSettings = convert(settings.follower);
            npcLights.RefreshSource(NpcLightSource::Follower, followerSettings, settings.follower.transition, settings.follower.duration);
            for (const auto& actor : Followers::Update())
                if (!externalActive && !dialogueOpen && settings.follower.enabled && settings.follower.intensity > 0)
                    npcLights.Submit(actor, NpcLightSource::Follower, followerSettings, settings.follower.transition, settings.follower.duration);
            const auto selectedSettings = convert(settings.selected);
            npcLights.RefreshSource(NpcLightSource::Selected, selectedSettings, settings.selected.transition, settings.selected.duration);
            for (const auto& actor : SelectedNPCs::Update())
                if (!externalActive && !dialogueOpen && settings.selected.enabled && settings.selected.intensity > 0)
                    npcLights.Submit(actor, NpcLightSource::Selected, selectedSettings, settings.selected.transition, settings.selected.duration);
            npcLights.RefreshSource(NpcLightSource::Dialogue, lightSettings, d.transition, d.duration);
            if (!externalActive && target && dialogueAmbientAllowed) npcLights.Submit(target, NpcLightSource::Dialogue, lightSettings, d.transition, d.duration);
            if (externalActive) for (const auto& request : temporary.Targets()) {
                RE::NiPointer<RE::Actor> actor;
                if (ResolveTemporary(request.actor, session.load(), actor) != Result::Ok || actor->IsPlayerRef()) continue;
                auto parameters = TemporaryParameters(lightSettings, request.light);
                // A disabled target suppresses fallback sources while its existing light fades out.
                if (parameters.settings.intensity <= 0) parameters.settings.intensity = lightSettings.intensity > 0 ? lightSettings.intensity : 1.0f;
                npcLights.Submit(actor->GetHandle(), NpcLightSource::External, parameters,
                    request.light.transitionSeconds > 0, request.light.transitionSeconds,
                    request.light.enabled && request.light.intensity > 0);
            }
            // Dialogue temporarily reserves NPC lighting for its current speaker.
            npcLights.Update(ui->GameIsPaused() ? 0.0f : std::clamp(delta, 0.0f, 0.1f), [&](const RE::ActorHandle& handle) {
                const auto actor = handle.get();
                const auto cell = actor ? actor->GetParentCell() : nullptr;
                return ActorRuntime::Eligible(actor.get()) &&
                    !ActorRuntime::SneakHidden(actor.get(), settings.hideWhileSneaking) &&
                    actor->Get3D(false) && cell && cell->IsAttached();
            }, [](LightInstance& light, const RE::ActorHandle& handle, const RenderParameters& parameters, float opacity) {
                const auto actor = handle.get();
                light.Update(actor.get(), parameters.settings, opacity, false, parameters.color);
            }, static_cast<std::size_t>(settings.npcLightLimit), dialogueOpen || externalActive,
                !externalActive && target && !dialogueAmbientAllowed ? &target : nullptr);
        }
    };
    State& GetState() {
        static auto* state = new State;
        return *state;
    }

    REL::Relocation<void (*)(RE::PlayerCharacter*, float)> originalUpdate;
    void UpdatePlayer(RE::PlayerCharacter* player, float delta) {
        originalUpdate(player, delta);
        // DataLoaded can arrive on a loader thread. Establish the actual game
        // thread from the update hook, before accepting public API calls.
        GetState().gameThread.store(GetCurrentThreadId());
        GetState().Update();
    }

    class MenuEvents final : public RE::BSTEventSink<RE::MenuOpenCloseEvent> {
        RE::BSEventNotifyControl ProcessEvent(const RE::MenuOpenCloseEvent* event,
            RE::BSTEventSource<RE::MenuOpenCloseEvent>*) override {
            if (event) {
                if (event->menuName == RE::LoadingMenu::MENU_NAME) {
                    GetState().loading.store(event->opening);
                    if (event->opening) GetState().resetRequested.store(true);
                    FaceLight::RequestUpdate();
                } else if (event->menuName == RE::DialogueMenu::MENU_NAME) {
                    FaceLight::RequestUpdate();
                } else if (event->menuName == RE::MainMenu::MENU_NAME && event->opening) {
                    FaceLight::SetGameActive(false);
                }
            }
            return RE::BSEventNotifyControl::kContinue;
        }
    };
}

void FaceLight::Install() {
    static bool installed = false;
    if (installed) return;
    CSLighting::Detect();
    LightExclusionProbe::Install();
    // Actor::Update is virtual slot 0xAD on SE/AE; preserve the previous hook.
    REL::Relocation<std::uintptr_t> vtable{RE::VTABLE_PlayerCharacter[0]};
    originalUpdate = vtable.write_vfunc(0xAD, UpdatePlayer);
    static auto* menuEvents = new MenuEvents;
    if (const auto ui = RE::UI::GetSingleton()) ui->AddEventSink(menuEvents);
    installed = true;
    SKSE::log::info("Player face lighting update hook installed");
}

void FaceLight::SetGameActive(bool active) {
    auto& state = GetState();
    ++state.session;
    state.gameActive.store(active);
    SelectedNPCs::SetActive(active);
    // SKSE load/new-game messages execute on the game thread.
    state.Clear();
    Settings::ClearPreview();
    if (active) RequestUpdate();
}

void FaceLight::RequestUpdate() {
    auto& state = GetState();
    const auto tasks = SKSE::GetTaskInterface();
    if (!tasks || state.taskPending.exchange(true)) return;
    tasks->AddTask([] {
        GetState().taskPending.store(false);
        GetState().Update();
    });
}

FaceLight::Context FaceLight::GetContext() {
    const auto& state = GetState();
    return {state.session.load(), state.gameActive.load() && !state.loading.load(),
        state.gameThread.load() == GetCurrentThreadId()};
}

FaceLightingAPI::Result FaceLight::BeginTemporary(const FaceLightingAPI::V2::BeginInfo& info, FaceLightingAPI::V2::Session& output) {
    auto& state = GetState();
    return state.temporary.Begin(info, state.session.load(), Seconds(), output);
}
FaceLightingAPI::Result FaceLight::UpdateTemporary(const FaceLightingAPI::V2::UpdateInfo& info) {
    auto& state = GetState();
    const auto result = state.temporary.Update(info, state.session.load(), Seconds(), [&](const auto& token) {
        RE::NiPointer<RE::Actor> actor;
        return ResolveTemporary(token, state.session.load(), actor);
    });
    if (result == Result::Ok) {
        if (!state.temporary.Active()) state.ReleaseTemporary();
        RequestUpdate();
    }
    return result;
}
FaceLightingAPI::Result FaceLight::RenewTemporary(const FaceLightingAPI::V2::Session& session) {
    auto& state = GetState();
    return state.temporary.Renew(session, state.session.load(), Seconds());
}
FaceLightingAPI::Result FaceLight::EndTemporary(const FaceLightingAPI::V2::Session& session) {
    auto& state = GetState();
    const auto result = state.temporary.End(session, state.session.load(), Seconds());
    if (result == Result::Ok) { state.ReleaseTemporary(); RequestUpdate(); }
    return result;
}
FaceLightingAPI::Result FaceLight::QueryTemporary(const FaceLightingAPI::V2::Session& session, FaceLightingAPI::V2::SessionState& output) {
    auto& state = GetState();
    return state.temporary.Query(session, state.session.load(), Seconds(), output);
}

std::uint32_t FaceLight::RuntimeFlags(RE::Actor* actor) {
    using namespace FaceLightingAPI;
    std::uint32_t flags = 0;
    if (!ActorRuntime::SafeForLight(actor)) return Unsafe;
    const auto& state = GetState();
    const auto settings = Settings::GetActive();
    const auto cell = actor->GetParentCell();
    const auto root = actor->Get3D(false);
    if (cell && cell->IsAttached() && root) flags |= Loaded;
    if (ActorRuntime::SneakHidden(actor, settings.hideWhileSneaking)) flags |= HiddenSneak;
    const auto ui = RE::UI::GetSingleton();
    const bool dialogue = ui && ui->IsMenuOpen(RE::DialogueMenu::MENU_NAME);
    const auto topics = RE::MenuTopicManager::GetSingleton();
    const auto speaker = topics ? topics->speaker.get() : RE::NiPointer<RE::TESObjectREFR>{};
    const bool isSpeaker = dialogue && speaker.get() == actor && settings.dialogue.enabled && settings.dialogue.intensity > 0;
    const bool externalActive = state.temporary.Rendering(ConfigMenu::IsOpen() || Settings::HasPreview());
    const auto external = externalActive ? std::find_if(state.temporary.Targets().begin(), state.temporary.Targets().end(),
        [&](const auto& t) { return t.actor.formID == actor->GetFormID(); }) : state.temporary.Targets().end();
    const bool externalTarget = external != state.temporary.Targets().end();
    if (isSpeaker && !externalActive) flags |= Dialogue;
    const bool hiddenDialogueAmbient = !externalActive && isSpeaker && !state.dialogueAmbientAllowed;
    if (hiddenDialogueAmbient) flags |= HiddenAmbient;
    const LightInstance* light = nullptr;
    if (actor->IsPlayerRef()) {
        light = &state.playerLight;
        const auto camera = RE::PlayerCamera::GetSingleton();
        if (!camera || (camera->IsInFirstPerson() && !settings.firstPersonLight)) flags |= HiddenView;
        if (!externalTarget && !state.ambientAllowed) flags |= HiddenAmbient;
    } else {
        light = state.npcLights.Find(actor->GetHandle());
        if (externalActive ? !externalTarget : dialogue && !isSpeaker) flags |= HiddenDialogue;
        const auto selected = SelectedNPCs::PersonalEnabled(actor->GetFormID());
        const bool normalWanted = (isSpeaker && !hiddenDialogueAmbient) || (actor->IsPlayerTeammate() && settings.follower.enabled &&
            settings.follower.intensity > 0 && Followers::PersonalEnabled(actor->GetFormID())) ||
            (selected.value_or(false) && settings.selected.enabled && settings.selected.intensity > 0);
        const bool wanted = externalActive ? externalTarget && external->light.enabled && external->light.intensity > 0 : normalWanted;
        if (wanted && !light && (flags & Loaded) && !(flags & (HiddenDialogue | HiddenSneak | HiddenAmbient))) flags |= NotAllocated;
    }
    const auto camera = actor->IsPlayerRef() ? RE::PlayerCamera::GetSingleton() : nullptr;
    const bool cameraLight = camera && camera->IsInFirstPerson() && settings.firstPersonLight;
    static const RE::BSFixedString headName("NPC Head [Head]");
    const auto modelRoot = cameraLight ? camera->cameraRoot.get() : root;
    const auto headObject = modelRoot ? modelRoot->GetObjectByName(headName) : nullptr;
    const auto head = cameraLight ? (modelRoot ? modelRoot->AsNode() : nullptr) : (headObject ? headObject->AsNode() : nullptr);
    if ((flags & Loaded) && (!head || !std::isfinite(head->world.scale) || head->world.scale <= 0.0001f)) flags |= MissingModel;
    if (light && light->rendererLight) {
        flags |= Registered;
        const auto opacity = actor->IsPlayerRef() ? state.playerFade.value : state.npcLights.Opacity(actor->GetHandle());
        if (opacity > 0 && opacity < 1) flags |= Fading;
    }
    return flags;
}
