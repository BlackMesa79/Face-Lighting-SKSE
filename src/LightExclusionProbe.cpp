#include <SKSE/SKSE.h>
#include "LightExclusionProbe.h"
#include <Windows.h>
#include <intrin.h>
#include <array>
#include <atomic>
#include <chrono>
#include <mutex>
#include <shared_mutex>
#include <unordered_map>
#include "LightCallScan.h"
#include "Exclusion1170Layout.h"
#include "ExclusionTotals.h"
#include "Settings.h"
#include "AmbientSample.h"

namespace {
    using Function = float (*)(RE::BSLight*, RE::NiPoint3*, RE::NiLight*);
    Function original = nullptr;
    using QueryFunction = float (*)(RE::ShadowSceneNode*, RE::NiPoint3*, std::uint32_t*, float*, float*, RE::NiLight*, std::uint32_t);
    using StoreFunction = void (*)(RE::AIProcess*, float);
    QueryFunction originalQuery = nullptr;
    StoreFunction originalStore = nullptr;
    bool pairingInstalled = false;
    float queryScale = 0;
    std::uintptr_t imageBase = 0;
    std::atomic<bool> collecting = false;
    std::atomic<bool> logging = false;
    std::atomic<std::uint64_t> epoch = 0;
    bool installed = false;
    std::mutex stateMutex;
    std::string status = "disabled; enable, save and restart game";
    std::chrono::steady_clock::time_point next{};

    // Registry tracks identity only; the owning LightInstance controls NiLight lifetime.
    struct Registry {
        std::shared_mutex mutex;
        std::unordered_map<RE::NiLight*, unsigned> sources;
    };
    Registry& Lights() { static auto* value = new Registry; return *value; }

    struct Event {
        std::uintptr_t caller = 0;
        DWORD thread = 0;
        RE::NiPoint3 position{};
        float contribution = 0;
        bool referenceMatch = false;
    };
    struct Batch {
        std::array<std::uint64_t, 3> calls{};
        std::array<unsigned, 3> used{};
        std::array<std::array<Event, 4>, 3> examples{};
    };
    struct Pair {
        ExclusionTotals totals;
        float query = 0, raw = 0, cached = 0;
        std::optional<float> filtered;
        RE::NiPoint3 position{};
        DWORD thread = 0;
        std::chrono::steady_clock::time_point time{};
        std::string reason;
    };
    struct Pending {
        Pair pair;
        void* callerFrame = nullptr;
        std::uint64_t generation = 0;
        bool ready = false;
    };
    thread_local ExclusionTotals* activeQuery = nullptr;
    thread_local Pending pending;
    std::optional<Pair> latestPair;
    AmbientSample controlSample;
    RE::AIProcess* sampleProcess = nullptr;
    std::chrono::steady_clock::time_point lastUpdate{};
    std::uint64_t pairCount = 0, rejectedCount = 0;
    std::mutex batchMutex;
    Batch batch;
    constexpr const char* sourceNames[]{"external", "player", "npc"};

    float Observe(RE::BSLight* light, RE::NiPoint3* position, RE::NiLight* reference) {
        if (!collecting.load(std::memory_order_relaxed)) return original(light, position, reference);
        const auto generation = epoch.load();
        auto* niLight = light ? light->light.get() : nullptr;
        unsigned source = 0;
        {
            auto& registry = Lights();
            std::shared_lock lock(registry.mutex);
            if (const auto it = registry.sources.find(niLight); it != registry.sources.end()) source = it->second;
        }
        Event sample;
        sample.caller = reinterpret_cast<std::uintptr_t>(_ReturnAddress()) - imageBase;
        sample.thread = GetCurrentThreadId();
        if (position) sample.position = *position;
        sample.referenceMatch = niLight && niLight == reference;
        // Preserve exactly one call through the existing engine/CS chain and its result.
        const float result = original(light, position, reference);
        if (activeQuery) activeQuery->Add(source, result);
        if (!logging.load()) return result;
        sample.contribution = result;
        std::scoped_lock lock(batchMutex);
        if (collecting.load() && generation == epoch.load()) {
            ++batch.calls[source];
            if (batch.used[source] < batch.examples[source].size())
                batch.examples[source][batch.used[source]++] = sample;
        }
        return result;
    }

