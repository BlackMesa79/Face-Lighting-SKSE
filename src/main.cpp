#include <SKSE/SKSE.h>
#include "ConfigMenu.h"
#include "Settings.h"
#include "FaceLight.h"
#include "Hotkeys.h"
#include "Localization.h"
#include "SelectedNPCs.h"

namespace {
    void OnMessage(SKSE::MessagingInterface::Message* message) {
        if (!message) return;
        switch (message->type) {
        case SKSE::MessagingInterface::kPostLoad:
            ConfigMenu::Register();
            break;
        case SKSE::MessagingInterface::kDataLoaded:
            FaceLight::Install();
            Hotkeys::Install();
            break;
        case SKSE::MessagingInterface::kPreLoadGame:
            FaceLight::SetGameActive(false);
            break;
        case SKSE::MessagingInterface::kNewGame:
            FaceLight::SetGameActive(true);
            break;
        case SKSE::MessagingInterface::kPostLoadGame:
            FaceLight::SetGameActive(message->data != nullptr);
            break;
        default:
            break;
        }
    }
}

SKSEPluginLoad(const SKSE::LoadInterface* skse) {
    SKSE::Init(skse);
    if (!SelectedNPCs::Install()) return false;
    Localization::Load();
    Settings::Load();
    const auto messaging = SKSE::GetMessagingInterface();
    if (!messaging || !SKSE::GetTaskInterface() || !messaging->RegisterListener(OnMessage)) {
        SKSE::log::error("SKSE messaging unavailable or listener registration failed");
        return false;
    }
    SKSE::log::info("FaceLighting loaded");
    return true;
}

