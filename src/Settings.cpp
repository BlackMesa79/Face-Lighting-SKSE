#include <SKSE/SKSE.h>
#include <Windows.h>
#include <algorithm>
#include <charconv>
#include <cmath>
#include <mutex>
#include <optional>
#include <filesystem>
#include "Settings.h"
#include "Hotkeys.h"
#include "Localization.h"

namespace {
    std::mutex settingsMutex;
    Settings::Values current;
    std::optional<Settings::Values> preview;
    std::optional<std::filesystem::path> configPath;
    std::wstring Wide(std::string_view ascii) { return {ascii.begin(), ascii.end()}; }
    std::string UTF8(std::wstring_view text) {
        if (text.empty()) return {};
        const int size = WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
        std::string result(size, '\0');
        WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), result.data(), size, nullptr, nullptr);
        return result;
    }
    const std::filesystem::path& ConfigPath() {
        if (!configPath) {
            // Resolve against SkyrimSE.exe, never a mutable process working directory
            // or this DLL's physical mod-manager installation directory.
            std::wstring executable(32768, L'\0');
            const auto size = GetModuleFileNameW(nullptr, executable.data(), static_cast<DWORD>(executable.size()));
            if (!size || size >= executable.size()) {
                configPath = std::filesystem::path{};
                SKSE::log::error("Could not resolve executable path for settings, Windows error {}", GetLastError());
            } else {
                executable.resize(size);
                configPath = std::filesystem::path(executable).parent_path() / L"Data/SKSE/Plugins/FaceLighting.ini";
            }
        }
        return *configPath;
    }
    std::string ReadText(const char* key, const char* fallback = "") {
        if (ConfigPath().empty()) return fallback;
        wchar_t buffer[128]{};
        GetPrivateProfileStringW(L"General", Wide(key).c_str(), Wide(fallback).c_str(), buffer, 128, ConfigPath().c_str());
        return UTF8(buffer);
    }
    UINT ReadProfileInt(const char* key, INT fallback) {
        return ConfigPath().empty() ? fallback :
            GetPrivateProfileIntW(L"General", Wide(key).c_str(), fallback, ConfigPath().c_str());
    }
    bool EnsureConfigDirectory() {
        const auto& path = ConfigPath();
        if (path.empty()) { SetLastError(ERROR_PATH_NOT_FOUND); return false; }
        std::error_code error;
        std::filesystem::create_directories(path.parent_path(), error);
        if (error) { SetLastError(static_cast<DWORD>(error.value())); return false; }
        return true;
    }
    void SaveFailure(const char* operation, DWORD error) {
        std::error_code cwdError;
        const auto cwd = std::filesystem::current_path(cwdError);
        SKSE::log::error("Failed to save {}: path='{}', cwd='{}', Windows error {}",
            operation, UTF8(ConfigPath().wstring()), cwdError ? "unavailable" : UTF8(cwd.wstring()), error);
    }

    void Publish(const Settings::Values& values) {
        current = values;
        preview.reset();
        spdlog::set_level(values.debugLogging ? spdlog::level::debug : spdlog::level::info);
    }

    int ReadInt(const char* key, int fallback) {
        const auto buffer = ReadText(key);
        std::string_view text(buffer);
        if (text.empty()) return fallback;
        int result{};
        const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), result);
        if (error != std::errc{} || end != text.data() + text.size()) {
            SKSE::log::warn("Invalid {} value '{}'; using {}", key, text, fallback);
            return fallback;
        }
        return result;
    }

    float ReadFloat(const char* key, float fallback) {
        const auto buffer = ReadText(key);
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
    values.npcLightLimit = std::clamp(values.npcLightLimit, minNpcLightLimit, maxNpcLightLimit);
    if (values.selectedHotkey < 0 || values.selectedHotkey > 255) values.selectedHotkey = 38;
    if (values.selectedHotkeyModifier < 0 || values.selectedHotkeyModifier > 3) values.selectedHotkeyModifier = 1;
    if (values.selectedHotkeyModifier && Hotkeys::ModifierMask(values.selectedHotkey) == (1u << (values.selectedHotkeyModifier - 1)))
        values.selectedHotkey = 38;
    if (!Hotkeys::ValidPadCode(values.gamepadKey)) values.gamepadKey = 0;
    if (!Hotkeys::ValidPadCode(values.gamepadModifier)) values.gamepadModifier = 0;
    if (values.gamepadKey != 0 && values.gamepadKey == values.gamepadModifier) values.gamepadModifier = 0;
    values.csMode = values.csMode == 2 ? 2 : 1; // Preserve legacy off; old auto/manual become on.
    if (values.hotkey < 0 || values.hotkey > 255) values.hotkey = 38;
    if (values.hotkeyModifier < 0 || values.hotkeyModifier > 3) values.hotkeyModifier = 0;
    if (values.hotkeyModifier && Hotkeys::ModifierMask(values.hotkey) == (1u << (values.hotkeyModifier - 1)))
        values.hotkey = 38;
    values.language = Localization::NormalizeCode(values.language);
    const auto clamp = [](float value, float low, float high, float fallback) {
        return std::isfinite(value) ? std::clamp(value, low, high) : fallback;
    };
    values.radius = clamp(values.radius, minRadius, maxRadius, defaults.radius);
    values.playerDuration = clamp(values.playerDuration, 0.0f, 3.0f, defaults.playerDuration);
    if (values.ambientPollMode < 0 || values.ambientPollMode > 2) values.ambientPollMode = 0;
    if (values.ambientMode < 0 || values.ambientMode > 3) values.ambientMode = 0;
    if (values.dialogueAmbientMode != 0 && values.dialogueAmbientMode != 2 && values.dialogueAmbientMode != 3)
        values.dialogueAmbientMode = 0;
    values.dialogueAmbientCompensation = clamp(values.dialogueAmbientCompensation, 0.0f, 1000.0f, defaults.dialogueAmbientCompensation);
    values.dialogueAmbientOnThreshold = clamp(values.dialogueAmbientOnThreshold, 0.0f, 999.0f, defaults.dialogueAmbientOnThreshold);
    values.dialogueAmbientOffThreshold = clamp(values.dialogueAmbientOffThreshold, values.dialogueAmbientOnThreshold + 1.0f, 1000.0f,
        std::max(defaults.dialogueAmbientOffThreshold, values.dialogueAmbientOnThreshold + 1.0f));
    values.dialogueAmbientDelay = clamp(values.dialogueAmbientDelay, 0.5f, 10.0f, defaults.dialogueAmbientDelay);
    values.ambientCompensation = clamp(values.ambientCompensation, 0.0f, 1000.0f, defaults.ambientCompensation);
    values.ambientOnThreshold = clamp(values.ambientOnThreshold, 0.0f, 999.0f, defaults.ambientOnThreshold);
    values.ambientOffThreshold = clamp(values.ambientOffThreshold, values.ambientOnThreshold + 1.0f, 1000.0f,
        std::max(defaults.ambientOffThreshold, values.ambientOnThreshold + 1.0f));
    values.ambientDelay = clamp(values.ambientDelay, 0.5f, 10.0f, defaults.ambientDelay);
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
    values.follower.inverseRadius = clamp(values.follower.inverseRadius, minRadius, maxRadius, defaults.follower.inverseRadius);
    values.follower.radius = clamp(values.follower.radius, minRadius, maxRadius, defaults.follower.radius);
    values.follower.intensity = clamp(values.follower.intensity, 0.0f, maxIntensity, defaults.follower.intensity);
    values.follower.offsetX = clamp(values.follower.offsetX, -maxOffset, maxOffset, defaults.follower.offsetX);
    values.follower.offsetY = clamp(values.follower.offsetY, -maxOffset, maxOffset, defaults.follower.offsetY);
    values.follower.offsetZ = clamp(values.follower.offsetZ, -maxOffset, maxOffset, defaults.follower.offsetZ);
    values.follower.duration = clamp(values.follower.duration, 0.0f, 3.0f, defaults.follower.duration);
    values.follower.temperature = clamp(values.follower.temperature, 2000.0f, 10000.0f, defaults.follower.temperature);
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

bool Settings::HasPreview() {
    std::scoped_lock lock(settingsMutex);
    return preview.has_value();
}

void Settings::Load() {
    std::scoped_lock lock(settingsMutex);
    Values values;
    SKSE::log::info("Settings path: {}", UTF8(ConfigPath().wstring()));
    values.language = Localization::NormalizeCode(ReadText("Language", "auto"));
    values.enabled = ReadProfileInt("Enabled", 0) != 0;
    values.enablePlayerOnDialogue = ReadProfileInt("EnablePlayerOnDialogue", 1) != 0;
    values.disablePlayerAfterDialogue = ReadProfileInt("DisablePlayerAfterDialogue", 0) != 0;
    values.followHeadRotation = ReadProfileInt("FollowHeadRotation", 0) != 0;
    values.dialogue.followHeadRotation = ReadProfileInt("DialogueFollowHeadRotation", 0) != 0;
    values.debugLogging = ReadProfileInt("DebugLogging", 0) != 0;
    values.csMode = ReadProfileInt("CSMode", 1);
    values.csGlobalLinear = ReadProfileInt("CSGlobalLinearLighting", 0) != 0;
    values.csInverseSquare = ReadProfileInt("CSInverseSquare", 0) != 0;
    values.csLinear = ReadProfileInt("CSLinear", 0) != 0;
    values.radius = ReadFloat("Radius", values.radius);
    values.manualRange = ReadProfileInt("ManualInverseRange", 0) != 0;
    values.dialogue.manualRange = ReadProfileInt("DialogueManualInverseRange", 0) != 0;
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
    values.dialogue.enabled = ReadProfileInt("DialogueEnabled", 1) != 0;
    values.dialogue.transition = ReadProfileInt("DialogueTransition", 1) != 0;
    values.dialogue.csInverseSquare = ReadProfileInt("DialogueCSInverseSquare", 1) != 0;
    values.dialogue.csLinear = ReadProfileInt("DialogueCSLinear", 1) != 0;
    values.hotkey = ReadProfileInt("PlayerHotkey", 38);
    values.selectedHotkey = ReadProfileInt("SelectedHotkey", 38);
    values.selectedHotkeyModifier = ReadProfileInt("SelectedHotkeyModifier", 1);
    values.gamepadKey = ReadProfileInt("PlayerGamepadKey", 0);
    values.gamepadModifier = ReadProfileInt("PlayerGamepadModifier", 0);
    values.hotkeyModifier = ReadProfileInt("PlayerHotkeyModifier", 0);
    values.hideWhileSneaking = ReadProfileInt("HidePlayerWhileSneaking", 1) != 0;
    values.temperature = ReadFloat("Temperature", values.temperature);
    values.dialogue.temperature = ReadFloat("DialogueTemperature", values.dialogue.temperature);
    values.selected.followHeadRotation = ReadProfileInt("SelectedFollowHeadRotation", 0) != 0;
    values.selected.manualRange = ReadProfileInt("SelectedManualInverseRange", 0) != 0;
    values.selected.inverseRadius = ReadFloat("SelectedInverseRadius", values.selected.inverseRadius);
    values.selected.radius = ReadFloat("SelectedRadius", values.selected.radius);
    values.selected.intensity = ReadFloat("SelectedIntensity", values.selected.intensity);
    values.selected.offsetX = ReadFloat("SelectedOffsetX", values.selected.offsetX);
    values.selected.offsetY = ReadFloat("SelectedOffsetY", values.selected.offsetY);
    values.selected.offsetZ = ReadFloat("SelectedOffsetZ", values.selected.offsetZ);
    values.selected.duration = ReadFloat("SelectedDuration", values.selected.duration);
    values.selected.enabled = ReadProfileInt("SelectedEnabled", 1) != 0;
    values.selected.transition = ReadProfileInt("SelectedTransition", 1) != 0;
    values.selected.csInverseSquare = ReadProfileInt("SelectedCSInverseSquare", 1) != 0;
    values.selected.csLinear = ReadProfileInt("SelectedCSLinear", 1) != 0;
    values.selected.temperature = ReadFloat("SelectedTemperature", values.selected.temperature);
    values.follower.followHeadRotation = ReadProfileInt("FollowerFollowHeadRotation", 0) != 0;
    values.follower.manualRange = ReadProfileInt("FollowerManualInverseRange", 0) != 0;
    values.follower.inverseRadius = ReadFloat("FollowerInverseRadius", values.follower.inverseRadius);
    values.follower.radius = ReadFloat("FollowerRadius", values.follower.radius);
    values.follower.intensity = ReadFloat("FollowerIntensity", values.follower.intensity);
    values.follower.offsetX = ReadFloat("FollowerOffsetX", values.follower.offsetX);
    values.follower.offsetY = ReadFloat("FollowerOffsetY", values.follower.offsetY);
    values.follower.offsetZ = ReadFloat("FollowerOffsetZ", values.follower.offsetZ);
    values.follower.duration = ReadFloat("FollowerDuration", values.follower.duration);
    values.follower.enabled = ReadProfileInt("FollowerEnabled", 1) != 0;
    values.follower.transition = ReadProfileInt("FollowerTransition", 1) != 0;
    values.follower.csInverseSquare = ReadProfileInt("FollowerCSInverseSquare", 1) != 0;
    values.follower.csLinear = ReadProfileInt("FollowerCSLinear", 1) != 0;
    values.follower.temperature = ReadFloat("FollowerTemperature", values.follower.temperature);
    values.firstPersonLight = ReadProfileInt("FirstPersonLight", 0) != 0;
    values.lightDiagnostics = ReadProfileInt("LightDiagnostics", 0) != 0;
    values.exclusionDiagnostics = ReadProfileInt("ExclusionDiagnostics", 0) != 0;
    values.playerTransition = ReadProfileInt("PlayerTransition", 1) != 0;
    values.playerDuration = ReadFloat("PlayerDuration", values.playerDuration);
    values.ambientPollMode = ReadInt("AmbientPollMode", values.ambientPollMode);
    values.ambientMode = ReadProfileInt("AmbientMode", 0);
    values.ambientCompensation = ReadFloat("AmbientCompensation", values.ambientCompensation);
    values.ambientOnThreshold = ReadFloat("AmbientOnThreshold", values.ambientOnThreshold);
    values.ambientOffThreshold = ReadFloat("AmbientOffThreshold", values.ambientOffThreshold);
    values.ambientDelay = ReadFloat("AmbientDelay", values.ambientDelay);
    values.dialogueAmbientMode = ReadInt("DialogueAmbientMode", values.dialogueAmbientMode);
    values.dialogueAmbientCompensation = ReadFloat("DialogueAmbientCompensation", values.dialogueAmbientCompensation);
    values.dialogueAmbientOnThreshold = ReadFloat("DialogueAmbientOnThreshold", values.dialogueAmbientOnThreshold);
    values.dialogueAmbientOffThreshold = ReadFloat("DialogueAmbientOffThreshold", values.dialogueAmbientOffThreshold);
    values.dialogueAmbientDelay = ReadFloat("DialogueAmbientDelay", values.dialogueAmbientDelay);
    values.npcLightLimit = ReadInt("NPCLightLimit", values.npcLightLimit);
    values.rosterNotifications = ReadProfileInt("RosterNotifications", 1) != 0;
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
    append("SelectedHotkey", std::format("{}", values.selectedHotkey));
    append("SelectedHotkeyModifier", std::format("{}", values.selectedHotkeyModifier));
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
    append("FollowerFollowHeadRotation", values.follower.followHeadRotation ? "1" : "0");
    append("FollowerManualInverseRange", values.follower.manualRange ? "1" : "0");
    append("FollowerInverseRadius", std::format("{}", values.follower.inverseRadius));
    append("FollowerRadius", std::format("{}", values.follower.radius));
    append("FollowerIntensity", std::format("{}", values.follower.intensity));
    append("FollowerOffsetX", std::format("{}", values.follower.offsetX));
    append("FollowerOffsetY", std::format("{}", values.follower.offsetY));
    append("FollowerOffsetZ", std::format("{}", values.follower.offsetZ));
    append("FollowerDuration", std::format("{}", values.follower.duration));
    append("FollowerEnabled", values.follower.enabled ? "1" : "0");
    append("FollowerTransition", values.follower.transition ? "1" : "0");
    append("FollowerCSInverseSquare", values.follower.csInverseSquare ? "1" : "0");
    append("FollowerCSLinear", values.follower.csLinear ? "1" : "0");
    append("FollowerTemperature", std::format("{}", values.follower.temperature));
    append("FirstPersonLight", values.firstPersonLight ? "1" : "0");
    append("LightDiagnostics", values.lightDiagnostics ? "1" : "0");
    append("ExclusionDiagnostics", values.exclusionDiagnostics ? "1" : "0");
    append("PlayerTransition", values.playerTransition ? "1" : "0");
    append("PlayerDuration", std::to_string(values.playerDuration));
    append("AmbientPollMode", std::to_string(values.ambientPollMode));
    append("AmbientMode", std::to_string(values.ambientMode));
    append("AmbientCompensation", std::to_string(values.ambientCompensation));
    append("AmbientOnThreshold", std::to_string(values.ambientOnThreshold));
    append("AmbientOffThreshold", std::to_string(values.ambientOffThreshold));
    append("AmbientDelay", std::to_string(values.ambientDelay));
    append("DialogueAmbientMode", std::to_string(values.dialogueAmbientMode));
    append("DialogueAmbientCompensation", std::to_string(values.dialogueAmbientCompensation));
    append("DialogueAmbientOnThreshold", std::to_string(values.dialogueAmbientOnThreshold));
    append("DialogueAmbientOffThreshold", std::to_string(values.dialogueAmbientOffThreshold));
    append("DialogueAmbientDelay", std::to_string(values.dialogueAmbientDelay));
    append("NPCLightLimit", std::to_string(values.npcLightLimit));
    append("RosterNotifications", values.rosterNotifications ? "1" : "0");
    if (!EnsureConfigDirectory() || !WritePrivateProfileSectionW(L"General", Wide(section).c_str(), ConfigPath().c_str())) {
        const auto error = GetLastError();
        preview.reset();
        SaveFailure("FaceLighting settings", error);
        return false;
    }
    Publish(values);
    SKSE::log::info("Settings saved: enabled={}, radius={}, intensity={}, offset=({}, {}, {})",
        values.enabled, values.radius, values.intensity, values.offsetX, values.offsetY, values.offsetZ);
    return true;
}

bool Settings::SetPlayerEnabled(bool enabled) {
    std::scoped_lock lock(settingsMutex);
    if (current.enabled != enabled && (!EnsureConfigDirectory() ||
        !WritePrivateProfileStringW(L"General", L"Enabled", enabled ? L"1" : L"0", ConfigPath().c_str()))) {
        SaveFailure("dialogue player light state", GetLastError());
        return false;
    }
    current.enabled = enabled;
    if (preview) preview->enabled = enabled;
    return true;
}

#ifdef FACE_LIGHTING_SETTINGS_TESTING
void Settings::SetPathForTests(const std::filesystem::path& path) {
    std::scoped_lock lock(settingsMutex);
    configPath = std::filesystem::absolute(path);
}
#endif