    float ObserveQuery(RE::ShadowSceneNode* scene, RE::NiPoint3* position, std::uint32_t* count,
        float* ambient, float* directional, RE::NiLight* reference, std::uint32_t shadowMask) {
        pending.ready = false;
        if (!collecting.load()) return originalQuery(scene, position, count, ambient, directional, reference, shadowMask);
        Pending capture;
        capture.callerFrame = _AddressOfReturnAddress();
        capture.generation = epoch.load();
        capture.pair.thread = GetCurrentThreadId();
        if (position) capture.pair.position = *position;
        // A nested observed query uses its own accumulator; return to the outer one afterwards.
        {
            struct Scope {
                ExclusionTotals* previous;
                explicit Scope(ExclusionTotals* value) : previous(activeQuery) { activeQuery = value; }
                ~Scope() { activeQuery = previous; }
            } scope(&capture.pair.totals);
            capture.pair.query = originalQuery(scene, position, count, ambient, directional, reference, shadowMask);
        }
        capture.pair.time = std::chrono::steady_clock::now();
        capture.ready = collecting.load() && capture.generation == epoch.load();
        pending = capture;
        return capture.pair.query;
    }

    void ObserveStore(RE::AIProcess* process, float value) {
        const auto capture = pending;
        pending.ready = false; // Never reuse a sample for a later store.
        const auto frame = _AddressOfReturnAddress();
        originalStore(process, value);
        if (!collecting.load()) return;
        const auto player = RE::PlayerCharacter::GetSingleton();
        if (!player || player->GetActorRuntimeData().currentProcess != process) return;
        Pair result = capture.pair;
        result.raw = value;
        result.time = std::chrono::steady_clock::now();
        const bool paired = capture.ready && capture.callerFrame == frame &&
            capture.generation == epoch.load() && result.time - capture.pair.time < std::chrono::milliseconds(100);
        if (!paired) result.reason = "unpaired_query";
        else if (!process || !process->InHighProcess() || !process->high) result.reason = "no_high_process";
        else {
            result.cached = process->high->lightLevel;
            result.filtered = result.totals.Filter(result.query, value, result.cached, queryScale);
            if (!result.filtered) result.reason = "sum_or_cache_mismatch";
        }
        std::scoped_lock lock(batchMutex);
        if (!collecting.load() || capture.generation != epoch.load()) return;
        const auto cell = player->GetParentCell();
        controlSample = {result.filtered, std::chrono::duration<double>(result.time.time_since_epoch()).count(),
            cell ? cell->GetFormID() : 0};
        sampleProcess = process;
        if (result.filtered) ++pairCount; else ++rejectedCount;
        latestPair = result;
    }

    std::vector<std::uint8_t> FunctionBytes(std::uintptr_t address) {
        DWORD64 base = 0;
        const auto entry = RtlLookupFunctionEntry(address, &base, nullptr);
        if (!entry || base != imageBase || base + entry->BeginAddress != address ||
            entry->EndAddress <= entry->BeginAddress || entry->EndAddress - entry->BeginAddress > 4096) return {};
        const auto size = entry->EndAddress - entry->BeginAddress;
        MEMORY_BASIC_INFORMATION region{};
        if (!VirtualQuery(reinterpret_cast<void*>(address), &region, sizeof(region)) || region.State != MEM_COMMIT ||
            region.Protect & (PAGE_NOACCESS | PAGE_GUARD) ||
            !(region.Protect & (PAGE_EXECUTE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY)) ||
            address + size > reinterpret_cast<std::uintptr_t>(region.BaseAddress) + region.RegionSize) return {};
        const auto* begin = reinterpret_cast<const std::uint8_t*>(address);
        return {begin, begin + size};
    }
    void Dump(const char* name, std::uintptr_t address, const std::vector<std::uint8_t>& bytes) {
        SKSE::log::info("[LightExclusion] code {} rva={:X} bytes={}", name, address - imageBase, bytes.size());
        for (std::size_t offset = 0; offset < bytes.size(); offset += 64) {
            std::string hex;
            for (auto i = offset; i < std::min(offset + 64, bytes.size()); ++i)
                hex += std::format("{:02X}", bytes[i]);
            SKSE::log::info("[LightExclusion] code {} +{:X} {}", name, offset, hex);
        }
    }
}

