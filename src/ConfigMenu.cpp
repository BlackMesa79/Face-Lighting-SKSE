#include <SKSE/SKSE.h>
#include <RE/Skyrim.h>
#include <atomic>
#include "ConfigMenu.h"
#include "Settings.h"
#include "FaceLight.h"
#include "CSLighting.h"
#include "Hotkeys.h"
#include "Localization.h"
#include "SKSEMenuFramework.h"
#include "MenuStyle.h"
#include "SelectedNPCs.h"

namespace {
    std::atomic<bool> menuOpen = false;
    Settings::Values draft;
    const Localization::Text* status = &Localization::empty;
    bool registered = false;

    void __stdcall OnMenuEvent(SKSEMenuFramework::Model::EventType event) {
        if (event == SKSEMenuFramework::Model::kOpenMenu) {
            menuOpen.store(true);
            SelectedNPCs::CaptureTargets();
            draft = Settings::Get();
            Settings::ClearPreview();
            status = &Localization::empty;
        } else if (event == SKSEMenuFramework::Model::kCloseMenu) {
            menuOpen.store(false);
            Settings::ClearPreview();
            FaceLight::RequestUpdate();
        } else if (event == SKSEMenuFramework::Model::kBeforeRender && menuOpen.load()) {
            // Also refresh previews when a framework menu pauses actor updates.
            FaceLight::RequestUpdate();
        }
    }

    void RenderRoster() {
        const auto tr = [&](const Localization::Text& text) { return text.Get(draft.language); };
        const auto view = SelectedNPCs::Snapshot();
        MenuStyle::Heading(tr(Localization::rosterTitle));
        ImGuiMCP::TextWrapped("%s", tr(Localization::rosterHelp));
        ImGuiMCP::TextUnformatted(std::format("{} / {}", view.rows.size(), SelectedNPCs::limit).c_str());
        const auto target = [&](bool console, const std::string& name) {
            ImGuiMCP::BeginDisabled(!view.active || name.empty() || view.rows.size() >= SelectedNPCs::limit);
            if (ImGuiMCP::Button(tr(console ? Localization::addConsole : Localization::addCrosshair))) SelectedNPCs::AddTarget(console);
            ImGuiMCP::EndDisabled();
            ImGuiMCP::SameLine();
            ImGuiMCP::TextUnformatted(name.empty() ? tr(Localization::noTarget) : name.c_str());
        };
        target(false, view.crosshair); target(true, view.console);
        if (view.rows.empty()) ImGuiMCP::TextUnformatted(tr(Localization::rosterEmpty));
        for (const auto& row : view.rows) {
            ImGuiMCP::PushID(static_cast<int>(row.id));
            ImGuiMCP::Separator();
            const auto label = std::format("{} [{:08X}]", row.name, row.id);
            ImGuiMCP::TextUnformatted(label.c_str());
            ImGuiMCP::SameLine();
            ImGuiMCP::TextUnformatted(tr(row.loaded ? Localization::npcLoaded : Localization::npcUnloaded));
            bool enabled = row.enabled;
            if (ImGuiMCP::Checkbox(tr(Localization::rosterEnabled), &enabled)) SelectedNPCs::SetEnabled(row.id, enabled);
            ImGuiMCP::SameLine();
            if (ImGuiMCP::Button(tr(Localization::removeNPC))) SelectedNPCs::Remove(row.id);
            ImGuiMCP::PopID();
        }
        ImGuiMCP::Spacing();
    }
    enum class Page { basic, player, npc };

