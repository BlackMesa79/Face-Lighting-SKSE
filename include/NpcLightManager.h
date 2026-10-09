#pragma once
#include "LightTransition.h"
#include <algorithm>
#include <cstddef>
#include <memory>
#include <limits>
#include <vector>

// Internal request priorities; these do not enable any target-selection feature.
enum class NpcLightSource { Nearby, Selected, Follower, Dialogue, External };

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
        bool visible = true;
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
    void Submit(const Key& actor, NpcLightSource source, const Parameters& parameters, bool transition, float duration, bool visible = true) {
        for (auto& request : requests) {
            if (request.actor == actor && request.source == source) {
                request = {actor, source, parameters, transition, duration, visible};
                return;
            }
        }
        requests.push_back({actor, source, parameters, transition, duration, visible});
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
    void DropSource(NpcLightSource source) {
        std::erase_if(requests, [source](const auto& request) { return request.source == source; });
        std::erase_if(entries, [source](auto& entry) {
            if (entry->request.source != source) return false;
            entry->light.Clear(); return true;
        });
    }
    std::size_t Size() const { return entries.size(); }
    const Light* Find(const Key& actor) const {
        for (const auto& entry : entries) if (entry && entry->request.actor == actor) return &entry->light;
        return nullptr;
    }
    float Opacity(const Key& actor) const {
        for (const auto& entry : entries) if (entry && entry->request.actor == actor) return entry->fade.value;
        return 0;
    }
    // Hard release denied lights before rendering protected actors. A fade-out
    // would still consume renderer resources. Preferences are never changed.
    void Protect(std::size_t secondaryLimit, bool dialogueOpen = false, const Key* fadingDialogue = nullptr) {
        std::stable_sort(requests.begin(), requests.end(), [](const auto& a, const auto& b) { return a.source > b.source; });
        const bool dialogue = dialogueOpen || std::any_of(requests.begin(), requests.end(), [](const auto& r) { return r.source >= NpcLightSource::Dialogue; });
        std::vector<Key> accepted;
        std::vector<Key> denied;
        std::size_t secondary = 0;
        for (const auto& request : requests) {
            if (std::find(accepted.begin(), accepted.end(), request.actor) != accepted.end() ||
                std::find(denied.begin(), denied.end(), request.actor) != denied.end()) continue;
            if (request.source >= NpcLightSource::Dialogue ||
                (!dialogue && secondary < secondaryLimit)) {
                accepted.push_back(request.actor);
                if (request.source < NpcLightSource::Dialogue) ++secondary;
            } else denied.push_back(request.actor);
        }
        std::erase_if(requests, [&](const auto& r) { return std::find(denied.begin(), denied.end(), r.actor) != denied.end(); });
        // Include outgoing lights in the budget, and release an old dialogue
        // target immediately when a new protected target takes over.
        std::erase_if(entries, [&](auto& entry) {
            const auto& r = entry->request;
            bool drop = std::find(denied.begin(), denied.end(), r.actor) != denied.end();
            if (!Winner(r.actor)) {
                const bool keepDialogueFade = dialogue && fadingDialogue && r.actor == *fadingDialogue &&
                    r.source == NpcLightSource::Dialogue;
                if (!keepDialogueFade) {
                    if (dialogue || secondary >= secondaryLimit) drop = true;
                    else if (!drop) ++secondary;
                }
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
    void Update(float delta, Valid valid, Render render, std::size_t secondaryLimit = std::numeric_limits<std::size_t>::max(), bool dialogueOpen = false, const Key* fadingDialogue = nullptr) {
        // Invalid actors cannot reserve capacity or retain a scene object during a fade.
        std::erase_if(requests, [&](const auto& request) { return !valid(request.actor); });
        std::erase_if(entries, [&](auto& entry) {
            if (valid(entry->request.actor)) return false;
            entry->light.Clear();
            return true;
        });
        if (secondaryLimit != std::numeric_limits<std::size_t>::max()) Protect(secondaryLimit, dialogueOpen, fadingDialogue);
        std::stable_sort(requests.begin(), requests.end(), [](const auto& a, const auto& b) { return a.source > b.source; });
        for (const auto& request : requests) {
            if (Winner(request.actor) != &request) continue;
            auto found = std::find_if(entries.begin(), entries.end(), [&](const auto& entry) { return entry->request.actor == request.actor; });
            if (found != entries.end()) {
                auto updated = request;
                if (!request.visible) updated.parameters = (*found)->request.parameters;
                (*found)->request = updated;
                continue;
            }
            if (!request.visible) continue; // Suppress lower sources without creating an invisible scene object.
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
            const auto winner = Winner(entry.request.actor);
            const bool visible = winner && winner->visible;
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