void LightExclusionProbe::Install() {
    std::scoped_lock lock(stateMutex);
    const auto settings = Settings::Get();
    if ((!settings.exclusionDiagnostics && settings.ambientMode != 3) || installed) return;
    // Call-site diagnostics are deliberately restricted to the researched runtime.
    if (REL::Module::get().version() != REL::Version(1, 6, 1170, 0)) {
        status = "unsupported runtime; requires Skyrim 1.6.1170";
        SKSE::log::warn("[LightExclusion] {}", status);
        return;
    }
    imageBase = REL::Module::get().base();
    const auto query = REL::ID(106362).address();
    const auto luminance = REL::ID(108292).address();
    const auto calculate = REL::ID(39946).address();
    const auto store = REL::ID(39510).address();
    const auto code = FunctionBytes(query);
    const auto calculateCode = FunctionBytes(calculate);
    if (settings.exclusionDiagnostics) {
    Dump("GetLuminanceAtPoint", query, code);
    Dump("CalculateLightValue", calculate, calculateCode);
    Dump("GetLightLevel", REL::ID(39509).address(), FunctionBytes(REL::ID(39509).address()));
    Dump("BSLight_GetLuminance", luminance, FunctionBytes(luminance));
    }
    const auto sites = LightCallScan(code, query, luminance);
    if (!sites || sites->empty() || sites->size() > 4) {
        status = "unavailable: query changed or no validated direct luminance calls; code logged";
        SKSE::log::warn("[LightExclusion] {}", status);
        return;
    }
    const auto queryCalls = LightCallScan(calculateCode, calculate, query);
    const auto storeCalls = LightCallScan(calculateCode, calculate, store);
    const bool layoutVerified = Exclusion1170::Matches(code, calculateCode) &&
        queryCalls && *queryCalls == std::vector<std::size_t>{0x1D3} &&
        storeCalls && *storeCalls == std::vector<std::size_t>{0x2FC};
    if (layoutVerified) {
        // Verified MULSS xmm6,[RIP+disp32] immediately before the final value is passed to SetLightLevel.
        std::int32_t displacement;
        std::memcpy(&displacement, calculateCode.data() + 0x2F2, sizeof(displacement));
        const auto constant = calculate + 0x2F6 + displacement;
        // In this runtime the constant must be in the game's read-only data segment.
        const auto data = REL::Module::get().segment(REL::Segment::rdata);
        if (constant >= data.address() && constant + sizeof(float) <= data.address() + data.size())
            std::memcpy(&queryScale, reinterpret_cast<const void*>(constant), sizeof(queryScale));
    }
    if (std::memcmp(reinterpret_cast<const void*>(query), code.data(), code.size()) != 0 ||
        std::memcmp(reinterpret_cast<const void*>(calculate), calculateCode.data(), calculateCode.size()) != 0) {
        status = "unavailable: query changed during validation";
        SKSE::log::warn("[LightExclusion] {}", status);
        return;
    }
    // Private trampoline; never resize/replace a shared SKSE trampoline used by another hook.
    static auto* trampoline = new SKSE::Trampoline("FaceLighting luminance observer");
    trampoline->create(256, reinterpret_cast<void*>(query));
    original = reinterpret_cast<Function>(luminance);
    originalQuery = reinterpret_cast<QueryFunction>(query);
    originalStore = reinterpret_cast<StoreFunction>(store);
    for (const auto offset : *sites) {
        trampoline->write_call<5>(query + offset, Observe);
        FlushInstructionCache(GetCurrentProcess(), reinterpret_cast<void*>(query + offset), 5);
        SKSE::log::info("[LightExclusion] observed call site rva={:X}; original target={:X}",
            query + offset - imageBase, luminance - imageBase);
    }
    if (layoutVerified && queryScale == 100.0f) {
        trampoline->write_call<5>(calculate + 0x1D3, ObserveQuery);
        trampoline->write_call<5>(calculate + 0x2FC, ObserveStore);
        FlushInstructionCache(GetCurrentProcess(), reinterpret_cast<void*>(calculate), calculateCode.size());
        pairingInstalled = true;
        SKSE::log::info("[LightExclusion] paired-query diagnostics installed; scale={}; original cache and return values preserved", queryScale);
    } else {
        SKSE::log::warn("[LightExclusion] paired-query unavailable: unverified calculation layout or scale; passive observer retained");
    }
    installed = true;
    status = pairingInstalled ? "paired-query ready; waiting for a completed player sample" : "observer ready; filtered=unavailable:unverified_layout";
    SKSE::log::info("[LightExclusion] {}; no return-value filtering or raw-cache writes", status);
}