    void Render(Page page) {
        // Dialogue edges can update Enabled while other settings are being previewed.
        draft.enabled = Settings::GetActive().enabled;
        const auto tr = [&](const Localization::Text& text) { return text.Get(draft.language); };
        MenuStyle::Scope style;
        MenuStyle::Heading(tr(page == Page::basic ? Localization::basicEntry : page == Page::player ? Localization::playerEntry : Localization::npcEntry));
        bool changed = false;
        if (page == Page::basic) {
            std::vector<const char*> names{tr(Localization::autoLanguage)};
            std::vector<std::string> codes{"auto"};
            int selected = 0;
            for (const auto& language : Localization::Languages()) {
                codes.push_back(language.code);
                names.push_back(language.name.c_str());
                if (draft.language == language.code) selected = static_cast<int>(codes.size()) - 1;
            }
            // Preserve an explicit missing language rather than misrepresenting it as Auto.
            if (draft.language != "auto" && selected == 0) {
                codes.push_back(draft.language);
                names.push_back(draft.language.c_str());
                selected = static_cast<int>(codes.size()) - 1;
            }
            if (ImGuiMCP::Combo(tr(Localization::languageLabel), &selected, names.data(), static_cast<int>(names.size()))) {
                draft.language = codes[selected];
                changed = true;
            }
        }
        if (page == Page::basic) {
            MenuStyle::Heading(tr(Localization::sectionCompatibility));
            const char* modes[] = {tr(Localization::modeAuto), tr(Localization::modeManualEnable), tr(Localization::modeDisabled)};

            changed |= ImGuiMCP::Combo(tr(Localization::csMode), &draft.csMode, modes, 3);
            ImGuiMCP::TextWrapped("%s", tr(Localization::csModeHelp));
            ImGuiMCP::TextUnformatted(CSLighting::Diagnostics(draft.language).c_str());
            ImGuiMCP::TextWrapped("%s", tr(Localization::languageHelp));
            changed |= ImGuiMCP::Checkbox(tr(Localization::debug), &draft.debugLogging);
            ImGuiMCP::TextUnformatted(CSLighting::Status(draft.language, draft.csMode));
            changed |= ImGuiMCP::Checkbox(tr(Localization::globalLinear), &draft.csGlobalLinear);
            ImGuiMCP::TextWrapped("%s", tr(Localization::globalLinearHelp));
            ImGuiMCP::TextWrapped("%s", tr(Localization::linearHelp));
        }
        if (page == Page::player) {
            if (MenuStyle::Section(tr(Localization::sectionKeys), false)) {
            int keyIndex = Hotkeys::Index(draft.hotkey);
            const auto custom = std::format("{} ({})", tr(Localization::customKey), draft.hotkey);
            constexpr int knownKeyCount = static_cast<int>(Hotkeys::codes.size());
            std::array<const char*, Hotkeys::codes.size() + 1> keys{};
            std::copy(std::begin(Hotkeys::names), std::end(Hotkeys::names), keys.begin());
            keys[0] = tr(Localization::keyOff);
            keys[knownKeyCount] = custom.c_str();
            const int count = knownKeyCount + (keyIndex == knownKeyCount ? 1 : 0);
            if (ImGuiMCP::Combo(tr(Localization::hotkey), &keyIndex, keys.data(), count)) {
                draft.hotkey = Hotkeys::FromIndex(keyIndex, draft.hotkey);
                changed = true;
            }
            ImGuiMCP::TextWrapped("%s", tr(Localization::hotkeyHelp));
            const char* modifiers[] = {tr(Localization::noModifier), tr(Localization::shiftKey), tr(Localization::ctrlKey), tr(Localization::altKey)};
            changed |= ImGuiMCP::Combo(tr(Localization::modifier), &draft.hotkeyModifier, modifiers, 4);
            ImGuiMCP::TextWrapped("%s", tr(Localization::modifierHelp));
            std::array<const char*, 17> padLabels{};
            const Localization::Text* padTexts[]{&Localization::keyOff,&Localization::padButton1,&Localization::padButton2,&Localization::padButton3,&Localization::padButton4,&Localization::padButton5,&Localization::padButton6,&Localization::padButton7,&Localization::padButton8,&Localization::padButton9,&Localization::padButton10,&Localization::padButton11,&Localization::padButton12,&Localization::padButton13,&Localization::padButton14,&Localization::padButton15,&Localization::padButton16};
            for (int i = 0; i <= 16; ++i) padLabels[i] = tr(*padTexts[i]);
            padLabels[0] = tr(Localization::keyOff);
            int padKey = Hotkeys::PadIndex(draft.gamepadKey);
            if (ImGuiMCP::Combo(tr(Localization::padKey), &padKey, padLabels.data(), 17)) {
                draft.gamepadKey = Hotkeys::PadCode(padKey);
                if (draft.gamepadModifier == draft.gamepadKey) draft.gamepadModifier = 0;
                changed = true;
            }
            // Omit the main key from modifier choices.
            std::array<int, 17> padCodes{};
            int padCount = 1, padModifier = 0;
            padLabels[0] = tr(Localization::noModifier);
            for (int i = 1; i <= 16; ++i) {
                if (Hotkeys::PadCode(i) == draft.gamepadKey) continue;
                padCodes[padCount] = Hotkeys::PadCode(i);
                padLabels[padCount] = tr(*padTexts[i]);
                if (padCodes[padCount] == draft.gamepadModifier) padModifier = padCount;
                ++padCount;
            }
            if (ImGuiMCP::Combo(tr(Localization::padModifier), &padModifier, padLabels.data(), padCount)) {
                draft.gamepadModifier = padCodes[padModifier];
                changed = true;
            }
            ImGuiMCP::TextWrapped("%s", tr(Localization::padHelp));
            }
            if (MenuStyle::Section(tr(Localization::sectionBehavior))) {
            changed |= ImGuiMCP::Checkbox(tr(Localization::sneakHide), &draft.hideWhileSneaking);
            ImGuiMCP::TextWrapped("%s", tr(Localization::sneakHelp));

            changed |= ImGuiMCP::Checkbox(tr(Localization::enabled), &draft.enabled);
            changed |= ImGuiMCP::Checkbox(tr(Localization::playerDialogueOn), &draft.enablePlayerOnDialogue);
            changed |= ImGuiMCP::Checkbox(tr(Localization::playerDialogueOff), &draft.disablePlayerAfterDialogue);
            ImGuiMCP::TextWrapped("%s", tr(Localization::playerDialogueHelp));
            }
            if (MenuStyle::Section(tr(Localization::sectionLighting))) {
            changed |= ImGuiMCP::SliderFloat(tr(Localization::temperature), &draft.temperature, 2000.0f, 10000.0f, "%.0f K");
            ImGuiMCP::TextUnformatted(CSLighting::Status(draft.language, draft.csMode));
            if (CSLighting::Available(draft.csMode)) {
                changed |= ImGuiMCP::Checkbox(tr(Localization::inverse), &draft.csInverseSquare);
                if (draft.csInverseSquare) {
                    ImGuiMCP::TextWrapped("%s", tr(Localization::automatic));
                } else {
                    changed |= ImGuiMCP::Checkbox(tr(Localization::linear), &draft.csLinear);
                }
                ImGuiMCP::TextWrapped("%s", tr(Localization::linearHelp));
            }
            if (!CSLighting::Available(draft.csMode) || !draft.csInverseSquare) {
                changed |= ImGuiMCP::SliderFloat(tr(Localization::radius), &draft.radius, Settings::minRadius, Settings::maxRadius, "%.1f");
            }
            changed |= ImGuiMCP::SliderFloat(tr(Localization::intensity), &draft.intensity, 0.0f, Settings::maxIntensity, "%.2f");
            if (CSLighting::Available(draft.csMode) && draft.csInverseSquare) {
                const char* options[] = {tr(Localization::modeAuto), tr(Localization::modeManual)};
                int mode = draft.manualRange ? 1 : 0;
                if (ImGuiMCP::Combo(tr(Localization::rangeMode), &mode, options, 2)) { draft.manualRange = mode == 1; changed = true; }
                if (draft.manualRange) {
                    changed |= ImGuiMCP::SliderFloat(tr(Localization::inverseRadius), &draft.inverseRadius, Settings::minRadius, Settings::maxRadius, "%.1f");
                    ImGuiMCP::TextWrapped("%s", tr(Localization::manualRangeHelp));
                }
                const auto clean = Settings::Normalize(draft);
                const auto range = CSLighting::InverseRange(CSLighting::InverseFade(clean.intensity), clean.manualRange, clean.inverseRadius);
                const auto text = std::format("{}{:.1f}", tr(Localization::range), draft.intensity > 0 ? range.radius : 0.0f);
                ImGuiMCP::TextUnformatted(text.c_str());
                if (!draft.manualRange) ImGuiMCP::TextWrapped("%s", tr(Localization::rangeHelp));
            }
            }
            if (MenuStyle::Section(tr(Localization::sectionPosition), false)) {
            changed |= ImGuiMCP::Checkbox(tr(Localization::followHead), &draft.followHeadRotation);
            changed |= ImGuiMCP::SliderFloat(tr(draft.followHeadRotation ? Localization::boneX : Localization::right), &draft.offsetX, -Settings::maxOffset, Settings::maxOffset, "%.1f");
            changed |= ImGuiMCP::SliderFloat(tr(draft.followHeadRotation ? Localization::boneY : Localization::front), &draft.offsetY, -Settings::maxOffset, Settings::maxOffset, "%.1f");
            changed |= ImGuiMCP::SliderFloat(tr(draft.followHeadRotation ? Localization::boneZ : Localization::up), &draft.offsetZ, -Settings::maxOffset, Settings::maxOffset, "%.1f");
            ImGuiMCP::TextWrapped("%s", tr(draft.followHeadRotation ? Localization::boneHelp : Localization::placement));
            }
            }
        if (page == Page::npc) {
        static bool selectedPanel = false;
        if (ImGuiMCP::Button(tr(Localization::dialogueTitle))) selectedPanel = false;
        ImGuiMCP::SameLine();
        if (ImGuiMCP::Button(tr(Localization::selectedTitle))) selectedPanel = true;
        MenuStyle::Heading(tr(selectedPanel ? Localization::selectedTitle : Localization::dialogueTitle));
        if (selectedPanel) {
            RenderRoster();
            ImGuiMCP::TextWrapped("%s", tr(Localization::selectedHelp));
        }
        ImGuiMCP::PushID(selectedPanel ? "SelectedSettings" : "DialogueSettings");
        auto& d = selectedPanel ? draft.selected : draft.dialogue;
        if (MenuStyle::Section(tr(Localization::sectionBehavior))) {

        changed |= ImGuiMCP::Checkbox(tr(selectedPanel ? Localization::selectedEnabled : Localization::dialogueEnabled), &d.enabled);
        changed |= ImGuiMCP::Checkbox(tr(Localization::dialogueTransition), &d.transition);
        if (d.transition) changed |= ImGuiMCP::SliderFloat(tr(Localization::dialogueDuration), &d.duration, 0.0f, 3.0f, "%.2f");
        }
        if (MenuStyle::Section(tr(Localization::sectionLighting))) {
        changed |= ImGuiMCP::SliderFloat(tr(Localization::dialogueTemperature), &d.temperature, 2000.0f, 10000.0f, "%.0f K");
        if (CSLighting::Available(draft.csMode)) {
            changed |= ImGuiMCP::Checkbox(tr(Localization::dialogueInverse), &d.csInverseSquare);
            if (d.csInverseSquare) ImGuiMCP::TextWrapped("%s", tr(Localization::automatic));
            else changed |= ImGuiMCP::Checkbox(tr(Localization::dialogueLinear), &d.csLinear);
        }
        if (!CSLighting::Available(draft.csMode) || !d.csInverseSquare)
            changed |= ImGuiMCP::SliderFloat(tr(Localization::dialogueRadius), &d.radius, Settings::minRadius, Settings::maxRadius, "%.1f");
        changed |= ImGuiMCP::SliderFloat(tr(Localization::dialogueIntensity), &d.intensity, 0.0f, Settings::maxIntensity, "%.2f");
        if (CSLighting::Available(draft.csMode) && d.csInverseSquare) {
            const char* options[] = {tr(Localization::modeAuto), tr(Localization::modeManual)};
            int mode = d.manualRange ? 1 : 0;
            if (ImGuiMCP::Combo(tr(Localization::dialogueRangeMode), &mode, options, 2)) { d.manualRange = mode == 1; changed = true; }
            if (d.manualRange) {
                changed |= ImGuiMCP::SliderFloat(tr(Localization::dialogueInverseRadius), &d.inverseRadius, Settings::minRadius, Settings::maxRadius, "%.1f");
                ImGuiMCP::TextWrapped("%s", tr(Localization::manualRangeHelp));
            }
            const auto normalized = Settings::Normalize(draft);
            const auto& clean = selectedPanel ? normalized.selected : normalized.dialogue;
            const auto radius = CSLighting::InverseRange(CSLighting::InverseFade(clean.intensity), clean.manualRange, clean.inverseRadius).radius;
            const auto text = std::format("{}{:.1f}", tr(Localization::range), d.intensity > 0 ? radius : 0.0f);
            ImGuiMCP::TextUnformatted(text.c_str());
        }
        }
        if (MenuStyle::Section(tr(Localization::sectionPosition), false)) {
        changed |= ImGuiMCP::Checkbox(tr(Localization::dialogueFollowHead), &d.followHeadRotation);
        changed |= ImGuiMCP::SliderFloat(tr(d.followHeadRotation ? Localization::dialogueBoneX : Localization::dialogueX), &d.offsetX, -Settings::maxOffset, Settings::maxOffset, "%.1f");
        changed |= ImGuiMCP::SliderFloat(tr(d.followHeadRotation ? Localization::dialogueBoneY : Localization::dialogueY), &d.offsetY, -Settings::maxOffset, Settings::maxOffset, "%.1f");
        changed |= ImGuiMCP::SliderFloat(tr(d.followHeadRotation ? Localization::dialogueBoneZ : Localization::dialogueZ), &d.offsetZ, -Settings::maxOffset, Settings::maxOffset, "%.1f");
        ImGuiMCP::TextWrapped("%s", tr(d.followHeadRotation ? Localization::boneHelp : Localization::dialogueHelp));
        }
        ImGuiMCP::PopID();
        }
        MenuStyle::Heading(tr(Localization::sectionActions));
        ImGuiMCP::TextWrapped("%s", tr(Localization::sharedActionsHelp));
        ImGuiMCP::TextWrapped("%s", tr(Localization::previewHelp));
        if (changed) {
            draft = Settings::Normalize(draft);
            Settings::Preview(draft);
            FaceLight::RequestUpdate();
            status = &Localization::previewing;
        }
        if (ImGuiMCP::Button(tr(Localization::saveAll))) {
            status = Settings::Save(draft) ? &Localization::saved : &Localization::failed;
            FaceLight::RequestUpdate();
        }
        ImGuiMCP::SameLine();
        if (ImGuiMCP::Button(tr(Localization::defaultsAll))) {
            draft = Settings::Values{};
            Settings::Preview(draft);
            FaceLight::RequestUpdate();
            status = &Localization::restored;
        }
        ImGuiMCP::SameLine();
        if (ImGuiMCP::Button(tr(Localization::discardAll))) {
            SelectedNPCs::CaptureTargets();
            draft = Settings::Get();
            Settings::ClearPreview();
            FaceLight::RequestUpdate();
            status = &Localization::discarded;
        }
        ImGuiMCP::TextUnformatted(status->Get(draft.language));
    }
    void __stdcall RenderBasic() { Render(Page::basic); }
    void __stdcall RenderPlayer() { Render(Page::player); }
    void __stdcall RenderNPC() { Render(Page::npc); }
}

