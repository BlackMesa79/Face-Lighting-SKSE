#include <SKSE/SKSE.h>
#include <Windows.h>
#include <algorithm>
#include <charconv>
#include <cmath>
#include <mutex>
#include <optional>
#include "Settings.h"
#include "Hotkeys.h"
#include "Localization.h"

namespace {
    std::mutex settingsMutex;
    Settings::Values current;
    std::optional<Settings::Values> preview;
    constexpr auto path = ".\\Data\\SKSE\\Plugins\\FaceLighting.ini";

    void Publish(const Settings::Values& values) {
        current = values;
        preview.reset();
        spdlog::set_level(values.debugLogging ? spdlog::level::debug : spdlog::level::info);
    }

    float ReadFloat(const char* key, float fallback) {
        char buffer[128]{};
        GetPrivateProfileStringA("General", key, "", buffer, sizeof(buffer), path);
        std::string_view text(buffer);
        if (text.empty()) return fallback;
        float result{};
        const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), result);
        if (error != std::errc{} || end != text.data() + text.size() || !std::isfinite(result)) {
            SKSE::log::warn("Invalid {} value '{}'; using {}", key, text, fallback);
            return fallback;
        }
        return result;
    }
}

Settings::Values Settings::Normalize(Values values) {
    const Values defaults;
    if (!Hotkeys::ValidPadCode(values.gamepadKey)) values.gamepadKey = 0;
    if (!Hotkeys::ValidPadCode(values.gamepadModifier)) values.gamepadModifier = 0;
    if (values.gamepadKey != 0 && values.gamepadKey == values.gamepadModifier) values.gamepadModifier = 0;
    if (values.csMode < 0 || values.csMode > 2) values.csMode = 0;
    if (values.hotkey < 0 || values.hotkey > 255) values.hotkey = 38;
    if (values.hotkeyModifier < 0 || values.hotkeyModifier > 3) values.hotkeyModifier = 0;
    if (values.hotkeyModifier && Hotkeys::ModifierMask(values.hotkey) == (1u << (values.hotkeyModifier - 1)))
        values.hotkey = 38;
    values.language = Localization::NormalizeCode(values.language);
    const auto clamp = [](float value, float low, float high, float fallback) {
        return std::isfinite(value) ? std::clamp(value, low, high) : fallback;
    };
    values.radius = clamp(values.radius, minRadius, maxRadius, defaults.radius);
    values.inverseRadius = clamp(values.inverseRadius, minRadius, maxRadius, defaults.inverseRadius);
    values.dialogue.inverseRadius = clamp(values.dialogue.inverseRadius, minRadius, maxRadius, defaults.dialogue.inverseRadius);
    values.intensity = clamp(values.intensity, 0.0f, maxIntensity, defaults.intensity);
    values.offsetX = clamp(values.offsetX, -maxOffset, maxOffset, defaults.offsetX);
    values.offsetY = clamp(values.offsetY, -maxOffset, maxOffset, defaults.offsetY);
    values.offsetZ = clamp(values.offsetZ, -maxOffset, maxOffset, defaults.offsetZ);
    values.dialogue.radius = clamp(values.dialogue.radius, minRadius, maxRadius, defaults.dialogue.radius);
    values.dialogue.intensity = clamp(values.dialogue.intensity, 0.0f, maxIntensity, defaults.dialogue.intensity);
    values.dialogue.offsetX = clamp(values.dialogue.offsetX, -maxOffset, maxOffset, defaults.dialogue.offsetX);
    values.dialogue.offsetY = clamp(values.dialogue.offsetY, -maxOffset, maxOffset, defaults.dialogue.offsetY);
    values.dialogue.offsetZ = clamp(values.dialogue.offsetZ, -maxOffset, maxOffset, defaults.dialogue.offsetZ);
    values.dialogue.duration = clamp(values.dialogue.duration, 0.0f, 3.0f, defaults.dialogue.duration);
    values.temperature = clamp(values.temperature, 2000.0f, 10000.0f, defaults.temperature);
    values.dialogue.temperature = clamp(values.dialogue.temperature, 2000.0f, 10000.0f, defaults.dialogue.temperature);
    values.selected.inverseRadius = clamp(values.selected.inverseRadius, minRadius, maxRadius, defaults.selected.inverseRadius);
    values.selected.radius = clamp(values.selected.radius, minRadius, maxRadius, defaults.selected.radius);
    values.selected.intensity = clamp(values.selected.intensity, 0.0f, maxIntensity, defaults.selected.intensity);
    values.selected.offsetX = clamp(values.selected.offsetX, -maxOffset, maxOffset, defaults.selected.offsetX);
    values.selected.offsetY = clamp(values.selected.offsetY, -maxOffset, maxOffset, defaults.selected.offsetY);
    values.selected.offsetZ = clamp(values.selected.offsetZ, -maxOffset, maxOffset, defaults.selected.offsetZ);
    values.selected.duration = clamp(values.selected.duration, 0.0f, 3.0f, defaults.selected.duration);
    values.selected.temperature = clamp(values.selected.temperature, 2000.0f, 10000.0f, defaults.selected.temperature);
    return values;
}

