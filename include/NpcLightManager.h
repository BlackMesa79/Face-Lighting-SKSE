#pragma once
#include "LightTransition.h"
#include <algorithm>
#include <cstddef>
#include <memory>
#include <vector>

// Internal request priorities; these do not enable any target-selection feature.
enum class NpcLightSource { Nearby, Follower, Selected, Dialogue };

// Main-thread only. Each producer renews its requests every frame. Keys must be
// lifetime-safe actor handles, not raw pointers or reusable FormIDs.
template <class Key, class Parameters, class Light>
class NpcLightManager {
    struct Request {
        Key actor;
        NpcLightSource source;
        Parameters parameters;
        bool transition;
        float duration;
    };
    struct Entry {
        Request request;
        Light light;
        LightTransition fade;
        explicit Entry(const Request& value) : request(value) {}
    };
    std::size_t capacity;
    std::vector<Request> requests;
    std::vector<std::unique_ptr<Entry>> entries;

    const Request* Winner(const Key& actor) const {
        const Request* winner = nullptr;
        for (const auto& request : requests)
            if (request.actor == actor && (!winner || request.source > winner->source)) winner = &request;
        return winner;
    }

public:
    explicit NpcLightManager(std::size_t limit) : capacity(limit) {}
    NpcLightManager(const NpcLightManager&) = delete;
    NpcLightManager& operator=(const NpcLightManager&) = delete;
    void BeginFrame() { requests.clear(); }
    void Submit(const Key& actor, NpcLightSource source, const Parameters& parameters, bool transition, float duration) {
        for (auto& request : requests) {
            if (request.actor == actor && request.source == source) {
                request = {actor, source, parameters, transition, duration};
                return;
            }
        }
        requests.push_back({actor, source, parameters, transition, duration});
    }
    // Keep outgoing lights responsive to the existing dialogue preview controls.
    void RefreshSource(NpcLightSource source, const Parameters& parameters, bool transition, float duration) {
        for (auto& entry : entries) if (entry->request.source == source) {
            entry->request.parameters = parameters;
            entry->request.transition = transition;
            entry->request.duration = duration;
        }
    }
    void Clear() {
        for (auto& entry : entries) entry->light.Clear();
        entries.clear();
        requests.clear();
    }
    std::size_t Size() const { return entries.size(); }
    bool Contains(const Key& actor) const {
        return std::any_of(entries.begin(), entries.end(), [&](const auto& e) { return e && e->request.actor == actor; });
    }

    // Hard release denied lights before rendering protected actors. A fade-out
    // would still consume renderer resources. Preferences are never changed.
    template <class Allow>
    void Protect(std::size_t secondaryLimit, Allow allow) {
        std::stable_sort(requests.begin(), requests.end(), [](const auto& a, const auto& b) { return a.source > b.source; });
        const bool dialogue = std::any_of(requests.begin(), requests.end(), [](const auto& r) { return r.source == NpcLightSource::Dialogue; });
        std::vector<Key> accepted;
        std::vector<Key> denied;
        std::size_t secondary = 0;
        for (const auto& request : requests) {
            if (std::find(accepted.begin(), accepted.end(), request.actor) != accepted.end() ||
                std::find(denied.begin(), denied.end(), request.actor) != denied.end()) continue;
            if (request.source == NpcLightSource::Dialogue ||
                (!dialogue && secondary < secondaryLimit && allow(request.actor, request.parameters))) {
                accepted.push_back(request.actor);
                if (request.source != NpcLightSource::Dialogue) ++secondary;
            } else denied.push_back(request.actor);
        }
        std::erase_if(requests, [&](const auto& r) { return std::find(denied.begin(), denied.end(), r.actor) != denied.end(); });
        // Include outgoing lights in the budget, and release an old dialogue
        // target immediately when a new protected target takes over.
        std::erase_if(entries, [&](auto& entry) {
            const auto& r = entry->request;
            bool drop = std::find(denied.begin(), denied.end(), r.actor) != denied.end();
            if (!Winner(r.actor)) {
                if (dialogue || !allow(r.actor, r.parameters) || secondary >= secondaryLimit) drop = true;
                else if (!drop) ++secondary;
            }
            if (drop) {
                if constexpr (requires { entry->light.Clear("priority protection"); })
                    entry->light.Clear("priority protection");
                else entry->light.Clear();
            }
            return drop;
        });
    }

    template <class Valid, class Render>
    void Update(float delta, Valid valid, Render render) {
        // Invalid actors cannot reserve capacity or retain a scene object during a fade.
        std::erase_if(requests, [&](const auto& request) { return !valid(request.actor); });
        std::erase_if(entries, [&](auto& entry) {
            if (valid(entry->request.actor)) return false;
            entry->light.Clear();
            return true;
        });
        std::stable_sort(requests.begin(), requests.end(), [](const auto& a, const auto& b) { return a.source > b.source; });
        for (const auto& request : requests) {
            if (Winner(request.actor) != &request) continue;
            auto found = std::find_if(entries.begin(), entries.end(), [&](const auto& entry) { return entry->request.actor == request.actor; });
            if (found != entries.end()) {
                (*found)->request = request;
                continue;
            }
            if (capacity == 0) continue;
            if (entries.size() >= capacity) {
                // Prefer outgoing lights, then preempt a lower-priority active actor.
                auto outgoing = std::find_if(entries.begin(), entries.end(), [&](const auto& entry) { return !Winner(entry->request.actor); });
                if (outgoing == entries.end()) {
                    outgoing = std::min_element(entries.begin(), entries.end(), [&](const auto& a, const auto& b) {
                        return Winner(a->request.actor)->source < Winner(b->request.actor)->source;
                    });
                    if (outgoing == entries.end() || Winner((*outgoing)->request.actor)->source >= request.source) continue;
                }
                (*outgoing)->light.Clear();
                entries.erase(outgoing);
            }
            entries.push_back(std::make_unique<Entry>(request));
        }
        // Keep the dialogue transition budget independent of the persistent roster.
        auto outgoingDialogue = std::count_if(entries.begin(), entries.end(), [&](const auto& entry) {
            return entry->request.source == NpcLightSource::Dialogue && !Winner(entry->request.actor);
        });
        std::erase_if(entries, [&](auto& entry) {
            if (outgoingDialogue <= 1 || entry->request.source != NpcLightSource::Dialogue || Winner(entry->request.actor)) return false;
            --outgoingDialogue;
            entry->light.Clear();
            return true;
        });
        for (auto it = entries.begin(); it != entries.end();) {
            auto& entry = **it;
            const bool visible = Winner(entry.request.actor) != nullptr;
            entry.fade.Update(visible, entry.request.transition, entry.request.duration, delta);
            if (!visible && entry.fade.value <= 0) {
                entry.light.Clear();
                it = entries.erase(it);
            } else {
                render(entry.light, entry.request.actor, entry.request.parameters, entry.fade.value);
                ++it;
            }
        }
    }
};