bool ConfigMenu::IsOpen() { return menuOpen.load(); }

void ConfigMenu::Register() {
    if (registered) return;
    const auto module = GetMenuFrameworkModule();
    if (!module) {
        SKSE::log::warn("SKSE Menu Framework not loaded; FaceLighting menu unavailable, INI configuration remains available");
        return;
    }
    for (const auto name : {"AddSectionItem", "RegisterEventPriority", "igTextUnformatted", "igCheckbox", "igSliderFloat", "igTextWrappedV", "igButton", "igSameLine", "igCombo_Str_arr", "igPushID_Str", "igPushID_Int", "igPopID", "igBeginDisabled", "igEndDisabled", "igSpacing", "igSeparator", "igTextColoredV", "igPushStyleColor_Vec4", "igPopStyleColor", "igPushStyleVar_Float", "igPushStyleVar_Vec2", "igPopStyleVar", "igGetContentRegionAvail", "igPushItemWidth", "igPopItemWidth", "igCollapsingHeader_TreeNodeFlags"}) {
        if (!GetProcAddress(module, name)) {
            SKSE::log::warn("Menu Framework missing export {}; skipping menu registration", name);
            return;
        }
    }
    draft = Settings::Get();

    SKSEMenuFramework::SetSection(Localization::sectionName.Get(draft.language));
    // '/' is the framework's hierarchy separator, not a label separator.
    SKSEMenuFramework::AddSectionItem(Localization::basicEntry.Get(draft.language), RenderBasic);
    SKSEMenuFramework::AddSectionItem(Localization::playerEntry.Get(draft.language), RenderPlayer);
    SKSEMenuFramework::AddSectionItem(Localization::npcEntry.Get(draft.language), RenderNPC);
    SKSEMenuFramework::AddEvent(OnMenuEvent, 0.0f);
    registered = true;
    SKSE::log::info("FaceLighting configuration menu registered");
}












