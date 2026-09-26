#include <SKSE/SKSE.h>
#include <RE/Skyrim.h>
#include <RE/S/SendHUDMessage.h>
#include "Notifications.h"
#include "Settings.h"
#include <mutex>
namespace { std::mutex mutex; std::string latest; }
void Notifications::Show(const Localization::Text& text, std::string_view detail) {
    const auto settings = Settings::Get();
    std::string message = text.Get(settings.language);
    if (!detail.empty()) message += ": " + std::string(detail);
    SKSE::log::info("[Roster] {}", message);
    { std::scoped_lock lock(mutex); latest = message; }
    if (settings.rosterNotifications) RE::SendHUDMessage::ShowHUDMessage(message.c_str());
}
std::string Notifications::Snapshot() { std::scoped_lock lock(mutex); return latest; }
