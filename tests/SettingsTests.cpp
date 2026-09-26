#include "Settings.h"
#include "Localization.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
    void Check(bool result, const char* description) {
        if (!result) throw std::runtime_error(description);
    }
    void Write(const char* text) {
        std::ofstream file("Data/SKSE/Plugins/FaceLighting.ini");
        file << text;
    }
}

int main() {
    try {
        const auto testRoot = std::filesystem::current_path() / "build" / "settings-test";
        std::filesystem::create_directories(testRoot / "Data/SKSE/Plugins");
        std::filesystem::current_path(testRoot);
        const Settings::Values defaults;
        Settings::Values requested;
        auto legacyCS = defaults;
        legacyCS.csMode = 0;
        Check(Settings::Normalize(legacyCS).csMode == 1, "legacyCS CS auto migrates to enabled");
        legacyCS.csMode = 2;
        Check(Settings::Normalize(legacyCS).csMode == 2, "legacyCS CS disabled remains disabled");

        requested.enabled = false;
        requested.enablePlayerOnDialogue = false;
        requested.disablePlayerAfterDialogue = true;
        requested.followHeadRotation = true;
        requested.dialogue.followHeadRotation = true;
        requested.language = "en";
        requested.hotkeyModifier = 1;
        requested.selectedHotkey = 60;
        requested.selectedHotkeyModifier = 2;
        requested.gamepadKey = 266;
        requested.gamepadModifier = 274;
        requested.hideWhileSneaking = false;
        requested.manualRange = true;
        requested.inverseRadius = 250;
        requested.dialogue.manualRange = true;
        requested.dialogue.inverseRadius = 75;
        requested.csMode = 1;
        requested.csGlobalLinear = true;
        requested.enabled = true;
        requested.hotkey = 59;
        requested.temperature = 3200;
        requested.dialogue.temperature = 8000;
        requested.dialogue.enabled = false;
        requested.dialogue.transition = false;
        requested.dialogue.csInverseSquare = false;
        requested.dialogue.csLinear = false;
        requested.dialogue.duration = 1.25f;
        requested.dialogue.radius = 125.0f;
        requested.dialogue.intensity = 0.75f;
        requested.dialogue.offsetX = -15.0f;
        requested.dialogue.offsetY = 35.0f;
        requested.dialogue.offsetZ = 8.0f;
        requested.selected = requested.dialogue;
        requested.selected.radius = 180.0f;
        requested.selected.temperature = 4500.0f;
        requested.follower = requested.dialogue;
        requested.follower.radius = 210.0f;
        requested.follower.temperature = 5100.0f;
        requested.firstPersonLight = true;
        requested.lightDiagnostics = true;
        requested.exclusionDiagnostics = true;
        requested.playerTransition = false;
        requested.playerDuration = 1.25f;
        requested.ambientMode = 3;
        requested.ambientCompensation = 83.5f;
        requested.ambientOnThreshold = 28;
        requested.ambientOffThreshold = 55;
        requested.ambientDelay = 3;
        requested.rosterNotifications = false;
        requested.debugLogging = true;
        requested.csInverseSquare = true;
        requested.csLinear = true;
        requested.radius = 155.5f;
        requested.intensity = 2.25f;
        requested.offsetX = -12.5f;
        requested.offsetY = 55.75f;
        requested.offsetZ = -8.25f;
        Check(Settings::Save(requested), "save non-default values");
        Settings::Load();
        Check(Settings::Get() == requested, "all fields round-trip");
        requested.language = "zh-cn";
        requested.csMode = 2;
        Check(Settings::Save(requested), "save Chinese override");
        Settings::Load();
        Check(Settings::Get() == requested, "Chinese language round-trip");
        requested.language = "fr-fr";
        Check(Settings::Save(requested), "save additional language");
        Settings::Load();
        Check(Settings::Get() == requested, "additional language round-trip");

        Settings::Preview(defaults);
        Check(Settings::SetPlayerEnabled(false), "dialogue switch saves independently");
        requested.enabled = false;
        Check(Settings::Get() == requested && Settings::GetActive() == defaults,
            "dialogue switch preserves all unrelated preview and saved fields");
        Settings::Load();
        Check(Settings::Get() == requested, "dialogue switch persists across reload");

        Settings::Preview(defaults);
        Check(Settings::GetActive() == defaults && Settings::Get() == requested, "preview leaves saved values intact");
        Settings::ClearPreview();
        Check(Settings::GetActive() == requested, "discard preview restores saved values");
        Settings::Preview(defaults);
        Check(Settings::Save(defaults), "save preview");
        Settings::ClearPreview();
        Check(Settings::GetActive() == defaults, "saved preview survives close");

        Write("[General]\nDebugLogging=1\n");
        Settings::Load();
        auto legacy = defaults;
        legacy.debugLogging = true;
        Check(Settings::Get() == legacy, "0.1 configuration gains lighting defaults");
        Check(Settings::Get().follower.enabled && !Settings::Get().firstPersonLight,
            "legacy settings enable followers but keep first-person opt-in");
        auto invalidFollower = defaults;
        invalidFollower.follower.intensity = std::numeric_limits<float>::quiet_NaN();
        invalidFollower.follower.radius = -10;
        invalidFollower.follower.duration = 20;
        const auto cleanFollower = Settings::Normalize(invalidFollower);
        Check(cleanFollower.follower.intensity == defaults.follower.intensity &&
            cleanFollower.follower.radius == Settings::minRadius && cleanFollower.follower.duration == 3,
            "follower lighting parameters are normalized");
        auto invalidKeys = defaults;
        invalidKeys.selectedHotkey = 256;
        invalidKeys.selectedHotkeyModifier = 4;
        Check(Settings::Normalize(invalidKeys) == defaults, "invalid NPC shortcut restores defaults");
        invalidKeys.selectedHotkey = 42;
        invalidKeys.selectedHotkeyModifier = 1;
        Check(Settings::Normalize(invalidKeys) == defaults, "NPC main key cannot equal modifier");
        invalidKeys.selectedHotkey = 0;
        Check(Settings::Normalize(invalidKeys).selectedHotkey == 0, "NPC shortcut can be disabled");
        Check(Settings::Get().enablePlayerOnDialogue, "missing dialogue entry flag defaults on");
        Write("[General]\nEnablePlayerOnDialogue=0\n");
        Settings::Load();
        Check(!Settings::Get().enablePlayerOnDialogue, "explicit dialogue opt-out preserved");
        Write("[General]\nPlayerGamepadKey=266\nPlayerGamepadModifier=266\n");
        Settings::Load();
        Check(Settings::Get().gamepadKey == 266 && Settings::Get().gamepadModifier == 0,
            "same gamepad main and modifier clears modifier");
        Write("[General]\nPlayerGamepadKey=265\nPlayerGamepadModifier=282\n");
        Settings::Load();
        Check(Settings::Get().gamepadKey == 0 && Settings::Get().gamepadModifier == 0,
            "invalid gamepad bindings disabled");
        Write("[General]\nLanguage=../unsupported\n");
        Settings::Load();
        Check(Settings::Get().language == "auto", "unknown language falls back to auto");
        Write("[General]\nRadius=180\nIntensity=2\n");
        Settings::Load();
        Check(!Settings::Get().csInverseSquare && !Settings::Get().csLinear &&
            Settings::Get().radius == 180 && Settings::Get().intensity == 2, "0.2 configuration preserves light settings without enabling CS modes");
        Write("[General]\nRadius=nan\nIntensity=inf\nOffsetX=1oops\nOffsetY=1e999\nOffsetZ=invalid\n");
        Settings::Load();
        Check(Settings::Get() == defaults, "malformed and non-finite numbers use defaults");
        Write("[General]\nRadius=-100\nIntensity=99\nOffsetX=-999\nOffsetY=999\nOffsetZ=-999\n");
        Settings::Load();
        auto clamped = Settings::Get();
        Check(clamped.radius == Settings::minRadius && clamped.intensity == Settings::maxIntensity &&
            clamped.offsetX == -Settings::maxOffset && clamped.offsetY == Settings::maxOffset &&
            clamped.offsetZ == -Settings::maxOffset, "out-of-range INI clamped");
        auto invalid = defaults;
        invalid.radius = std::numeric_limits<float>::quiet_NaN();
        invalid.offsetY = std::numeric_limits<float>::infinity();
        invalid.intensity = -10;
        Settings::Preview(invalid);
        Check(Settings::GetActive().radius == defaults.radius && Settings::GetActive().offsetY == defaults.offsetY &&
            Settings::GetActive().intensity == 0, "preview input sanitized");

        // Guaranteed write failure: the INI parent is a regular file.
        auto ambientInvalid = defaults;
        ambientInvalid.playerDuration = std::numeric_limits<float>::infinity();
        ambientInvalid.ambientMode = 9;
        ambientInvalid.ambientCompensation = std::numeric_limits<float>::quiet_NaN();
        ambientInvalid.ambientOnThreshold = 900;
        ambientInvalid.ambientOffThreshold = 10;
        ambientInvalid.ambientDelay = -2;
        const auto ambientClean = Settings::Normalize(ambientInvalid);
        Check(ambientClean.playerDuration == defaults.playerDuration, "invalid player transition duration defaults");
        Check(ambientClean.ambientMode == 0 && ambientClean.ambientCompensation == 80 &&
            ambientClean.ambientOnThreshold == 900 && ambientClean.ambientOffThreshold == 901 &&
            ambientClean.ambientDelay == 0.5f, "ambient thresholds ordered and malformed values sanitized");
        std::filesystem::create_directories(testRoot / "blocked/Data/SKSE");
        { std::ofstream file(testRoot / "blocked/Data/SKSE/Plugins"); file << "blocked"; }
        std::filesystem::current_path(testRoot / "blocked");
        Check(!Settings::SetPlayerEnabled(!clamped.enabled), "dialogue write failure reported");
        Check(Settings::Get() == clamped, "failed dialogue write leaves saved state intact");
        Check(!Settings::Save(requested), "save failure reported");
        Check(Settings::Get() == clamped && Settings::GetActive() == clamped, "save failure rolls back preview");
        Settings::Load();
        Check(Settings::Get() == defaults, "missing INI uses defaults");
        std::cout << "Settings persistence, preview/rollback, migration and numeric validation passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}

