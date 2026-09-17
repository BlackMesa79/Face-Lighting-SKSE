#include <SKSE/SKSE.h>
#include <RE/Skyrim.h>
#include "Hotkeys.h"
#include "Settings.h"
#include "ConfigMenu.h"
#include "FaceLight.h"
#include <Windows.h>

namespace {
    bool CanToggle() {
        DWORD foregroundProcess = 0;
        GetWindowThreadProcessId(GetForegroundWindow(), &foregroundProcess);
        if (foregroundProcess != GetCurrentProcessId()) return false;
        const auto ui = RE::UI::GetSingleton();
        const auto player = RE::PlayerCharacter::GetSingleton();
        const auto controls = RE::ControlMap::GetSingleton();
        return ui && player && player->Get3D(false) && !ConfigMenu::IsOpen() &&
            !ui->GameIsPaused() && !ui->IsMenuOpen(RE::Console::MENU_NAME) &&
            !ui->IsMenuOpen(RE::LoadingMenu::MENU_NAME) && !ui->IsMenuOpen(RE::MainMenu::MENU_NAME) &&
            controls && controls->GetRuntimeData().textEntryCount == 0;
    }
    class Events final : public RE::BSTEventSink<RE::InputEvent*> {
        RE::BSEventNotifyControl ProcessEvent(RE::InputEvent* const* events,
            RE::BSTEventSource<RE::InputEvent*>*) override {
            if (!events || !CanToggle()) return RE::BSEventNotifyControl::kContinue;
            const auto settings = Settings::Get();
            const int key = settings.hotkey;
            if (key == 0 && settings.gamepadKey == 0) return RE::BSEventNotifyControl::kContinue;
            const auto input = RE::BSInputDeviceManager::GetSingleton();
            const auto keyboard = input ? input->GetKeyboard() : nullptr;

            unsigned held = 0;
            // Query the engine's current device state; no cached modifier can stick after a menu or focus change.
            for (int code : {42, 54, 29, 157, 56, 184})
                if (keyboard && (keyboard->GetRuntimeData().curState[code] & 0x80) != 0) held |= Hotkeys::ModifierMask(code);
            bool padModifierHeld = false;
            if (settings.gamepadModifier != 0 && input && input->IsGamepadConnected()) {
                if (const auto pad = input->GetGamepad()) {
                    const auto& buttons = static_cast<RE::BSInputDevice*>(pad)->GetRuntimeData().deviceButtons;
                    const auto id = SKSE::InputMap::GamepadKeycodeToMask(settings.gamepadModifier);
                    const auto found = buttons.find(id);
                    padModifierHeld = found != buttons.end() && found->second && found->second->heldDownSecs > 0;
                }
                // Include this batch's changes, including press/release at the same polling tick.
                for (auto event = *events; event; event = event->next) {
                    const auto button = event->AsButtonEvent();
                    if (button && button->GetDevice() == RE::INPUT_DEVICE::kGamepad &&
                        SKSE::InputMap::GamepadMaskToKeycode(button->GetIDCode()) == static_cast<unsigned>(settings.gamepadModifier))
                        padModifierHeld = button->IsPressed();
                }
            }
            for (auto event = *events; event; event = event->next) {
                const auto button = event->AsButtonEvent();
                if (!button || !button->IsDown()) continue;
                const bool keyboardMatch = key != 0 && keyboard && button->GetDevice() == RE::INPUT_DEVICE::kKeyboard &&
                    button->GetIDCode() == static_cast<unsigned>(key) && Hotkeys::MatchesModifier(settings.hotkeyModifier, held);
                const bool padMatch = input && input->IsGamepadConnected() && button->GetDevice() == RE::INPUT_DEVICE::kGamepad &&
                    Hotkeys::PadMatch(settings.gamepadKey, settings.gamepadModifier,
                        SKSE::InputMap::GamepadMaskToKeycode(button->GetIDCode()), padModifierHeld);
                if (keyboardMatch || padMatch) {
                    if (const auto tasks = SKSE::GetTaskInterface()) tasks->AddTask([] {
                        if (!CanToggle()) return;
                        auto values = Settings::Get();
                        values.enabled = !values.enabled;
                        if (Settings::Save(values)) FaceLight::RequestUpdate();
                    });
                    break;
                }
            }
            return RE::BSEventNotifyControl::kContinue;
        }
    };
}
void Hotkeys::Install() {
    static auto* sink = new Events;
    if (const auto input = RE::BSInputDeviceManager::GetSingleton()) input->AddEventSink(sink);
}

