#include <SKSE/SKSE.h>
#include "SelectedNPCs.h"
#include "FaceLight.h"
#include "ConfigMenu.h"
#include <mutex>
#include <algorithm>
#include <atomic>

namespace {
    using Record = SelectedNPCRecord::Record;
    constexpr std::uint32_t recordType = 0x4E504353, pluginID = 0x464C424D;
    std::mutex mutex;
    std::vector<Record> records;
    SelectedNPCs::View view;
    RE::ActorHandle crosshair, console, lastCrosshair;
    std::atomic<std::uint64_t> epoch{0};
    bool active = false;
    bool Eligible(RE::Actor* actor) {
        return actor && !actor->IsPlayerRef() && !actor->IsDeleted() && !actor->IsDisabled() && !actor->IsDead();
    }
    void Reset(SKSE::SerializationInterface*) {
        std::scoped_lock lock(mutex);
        ++epoch;
        records.clear(); view = {}; crosshair = {}; console = {}; lastCrosshair = {}; active = false;
    }
    void Save(SKSE::SerializationInterface* api) {
        std::scoped_lock lock(mutex);
        if (!api->WriteRecord(recordType, 1, records.data(), static_cast<std::uint32_t>(records.size() * sizeof(Record))))
            SKSE::log::error("Could not save selected NPC list");
    }
    void Load(SKSE::SerializationInterface* api) {
        Reset(api);
        std::vector<Record> loaded;
        std::uint32_t type, version, length;
        while (api->GetNextRecordInfo(type, version, length)) {
            if (type != recordType || version != 1 || !SelectedNPCRecord::ValidLength(length)) continue;
            std::vector<Record> data(length / sizeof(Record));
            if (api->ReadRecordData(data.data(), length) != length) { SKSE::log::warn("Truncated selected NPC record"); continue; }
            SelectedNPCRecord::AppendResolved(loaded, data, [api](auto oldID, auto& newID) { return api->ResolveFormID(oldID, newID); });
        }
        std::scoped_lock lock(mutex);
        records = std::move(loaded);
    }
    template <class F> void Queue(F action) {
        const auto session = epoch.load();
        if (const auto tasks = SKSE::GetTaskInterface()) tasks->AddTask([session, action] {
            {
                std::scoped_lock lock(mutex);
                if (!active || session != epoch.load()) return;
                action();
            }
            FaceLight::RequestUpdate();
        });
    }
}
bool SelectedNPCs::Install() {
    const auto api = SKSE::GetSerializationInterface();
    if (!api) return false;
    api->SetUniqueID(pluginID);
    api->SetSaveCallback(Save); api->SetLoadCallback(Load); api->SetRevertCallback(Reset);
    return true;
}
void SelectedNPCs::SetActive(bool value) {
    if (!value) { Reset(nullptr); return; }
    std::scoped_lock lock(mutex); active = true;
}
void SelectedNPCs::CaptureTargets() {
    Queue([] {
        // Opening a menu may already have cleared the engine's crosshair target.
        crosshair = lastCrosshair; console = {};
        if (const auto pick = RE::CrosshairPickData::GetSingleton()) {
            const auto ref = pick->GetActiveTarget().get();
            if (const auto actor = ref ? ref->As<RE::Actor>() : nullptr; Eligible(actor)) crosshair = actor->GetHandle();
        }
        const auto ref = RE::Console::GetSelectedRef();
        if (const auto actor = ref ? ref->As<RE::Actor>() : nullptr; Eligible(actor)) console = actor->GetHandle();
    });
}
SelectedNPCs::View SelectedNPCs::Snapshot() { std::scoped_lock lock(mutex); return view; }
void SelectedNPCs::AddTarget(bool useConsole) {
    Queue([useConsole] {
        const auto actor = (useConsole ? console : crosshair).get();
        if (!Eligible(actor.get()) || records.size() >= limit) return;
        const auto id = actor->GetFormID();
        if (std::none_of(records.begin(), records.end(), [&](auto& row) { return row.id == id; })) records.push_back({id, 1});
    });
}
void SelectedNPCs::Remove(RE::FormID id) { Queue([id] { std::erase_if(records, [id](auto& row) { return row.id == id; }); }); }
void SelectedNPCs::SetEnabled(RE::FormID id, bool enabled) { Queue([id, enabled] { for (auto& row : records) if (row.id == id) row.enabled = enabled; }); }
std::vector<RE::ActorHandle> SelectedNPCs::Update() {
    std::scoped_lock lock(mutex);
    std::vector<RE::ActorHandle> result;
    view = {}; view.active = active;
    if (!active) return result;
    if (!ConfigMenu::IsOpen()) {
        lastCrosshair = {};
        if (const auto pick = RE::CrosshairPickData::GetSingleton()) {
            const auto ref = pick->GetActiveTarget().get();
            if (const auto actor = ref ? ref->As<RE::Actor>() : nullptr; Eligible(actor)) lastCrosshair = actor->GetHandle();
        }
    }
    const auto targetName = [](const RE::ActorHandle& handle) -> std::string {
        const auto actor = handle.get();
        return Eligible(actor.get()) ? std::format("{} [{:08X}]", actor->GetName(), actor->GetFormID()) : "";
    };
    view.crosshair = targetName(crosshair); view.console = targetName(console);
    for (const auto& row : records) {
        const auto actor = RE::TESForm::LookupByID<RE::Actor>(row.id);
        const auto cell = actor ? actor->GetParentCell() : nullptr;
        const bool loaded = Eligible(actor) && actor->Get3D(false) && cell && cell->IsAttached();
        view.rows.push_back({row.id, actor ? actor->GetName() : "", row.enabled != 0, loaded});
        if (loaded && row.enabled) result.push_back(actor->GetHandle());
    }
    return result;
}