void LightExclusionProbe::Register(RE::NiLight* light, bool player) {
    if (!light) return;
    auto& registry = Lights();
    std::unique_lock lock(registry.mutex);
    registry.sources[light] = player ? 1u : 2u;
}
void LightExclusionProbe::Unregister(RE::NiLight* light) {
    auto& registry = Lights();
    std::unique_lock lock(registry.mutex);
    registry.sources.erase(light);
}
void LightExclusionProbe::Reset() {
    collecting.store(false);
    ++epoch;
    std::scoped_lock lock(batchMutex);
    batch = {};
    latestPair.reset(); pairCount = rejectedCount = 0;
    controlSample = {}; sampleProcess = nullptr; lastUpdate = {};
    next = {};
}
void LightExclusionProbe::Update(bool enabled, bool gameplay, bool diagnostics) {
    if (!enabled || !gameplay || !installed) { Reset(); return; }
    const auto now = std::chrono::steady_clock::now();
    if (lastUpdate != std::chrono::steady_clock::time_point{} && now - lastUpdate > std::chrono::milliseconds(500)) Reset();
    lastUpdate = now;
    logging.store(diagnostics);
    collecting.store(true);
    if (now < next) return;
    next = now + std::chrono::seconds(1);
    Batch captured;
    std::optional<Pair> capturedPair;
    std::uint64_t pairs = 0, rejected = 0;
    {
        std::scoped_lock lock(batchMutex);
        captured = batch;
        batch = {};
        capturedPair = latestPair;
        latestPair.reset();
        pairs = pairCount; rejected = rejectedCount;
        pairCount = rejectedCount = 0;
    }
    auto summary = std::format("externalCalls={} playerCalls={} npcCalls={} paired={} rejected={}",
        captured.calls[0], captured.calls[1], captured.calls[2], pairs, rejected);
    if (capturedPair) {
        const auto& p = *capturedPair;
        summary += std::format(" pairedRaw={:.6f} filtered={} cached={:.6f} query={:.6f} observed={:.6f} external={:.6f} excluded={:.6f} playerPart={:.6f} npcPart={:.6f} ageMs={:.1f} thread={} pos=({:.3f},{:.3f},{:.3f})",
            p.raw, p.filtered ? std::format("{:.6f}", *p.filtered) : "unavailable:" + p.reason,
            p.cached, p.query, p.totals.all, p.totals.external, (p.totals.all - p.totals.external) * queryScale,
            p.totals.player * queryScale, p.totals.npc * queryScale,
            std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - p.time).count(), p.thread, p.position.x, p.position.y, p.position.z);
    } else summary += pairingInstalled ? " filtered=unavailable:no_completed_player_sample" : " filtered=unavailable:unverified_layout";
    { std::scoped_lock lock(stateMutex); status = summary; }
    if (!diagnostics) return;
    SKSE::log::info("[LightExclusion] {}", summary);
    for (unsigned source = 0; source < 3; ++source) {
        for (unsigned i = 0; i < captured.used[source]; ++i) {
            const auto& e = captured.examples[source][i];
            SKSE::log::info("[LightExclusion] source={} callerRva={:X} thread={} pos=({:.3f},{:.3f},{:.3f}) contribution={:.6f} referenceMatch={}",
                sourceNames[source], e.caller, e.thread, e.position.x, e.position.y, e.position.z,
                e.contribution, e.referenceMatch);
        }
    }
}
std::string LightExclusionProbe::Snapshot() { std::scoped_lock lock(stateMutex); return status; }
std::optional<float> LightExclusionProbe::ReadFiltered(RE::Actor* player) {
    std::scoped_lock lock(batchMutex);
    if (!collecting.load() || !pairingInstalled || !player ||
        player->GetActorRuntimeData().currentProcess != sampleProcess) return {};
    const auto cell = player->GetParentCell();
    return controlSample.Read(std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count(),
        cell ? cell->GetFormID() : 0);
}