Settings::Values Settings::Get() {
    std::scoped_lock lock(settingsMutex);
    return current;
}

Settings::Values Settings::GetActive() {
    std::scoped_lock lock(settingsMutex);
    return preview.value_or(current);
}

void Settings::Preview(const Values& values) {
    std::scoped_lock lock(settingsMutex);
    preview = Normalize(values);
}

void Settings::ClearPreview() {
    std::scoped_lock lock(settingsMutex);
    preview.reset();
}

void Settings::Load() {
    std::scoped_lock lock(settingsMutex);
    Values values;
    char language[32]{};
    GetPrivateProfileStringA("General", "Language", "auto", language, sizeof(language), path);
    values.language = Localization::NormalizeCode(language);
    values.enabled = GetPrivateProfileIntA("General", "Enabled", 0, path) != 0;
    values.enablePlayerOnDialogue = GetPrivateProfileIntA("General", "EnablePlayerOnDialogue", 1, path) != 0;
    values.disablePlayerAfterDialogue = GetPrivateProfileIntA("General", "DisablePlayerAfterDialogue", 0, path) != 0;
    values.followHeadRotation = GetPrivateProfileIntA("General", "FollowHeadRotation", 0, path) != 0;
    values.dialogue.followHeadRotation = GetPrivateProfileIntA("General", "DialogueFollowHeadRotation", 0, path) != 0;
    values.debugLogging = GetPrivateProfileIntA("General", "DebugLogging", 0, path) != 0;
    values.csMode = GetPrivateProfileIntA("General", "CSMode", 0, path);
    values.csGlobalLinear = GetPrivateProfileIntA("General", "CSGlobalLinearLighting", 0, path) != 0;
    values.csInverseSquare = GetPrivateProfileIntA("General", "CSInverseSquare", 0, path) != 0;
    values.csLinear = GetPrivateProfileIntA("General", "CSLinear", 0, path) != 0;
    values.radius = ReadFloat("Radius", values.radius);
    values.manualRange = GetPrivateProfileIntA("General", "ManualInverseRange", 0, path) != 0;
    values.dialogue.manualRange = GetPrivateProfileIntA("General", "DialogueManualInverseRange", 0, path) != 0;
    values.inverseRadius = ReadFloat("InverseRadius", values.inverseRadius);
    values.dialogue.inverseRadius = ReadFloat("DialogueInverseRadius", values.dialogue.inverseRadius);
    values.intensity = ReadFloat("Intensity", values.intensity);
    values.offsetX = ReadFloat("OffsetX", values.offsetX);
    values.offsetY = ReadFloat("OffsetY", values.offsetY);
    values.offsetZ = ReadFloat("OffsetZ", values.offsetZ);
    values.dialogue.radius = ReadFloat("DialogueRadius", values.dialogue.radius);
    values.dialogue.intensity = ReadFloat("DialogueIntensity", values.dialogue.intensity);
    values.dialogue.offsetX = ReadFloat("DialogueOffsetX", values.dialogue.offsetX);
    values.dialogue.offsetY = ReadFloat("DialogueOffsetY", values.dialogue.offsetY);
    values.dialogue.offsetZ = ReadFloat("DialogueOffsetZ", values.dialogue.offsetZ);
    values.dialogue.duration = ReadFloat("DialogueDuration", values.dialogue.duration);
    values.dialogue.enabled = GetPrivateProfileIntA("General", "DialogueEnabled", 1, path) != 0;
    values.dialogue.transition = GetPrivateProfileIntA("General", "DialogueTransition", 1, path) != 0;
    values.dialogue.csInverseSquare = GetPrivateProfileIntA("General", "DialogueCSInverseSquare", 1, path) != 0;
    values.dialogue.csLinear = GetPrivateProfileIntA("General", "DialogueCSLinear", 1, path) != 0;
    values.hotkey = GetPrivateProfileIntA("General", "PlayerHotkey", 38, path);
    values.gamepadKey = GetPrivateProfileIntA("General", "PlayerGamepadKey", 0, path);
    values.gamepadModifier = GetPrivateProfileIntA("General", "PlayerGamepadModifier", 0, path);
    values.hotkeyModifier = GetPrivateProfileIntA("General", "PlayerHotkeyModifier", 0, path);
    values.hideWhileSneaking = GetPrivateProfileIntA("General", "HidePlayerWhileSneaking", 1, path) != 0;
    values.temperature = ReadFloat("Temperature", values.temperature);
    values.dialogue.temperature = ReadFloat("DialogueTemperature", values.dialogue.temperature);
    values.selected.followHeadRotation = GetPrivateProfileIntA("General", "SelectedFollowHeadRotation", 0, path) != 0;
    values.selected.manualRange = GetPrivateProfileIntA("General", "SelectedManualInverseRange", 0, path) != 0;
    values.selected.inverseRadius = ReadFloat("SelectedInverseRadius", values.selected.inverseRadius);
    values.selected.radius = ReadFloat("SelectedRadius", values.selected.radius);
    values.selected.intensity = ReadFloat("SelectedIntensity", values.selected.intensity);
    values.selected.offsetX = ReadFloat("SelectedOffsetX", values.selected.offsetX);
    values.selected.offsetY = ReadFloat("SelectedOffsetY", values.selected.offsetY);
    values.selected.offsetZ = ReadFloat("SelectedOffsetZ", values.selected.offsetZ);
    values.selected.duration = ReadFloat("SelectedDuration", values.selected.duration);
    values.selected.enabled = GetPrivateProfileIntA("General", "SelectedEnabled", 1, path) != 0;
    values.selected.transition = GetPrivateProfileIntA("General", "SelectedTransition", 1, path) != 0;
    values.selected.csInverseSquare = GetPrivateProfileIntA("General", "SelectedCSInverseSquare", 1, path) != 0;
    values.selected.csLinear = GetPrivateProfileIntA("General", "SelectedCSLinear", 1, path) != 0;
    values.selected.temperature = ReadFloat("SelectedTemperature", values.selected.temperature);
    Publish(Normalize(values));
    SKSE::log::info("Settings loaded: enabled={}, radius={}, intensity={}", current.enabled, current.radius, current.intensity);
}

