#pragma once
#include <string>

namespace Settings {
    struct Dialogue {
        bool followHeadRotation = false;
        bool manualRange = false;
        float inverseRadius = 100.0f;
        bool enabled = true;
        bool transition = true;
        bool csInverseSquare = true;
        bool csLinear = true;
        float duration = 0.3f;
        float radius = 100.0f;
        float intensity = 1.0f;
        float temperature = 6500.0f;
        float offsetX = 0.0f, offsetY = 40.0f, offsetZ = 5.0f;
        bool operator==(const Dialogue&) const = default;
    };
    struct Values {
        bool enablePlayerOnDialogue = true;
        bool disablePlayerAfterDialogue = false;
        bool followHeadRotation = false;
        bool manualRange = false;
        float inverseRadius = 100.0f;
        Dialogue dialogue;
        Dialogue selected;
        Dialogue follower;
        bool firstPersonLight = false;
        bool lightDiagnostics = false;
        bool exclusionDiagnostics = false; // Passive call-site probe; enable at startup only.
        bool playerTransition = true;
        float playerDuration = 0.2f;
        int ambientMode = 0; // 0 disabled, 1 observe compensation, 2 fixed compensation, 3 live exclusion
        float ambientCompensation = 80.0f; // Measured raw delta at the current light settings.
        float ambientOnThreshold = 30.0f;
        float ambientOffThreshold = 50.0f;
        float ambientDelay = 2.0f;
        bool rosterNotifications = true;
        bool enabled = false;
        int hotkey = 38; // DirectInput scan code: L; 0 disables the shortcut.
        int hotkeyModifier = 0; // 0 none, 1 Shift, 2 Ctrl, 3 Alt
        int selectedHotkey = 38; // L
        int selectedHotkeyModifier = 1; // Shift
        int gamepadKey = 0; // SKSE unified gamepad codes 266..281; 0 disabled
        int gamepadModifier = 0;
        bool hideWhileSneaking = true;
        std::string language = "auto"; // Language file code or Windows UI language.
        bool debugLogging = false;
        int csMode = 1; // 1 enabled, 2 disabled; legacy 0 migrates to enabled
        bool csGlobalLinear = false; // User confirmation; CS presence does not imply Linear Lighting.
        bool csInverseSquare = false;
        bool csLinear = false;
        float radius = 100.0f;
        float intensity = 1.0f;
        float temperature = 6500.0f;
        float offsetX = 0.0f;
        float offsetY = 40.0f;
        float offsetZ = 5.0f;
        bool operator==(const Values&) const = default;
    };

    inline constexpr float minRadius = 10.0f, maxRadius = 500.0f;
    inline constexpr float maxIntensity = 5.0f, maxOffset = 150.0f;
    Values Normalize(Values values);
    Values Get();
    Values GetActive();
    void Preview(const Values& values);
    void ClearPreview();
    void Load();
    bool Save(const Values& values);
    bool SetPlayerEnabled(bool enabled);
}


