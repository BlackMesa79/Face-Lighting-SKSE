# Face Lighting SKSE API V2 — CCC integration

Available in Face Lighting SKSE 0.9.7 (2026-10-09). The user reported no issues during local gameplay testing of the V2 development build. Unit/ABI tests cover the implementation; CCC client integration still needs joint testing. Older 0.9.6 downloads do not include V2.

V2 lets CCC request temporary lighting for player-to-NPC and NPC-to-NPC conversations. CCC chooses participants, timing, colors and brightness. Face Lighting owns the actual scene lights, fades and cleanup. No CCC interface is required by Face Lighting: CCC can discover and call this optional API directly.

## Headers and discovery

Use `include/FaceLightingAPI.h` and `include/FaceLightingAPIV2.h`. They require only standard fixed-width types and type traits; no CommonLib/SKSE dependency or Face Lighting import library.

```cpp
auto module = GetModuleHandleW(L"FaceLighting.dll");
auto getAPI = module ? reinterpret_cast<FaceLightingAPI::V2::GetAPI>(
    GetProcAddress(module, FaceLightingAPI::exportName)) : nullptr;
const auto api = getAPI ? getAPI(FaceLightingAPI::V2::version) : nullptr;
if (!api || api->apiVersion != 2 ||
    api->structSize < sizeof(FaceLightingAPI::V2::Interface)) {
    // Continue without the lighting integration.
}
```

Discover after SKSE PostPostLoad. Never load/unload the plugin yourself. `FaceLighting_GetAPI(1)` still returns the original 64-byte V1 table, unchanged. Request 2 using the **V2 getter type**; V2 is a separate 80-byte table with `v1` pointing to the original table. Unknown versions return null. This preserves existing Favorite Wheel clients.

The getter is thread-independent. **Every table callback runs on the game thread**, for example a SKSE main-thread task. Render/UI threads must queue intent and read their own cached results. There are no callbacks into CCC and no engine pointers or STL containers crossing DLLs. Initialize structures using their defaults, keep reserved fields zero, and keep caller buffers valid throughout each call. Requests are copied before return. Target arrays use the exact supplied `Target` layout/stride; larger `structSize` values do not change array stride.

## Lifecycle

| Function | Contract |
| --- | --- |
| `v1->GetContext` | Get the current world/save generation. Check `ready`. |
| `ResolveActor(formID, ActorState*)` | Resolve an existing runtime reference FormID, including player `0x14`; returns a token. Never loads cells. Do not pass an NPC base FormID. |
| `BeginSession(BeginInfo*, Session*)` | Begin an empty lease using `context.session` as `world`. One external session at a time; a second returns `Blocked`. No scene lighting changes until a nonempty update. |
| `UpdateSession(UpdateInfo*)` | Replace the **entire** target snapshot (0–4 participants, including player). Validate all targets before committing; success also renews the lease. |
| `RenewSession(Session*)` | Extend the lease without changing targets or pause state. |
| `EndSession(Session*)` | Remove external requests and release their scene lights; restore normal lighting demands on the next update. |
| `QuerySession(Session*, SessionState*)` | Read pause/count/remaining wall-clock lease. Does not renew. Count may decrease when targets become invalid. |
| `GetEnvironment(Environment*)` | Read available player-position lighting samples, with validity flags. No world scan or forced engine recalculation. |

Begin before the ordinary DialogueMenu opening edge if CCC wants to avoid Face Lighting's existing persistent “enable player on dialogue” behavior. During a leased conversation, automatic player dialogue switch writes are suppressed, including its closing edge if the external session ends first. An automatic write that already occurred before Begin cannot be undone by V2.

The default lease is 5 seconds; valid leases are 1–30 seconds. Renew about once per second, including during trading/pause if retaining the session. Lease time is monotonic wall time, not game time. Expired handles return `StaleSession`; create a new session and resolve targets again. Load/new-game/main-menu/loading transitions invalidate the temporary session. Dead, disabled, unloaded or invalid-model targets are removed instead of being forcibly loaded or retried indefinitely.

For pause, send a full snapshot with `paused=1`; for resume, resolve current participants again and send `paused=0`. Pause, an empty snapshot, and End release external scene lights immediately. Ordinary sources resume under the user's normal rules. Configuration previews temporarily suppress external rendering without ending the lease; Begin and unpaused updates return `BusyPreview` while the menu/preview is active. Unpaused writes return `Blocked` during engine pause or console. Pause, Renew, End and Query remain permitted on the game thread during those interruptions. Stale/invalid targets still make a paused full snapshot fail; send an empty paused snapshot to release rendering when old targets cannot be resolved, then rebuild on resume.

