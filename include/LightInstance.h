#pragma once
#include <SKSE/SKSE.h>
#include <RE/Skyrim.h>
#include <cmath>
#include <optional>
#include "Settings.h"
#include "LightPlacement.h"
#include "CSLighting.h"
#include "ColorTemperature.h"

    // Scene ownership changes only in the player update hook or SKSE main-thread tasks.
    // Keep the holder until process exit, avoiding static destruction after the
    // game has already torn down its allocator. Clear releases it on transitions.
    struct LightInstance {
        RE::NiPointer<RE::NiAVObject> root;
        RE::NiPointer<RE::NiNode> head;
        RE::NiPointer<RE::ShadowSceneNode> scene;
        RE::NiPointer<RE::NiPointLight> light;
        RE::NiPointer<RE::BSLight> rendererLight;
        RE::FormID cellID = 0;
        std::optional<Settings::Values> applied;
        void Clear() {
            if (light) {
                light->GetLightRuntimeData().fade = 0.0f;
                if (scene && rendererLight) scene->RemoveLight(rendererLight);
                if (light->parent) light->parent->DetachChild(light.get());
                SKSE::log::debug("Face light removed");
            }
            rendererLight.reset();
            light.reset();
            head.reset();
            root.reset();
            scene.reset();
            cellID = 0;
            applied.reset();
        }

        void Update(RE::Actor* player, const Settings::Values& settings, float opacity = 1.0f) {
            if (!player || !settings.enabled || settings.intensity <= 0 || opacity <= 0) {
                Clear();
                return;
            }
            const auto cell = player->GetParentCell();
            const auto currentRoot = player->Get3D(false);
            const auto currentScene = RE::BSShaderManager::State::GetSingleton().shadowSceneNode[0];
            static const RE::BSFixedString headName("NPC Head [Head]");
            const auto headObject = currentRoot ? currentRoot->GetObjectByName(headName) : nullptr;
            const auto currentHead = headObject ? headObject->AsNode() : nullptr;
            if (!cell || !cell->IsAttached() || !currentRoot || !currentHead || !currentScene ||
                !std::isfinite(currentHead->world.scale) || currentHead->world.scale <= 0.0001f) {
                Clear();
                return;
            }
            if (root.get() != currentRoot || head.get() != currentHead || scene.get() != currentScene || cellID != cell->GetFormID()) {
                Clear();
            }
            const auto localOffset = LightPlacement::LocalOffset(currentHead->world, player->GetAngleZ(),
                {settings.offsetX, settings.offsetY, settings.offsetZ}, settings.followHeadRotation);
            const auto position = currentHead->world * localOffset;
            if (!std::isfinite(position.x) || !std::isfinite(position.y) || !std::isfinite(position.z)) {
                Clear();
                return;
            }
            // Recreate when entering/leaving CS so old extension flags cannot survive a fallback.
            if (applied && CSLighting::Available(applied->csMode) != CSLighting::Available(settings.csMode)) Clear();
            const bool creating = !light;
            if (creating) {
                light.reset(RE::NiPointLight::Create());
                if (!light) return;
                root.reset(currentRoot);
                head.reset(currentHead);
                scene.reset(currentScene);
                cellID = cell->GetFormID();
                light->name = player->IsPlayerRef() ? "FaceLighting_PlayerLight" : "FaceLighting_DialogueLight";
                auto& data = light->GetLightRuntimeData();
                data.ambient = {0.0f, 0.0f, 0.0f};
                data.diffuse = {1.0f, 1.0f, 1.0f};
                head->AttachChild(light.get());
            }
            light->local.translate = localOffset;
            // CS overlays ambient with flags/cutoff/formID and radius.z with size.
            // Publish only when settings change, preserving CS-owned state between frames.
            if (!applied || *applied != settings) {
                auto& data = light->GetLightRuntimeData();
                const bool cs = CSLighting::Available(settings.csMode);
                const bool inverse = cs && settings.csInverseSquare;
                const float fade = inverse ? CSLighting::InverseFade(settings.intensity) : settings.intensity;
                const auto range = CSLighting::InverseRange(fade, settings.manualRange, settings.inverseRadius);
                const auto radius = inverse ? range.radius : settings.radius;
                light->SetLightAttenuation(radius);
                data.radius.x = radius;
                if (cs) {
                    data.ambient.red = std::bit_cast<float>(CSLighting::Flags(
                        std::bit_cast<std::uint32_t>(data.ambient.red), inverse, settings.csLinear));
                    data.ambient.green = range.cutoff;
                    // ambient.blue is a FormID, left zero for our synthetic light.
                    data.radius.z = CSLighting::sourceSize;
                } else {
                    data.radius.y = radius;
                    data.radius.z = radius;
                }
                data.fade = fade;
                applied = settings;
                SKSE::log::debug("Face light applied: CS={}, inverse={}, linear={}, radius={}, intensity={}, engineFade={}",
                    cs, inverse, cs && (inverse || settings.csLinear), radius, settings.intensity, fade);
            }
            // Modulate emitted color, keeping the CS range based on target intensity.
            // This avoids a collapsing inverse-square radius during fade-out.
            const auto color = ColorTemperature::Get(settings.temperature,
                ColorTemperature::UseLinearColor(CSLighting::Available(settings.csMode), settings.csGlobalLinear,
                    settings.csInverseSquare, settings.csLinear));
            light->GetLightRuntimeData().diffuse = {color.r * opacity, color.g * opacity, color.b * opacity};
            RE::NiUpdateData update{};
            light->Update(update);
            if (creating) {
                RE::ShadowSceneNode::LIGHT_CREATE_PARAMS params{};
                params.dynamic = true;
                params.neverFades = true;
                params.fov = 1.0f;
                params.falloff = 1.0f;
                params.nearDistance = 5.0f;
                rendererLight.reset(scene->AddLight(light.get(), params));
                if (!rendererLight) {
                    SKSE::log::error("Could not register face light with renderer");
                    Clear();
                    return;
                }
                SKSE::log::debug("Face light created in cell {:08X}", cellID);
            }
        }
    };

