#include <SKSE/SKSE.h>
#include "Followers.h"
#include "SelectedNPCs.h"
#include "ActorRuntime.h"
#include "FollowerRoster.h"
#include "FollowerPreferences.h"
#include "Notifications.h"
#include <algorithm>
#include <chrono>
#include <mutex>
#include <unordered_set>

namespace {
    std::vector<RE::ActorHandle> members;
    std::vector<Followers::Row> rows;
    std::mutex viewMutex;
    FollowerPreferences preferences;
    std::unordered_set<RE::FormID> announced;
    constexpr std::uint32_t preferencesRecord = 0x464C5052;
    std::chrono::steady_clock::time_point nextScan{};
    bool Member(RE::Actor* actor) {
        return actor && actor->IsPlayerTeammate() && ActorRuntime::Eligible(actor);
    }
    bool Loaded(RE::Actor* actor) {
        const auto cell = actor->GetParentCell();
        return actor->Get3D(false) && cell && cell->IsAttached();
    }
}
void Followers::Reset() {
    std::scoped_lock lock(viewMutex);
    members.clear();
    nextScan = {};
    rows.clear();
}
std::vector<RE::ActorHandle> Followers::Update() {
    std::scoped_lock lock(viewMutex);
    const auto now = std::chrono::steady_clock::now();
    if (now >= nextScan) {
        nextScan = now + std::chrono::milliseconds(500);
        // Retain waiting/unloaded teammates, but release dismissed/dead/invalid ones.
        std::vector<RE::ActorHandle> candidates;
        if (const auto processes = RE::ProcessLists::GetSingleton()) {
            // Inspect existing process handles only; never load cells or enumerate all forms.
            for (const auto list : processes->allProcesses) if (list) {
                candidates.insert(candidates.end(), list->begin(), list->end());
            }
        }
        FollowerRoster::Reconcile(members, std::span<const RE::ActorHandle>(candidates),
            [](const auto& handle) { return Member(handle.get().get()); });
        for (const auto& handle : candidates) {
            const auto actor = handle.get();
            if (actor && !Member(actor.get())) announced.erase(actor->GetFormID());
        }
        std::string added;
        std::size_t addedCount = 0;
        std::vector<Row> snapshot;
        for (const auto& handle : members) if (const auto actor = handle.get()) {
            const auto id = actor->GetFormID();
            snapshot.push_back({id, actor->GetName(), Loaded(actor.get()), preferences.Enabled(id)});
            if (announced.insert(id).second) {
                if (++addedCount <= 3) { if (!added.empty()) added += ", "; added += actor->GetName(); }
            }
        }
        if (addedCount > 3) added += std::format(" (+{})", addedCount - 3);
        if (addedCount) Notifications::Show(Localization::noticeFollowersAdded, added);
        std::sort(snapshot.begin(), snapshot.end(), [](const auto& a, const auto& b) { return a.id < b.id; });
        rows = std::move(snapshot);
    }
    std::vector<RE::ActorHandle> result;
    for (const auto& handle : members) {
        const auto actor = handle.get();
        if (Member(actor.get()) && preferences.Enabled(actor->GetFormID()) && Loaded(actor.get())) result.push_back(handle);
    }
    // Stable selection for large parties: nearest loaded teammates receive the budget.
    if (const auto player = RE::PlayerCharacter::GetSingleton(); player && result.size() > lightLimit) {
        std::stable_sort(result.begin(), result.end(), [player](const auto& a, const auto& b) {
            return a.get()->GetPosition().GetSquaredDistance(player->GetPosition()) <
                b.get()->GetPosition().GetSquaredDistance(player->GetPosition());
        });
    }
    if (result.size() > lightLimit) result.resize(lightLimit);
    return result;
}
std::vector<Followers::Row> Followers::Snapshot() {
    std::scoped_lock lock(viewMutex);
    return rows;
}
void Followers::SetEnabled(RE::FormID id, bool enabled) {
    // The shared task locks selected preferences before follower preferences.
    SelectedNPCs::SetFollowerEnabled(id, enabled);
}
void Followers::RevertPreferences() {
    std::scoped_lock lock(viewMutex);
    preferences.disabled.clear(); announced.clear();
    members.clear(); rows.clear(); nextScan = {};
}
bool Followers::PersonalEnabled(RE::FormID id) {
    std::scoped_lock lock(viewMutex);
    return preferences.Enabled(id);
}
bool Followers::SetPersonalNow(RE::FormID id, bool enabled) {
    return SetPersonalWithCommit(id, enabled, {});
}
bool Followers::SetPersonalWithCommit(RE::FormID id, bool enabled, const std::function<bool()>& beforeCommit) {
    std::scoped_lock lock(viewMutex);
    if (!preferences.SetWithCommit(id, enabled, [&] { return !beforeCommit || beforeCommit(); })) return false;
    for (auto& row : rows) if (row.id == id) row.enabled = enabled;
    return true;
}
void Followers::Save(SKSE::SerializationInterface* api) {
    std::scoped_lock lock(viewMutex);
    if (!api->WriteRecord(preferencesRecord, 1, preferences.disabled.data(), static_cast<std::uint32_t>(preferences.disabled.size() * sizeof(RE::FormID))))
        SKSE::log::error("Could not save follower light preferences");
}
bool Followers::LoadRecord(SKSE::SerializationInterface* api, std::uint32_t type, std::uint32_t version, std::uint32_t length) {
    if (type != preferencesRecord) return false;
    if (version != 1 || !FollowerPreferences::ValidLength(length)) return true;
    std::vector<std::uint32_t> ids(length / sizeof(std::uint32_t));
    if (api->ReadRecordData(ids.data(), length) != length) { SKSE::log::warn("Truncated follower preferences"); return true; }
    std::scoped_lock lock(viewMutex);
    preferences.Restore(ids, [api](auto id, auto& remapped) { return api->ResolveFormID(id, remapped); });
    return true;
}
