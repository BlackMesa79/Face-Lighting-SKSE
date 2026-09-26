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

namespace {
    struct State {
        LightInstance playerLight;
        LightTransition playerFade;
        PlayerDialoguePolicy playerDialogue;
        NpcLightManager<RE::ActorHandle, Settings::Values, LightInstance> npcLights{SelectedNPCs::limit + Followers::lightLimit + 2};
        std::atomic<bool> gameActive = false, loading = false, taskPending = false, resetRequested = false;
        std::chrono::steady_clock::time_point lastUpdate{};

        void Clear() {
            playerLight.Clear();
            playerFade = {};
            LightExclusionProbe::Reset();
            playerDialogue.Reset();
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
            const auto saved = Settings::Get();
            if (const auto enabled = playerDialogue.Update(dialogueOpen, saved.enablePlayerOnDialogue,
                saved.disablePlayerAfterDialogue, saved.enabled)) Settings::SetPlayerEnabled(*enabled);
            const auto settings = Settings::GetActive();
            const auto now = std::chrono::steady_clock::now();
            const float delta = lastUpdate == std::chrono::steady_clock::time_point{} ? 0.0f :
                std::chrono::duration<float>(now - lastUpdate).count();
            lastUpdate = now;
            const auto camera = RE::PlayerCamera::GetSingleton();
            LightExclusionProbe::Update(settings.exclusionDiagnostics || settings.ambientMode == 3,
                !ConfigMenu::IsOpen() && !ui->GameIsPaused() && !ui->IsMenuOpen(RE::Console::MENU_NAME), settings.exclusionDiagnostics);
            const bool ambientAllows = LightProbe::Update(player, settings, playerLight.rendererLight.get() != nullptr,
                npcLights.Size(), playerLight.rendererLight ? playerFade.value : 0.0f);
            const bool viewAllowed = ActorRuntime::SafeForLight(player) &&
                !ActorRuntime::SneakHidden(player, settings.hideWhileSneaking) &&
                camera && (!camera->IsInFirstPerson() || settings.firstPersonLight);
            if (viewAllowed) {
                const bool wanted = settings.enabled && ambientAllows && settings.intensity > 0 &&
                    !(settings.hideWhileSneaking && player->IsSneaking());
                playerFade.Update(wanted, settings.playerTransition, settings.playerDuration,
                    ui->GameIsPaused() ? 0.0f : std::clamp(delta, 0.0f, 0.1f));
                auto renderSettings = settings;
                // Manual off requests fade-out; the instance must stay enabled until opacity reaches zero.
                renderSettings.enabled = true;
                playerLight.Update(player, renderSettings, playerFade.value, camera->IsInFirstPerson());
            } else { playerLight.Clear(); playerFade = {}; }

            RE::ActorHandle target;
            const auto topics = RE::MenuTopicManager::GetSingleton();
            if (settings.dialogue.enabled && settings.dialogue.intensity > 0 && topics &&
                ui->IsMenuOpen(RE::DialogueMenu::MENU_NAME)) {
                const auto reference = topics->speaker.get();
                if (const auto actor = reference ? reference->As<RE::Actor>() : nullptr;
                    actor && actor != player) target = actor->GetHandle();
            }
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
                if (settings.follower.enabled && settings.follower.intensity > 0)
                    npcLights.Submit(actor, NpcLightSource::Follower, followerSettings, settings.follower.transition, settings.follower.duration);
            const auto selectedSettings = convert(settings.selected);
            npcLights.RefreshSource(NpcLightSource::Selected, selectedSettings, settings.selected.transition, settings.selected.duration);
            for (const auto& actor : SelectedNPCs::Update())
                if (settings.selected.enabled && settings.selected.intensity > 0)
                    npcLights.Submit(actor, NpcLightSource::Selected, selectedSettings, settings.selected.transition, settings.selected.duration);
            npcLights.RefreshSource(NpcLightSource::Dialogue, lightSettings, d.transition, d.duration);
            if (target) npcLights.Submit(target, NpcLightSource::Dialogue, lightSettings, d.transition, d.duration);
            // Four is our conservative secondary-light budget, not an engine limit.
            // Protect the player's surrounding geometry as well as their face.
            npcLights.Protect(4, [&](const RE::ActorHandle& handle, const Settings::Values& values) {
                const auto actor = handle.get();
                if (!actor || !ActorRuntime::Eligible(actor.get())) return false;
                if (!playerLight.rendererLight) return true;
                const auto extent = [](const Settings::Values& v) {
                    if (CSLighting::Available(v.csMode) && v.csInverseSquare)
                        return CSLighting::InverseRange(CSLighting::InverseFade(v.intensity), v.manualRange, v.inverseRadius).radius;
                    return v.radius;
                };
                const auto offset = [](const Settings::Values& v) {
                    return std::sqrt(v.offsetX * v.offsetX + v.offsetY * v.offsetY + v.offsetZ * v.offsetZ);
                };
                // A wider restoration boundary prevents repeated on/off at the edge.
                const float separation = extent(settings) + extent(values) + offset(settings) + offset(values) +
                    256.0f + (npcLights.Contains(handle) ? 0.0f : 64.0f);
                return actor->GetPosition().GetSquaredDistance(player->GetPosition()) > separation * separation;
            });
            npcLights.Update(delta, [&](const RE::ActorHandle& handle) {
                const auto actor = handle.get();
                const auto cell = actor ? actor->GetParentCell() : nullptr;
                return ActorRuntime::Eligible(actor.get()) &&
                    !ActorRuntime::SneakHidden(actor.get(), settings.hideWhileSneaking) &&
                    actor->Get3D(false) && cell && cell->IsAttached();
            }, [](LightInstance& light, const RE::ActorHandle& handle, const Settings::Values& values, float opacity) {
                const auto actor = handle.get();
                light.Update(actor.get(), values, opacity);
            });
        }
    };
    State& GetState() {
        static auto* state = new State;
        return *state;
    }

    REL::Relocation<void (*)(RE::PlayerCharacter*, float)> originalUpdate;
    void UpdatePlayer(RE::PlayerCharacter* player, float delta) {
        originalUpdate(player, delta);
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
                    GetState().gameActive.store(false);
                    SelectedNPCs::SetActive(false);
                    FaceLight::RequestUpdate();
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