**Always call End** on conversation exit, CCC disable, aborted scenes and camera ownership loss. If cleanup is missed, timeout provides a fallback when the game-thread update resumes. Clear local handles on world changes and `StaleSession`. Do not serialize session handles. `Ok` means a request was committed, not that the engine displayed a light.

## Parameters and arbitration

`Target.light` is a full parameter set:

| Field | Range / meaning |
| --- | --- |
| `enabled` | 0/1. A disabled target suppresses normal NPC fallback while the session owns the scene. |
| `intensity` | 0–5; zero is off. |
| `radius` | 10–500 game units; manual range for standard and CS inverse-square lighting. |
| `colorMode` | `Temperature` or `SRGB`. |
| `temperature` | 2000–10000 K. |
| `red`, `green`, `blue` | 0–1 sRGB input, converted when the user's CS linear settings require it. |
| `offsetSpace` | `ActorHeading` or `HeadBone`. |
| `offsetX/Y/Z` | −150…150 game units, anchored at the standard head bone; actor-heading axes are right/forward/up. HeadBone uses the bone's local axes, excluding skeleton scale. |
| `transitionSeconds` | 0–3 seconds for emitted-light fade in/out; default 0.2. Color/brightness/position edits apply immediately and are not interpolated. |

All numeric fields must be finite and in range, even fields unused by the selected color mode. Unknown enums, duplicates, invalid tokens and invalid targets reject the full update. No partial target change or lease renewal occurs on validation failure.

An unpaused, nonempty session owns **NPC** dialogue lighting, even without DialogueMenu, supporting NPC-to-NPC scenes. It suppresses normal dialogue/follower/selected NPC lights and merges external targets into the existing one-light-per-actor manager. Up to four participants bypass the follower/selected budget; they still cost engine light resources. A listed player target temporarily overrides player parameters/on-off state. If omitted, the player's ordinary light demand continues.

Temporary requests do not edit INI, selected lists or follower preferences. Normal needs restore after End/pause/empty update/expiry. Explicit `enabled=0` fades an existing light out; leaving a target out of the next snapshot releases it immediately during the exclusive scene. End and safety cleanup are immediate, so fade targets off first and continue renewing if a smooth scene exit is desired.

Death/model safety, the user's sneak hiding, and first-person permission always apply. Temporary lighting bypasses normal player/dialogue ambient gates: CCC decides whether to request it. Standard CS integration settings are inherited; NPC requests use dialogue CS parameters, player requests use player CS parameters. No engine-drop detection is promised. Lighting remains a head-attached omnidirectional point light; camera-relative offsets, arbitrary bones and directional lights are outside V2.

`GetEnvironment` samples at the **player**, not the NPC skin or camera. Inspect `RawValid` and `FilteredValid` separately. Raw can include our lights and has unknown cache age. Filtered excludes our lights only when the existing live-exclusion hook was enabled at startup, is collecting, and has a valid sample; currently this is limited to Skyrim 1.6.1170. The API does not silently install hooks, change user settings or fall back to raw. `Ok` with no validity flags is valid; hold a decision or use CCC's own environment logic. Do not use these values as physical luminance or direct sun exposure.

## CCC integration checklist

1. Optionally discover V2; work normally without it. Offer a CCC-side integration toggle.
2. On a scene start, Begin, resolve reference FormIDs, and submit the participants. Include player when desired. Two NPCs work without a player target.
3. On speaker/camera changes, submit a new complete snapshot. CCC can compute environment-based colors/brightness itself or use valid API samples.
4. Renew on a main-thread timer. Pause/rebuild around trading, previews and camera suspension. Handle `BusyPreview`, `Blocked`, `NotLoaded` and `StaleSession` without repeatedly forcing writes each frame.
5. End on every normal/abnormal exit and on CCC disable. Discard handles across saves. Test NPC↔NPC, player↔NPC, rapid speaker changes, first person, sneaking, death, unloading, load, timeout and restoration of pre-existing lights.

See the compilable [client example](examples/ccc-face-lighting-v2.cpp). Face Lighting's V1 actor flags still describe saved personal preferences; they do not represent temporary enabled state. `Registered`/`Fading` reflect managed runtime lights and are not proof of visible engine illumination.