bool Settings::Save(const Values& requested) {
    std::scoped_lock lock(settingsMutex);
    const auto values = Normalize(requested);
    // One section write avoids publishing a partially saved set of owned keys.
    std::string section;
    const auto append = [&](const char* key, const std::string& value) {
        section += key;
        section += '=';
        section += value;
        section += '\0';
    };
    append("Enabled", values.enabled ? "1" : "0");
    append("EnablePlayerOnDialogue", values.enablePlayerOnDialogue ? "1" : "0");
    append("DisablePlayerAfterDialogue", values.disablePlayerAfterDialogue ? "1" : "0");
    append("FollowHeadRotation", values.followHeadRotation ? "1" : "0");
    append("DialogueFollowHeadRotation", values.dialogue.followHeadRotation ? "1" : "0");
    append("Language", values.language);
    append("DebugLogging", values.debugLogging ? "1" : "0");
    append("CSMode", std::format("{}", values.csMode));
    append("CSGlobalLinearLighting", values.csGlobalLinear ? "1" : "0");
    append("CSInverseSquare", values.csInverseSquare ? "1" : "0");
    append("CSLinear", values.csLinear ? "1" : "0");
    append("Radius", std::format("{}", values.radius));
    append("ManualInverseRange", values.manualRange ? "1" : "0");
    append("DialogueManualInverseRange", values.dialogue.manualRange ? "1" : "0");
    append("InverseRadius", std::format("{}", values.inverseRadius));
    append("DialogueInverseRadius", std::format("{}", values.dialogue.inverseRadius));
    append("Intensity", std::format("{}", values.intensity));
    append("OffsetX", std::format("{}", values.offsetX));
    append("OffsetY", std::format("{}", values.offsetY));
    append("OffsetZ", std::format("{}", values.offsetZ));
    append("DialogueRadius", std::format("{}", values.dialogue.radius));
    append("DialogueIntensity", std::format("{}", values.dialogue.intensity));
    append("DialogueOffsetX", std::format("{}", values.dialogue.offsetX));
    append("DialogueOffsetY", std::format("{}", values.dialogue.offsetY));
    append("DialogueOffsetZ", std::format("{}", values.dialogue.offsetZ));
    append("DialogueDuration", std::format("{}", values.dialogue.duration));
    append("DialogueEnabled", values.dialogue.enabled ? "1" : "0");
    append("DialogueTransition", values.dialogue.transition ? "1" : "0");
    append("DialogueCSInverseSquare", values.dialogue.csInverseSquare ? "1" : "0");
    append("DialogueCSLinear", values.dialogue.csLinear ? "1" : "0");
    append("PlayerHotkey", std::format("{}", values.hotkey));
    append("PlayerGamepadKey", std::format("{}", values.gamepadKey));
    append("PlayerGamepadModifier", std::format("{}", values.gamepadModifier));
    append("PlayerHotkeyModifier", std::format("{}", values.hotkeyModifier));
    append("HidePlayerWhileSneaking", values.hideWhileSneaking ? "1" : "0");
    append("Temperature", std::format("{}", values.temperature));
    append("DialogueTemperature", std::format("{}", values.dialogue.temperature));
    append("SelectedFollowHeadRotation", values.selected.followHeadRotation ? "1" : "0");
    append("SelectedManualInverseRange", values.selected.manualRange ? "1" : "0");
    append("SelectedInverseRadius", std::format("{}", values.selected.inverseRadius));
    append("SelectedRadius", std::format("{}", values.selected.radius));
    append("SelectedIntensity", std::format("{}", values.selected.intensity));
    append("SelectedOffsetX", std::format("{}", values.selected.offsetX));
    append("SelectedOffsetY", std::format("{}", values.selected.offsetY));
    append("SelectedOffsetZ", std::format("{}", values.selected.offsetZ));
    append("SelectedDuration", std::format("{}", values.selected.duration));
    append("SelectedEnabled", values.selected.enabled ? "1" : "0");
    append("SelectedTransition", values.selected.transition ? "1" : "0");
    append("SelectedCSInverseSquare", values.selected.csInverseSquare ? "1" : "0");
    append("SelectedCSLinear", values.selected.csLinear ? "1" : "0");
    append("SelectedTemperature", std::format("{}", values.selected.temperature));
    if (!WritePrivateProfileSectionA("General", section.c_str(), path)) {
        preview.reset();
        SKSE::log::error("Failed to save FaceLighting settings, Windows error {}", GetLastError());
        return false;
    }
    Publish(values);
    SKSE::log::info("Settings saved: enabled={}, radius={}, intensity={}, offset=({}, {}, {})",
        values.enabled, values.radius, values.intensity, values.offsetX, values.offsetY, values.offsetZ);
    return true;
}

bool Settings::SetPlayerEnabled(bool enabled) {
    std::scoped_lock lock(settingsMutex);
    if (current.enabled != enabled && !WritePrivateProfileStringA("General", "Enabled", enabled ? "1" : "0", path)) {
        SKSE::log::error("Failed to save dialogue player light state, Windows error {}", GetLastError());
        return false;
    }
    current.enabled = enabled;
    if (preview) preview->enabled = enabled;
    return true;
}




