# CommonLibSSE-NG 1.7.x migration and review

Release follow-up (2026-10-05): the author reports successful 1.6.1170 gameplay
and Favorite Wheel integration testing and authorizes packaging 0.9.5 for release.
The runtime logic/dependency pin are unchanged from experimental 1; version labels
and release documentation are updated. 1.7.99/104 gameplay reports remain pending.
The candidate validation and hash below describe the earlier experimental DLL.

Date: 2026-10-05 (Asia/Hong_Kong). Candidate: **0.9.5-experimental.1**.
Stable source baseline: `88f8512` (0.9.4). Local branch: `codex/commonlib-1-7-experimental`.
This is a local test candidate; gameplay compatibility is not established by compilation.

## Dependency

Vendored upstream [v11.0.0](https://github.com/alandtse/CommonLibSSE-NG/tree/94faaed0c60eddd8347767f2d4d29a97c93bde8c),
commit `94faaed0c60eddd8347767f2d4d29a97c93bde8c`, replacing the earlier 4.39.3 snapshot.
The historical `extern/CommonLibVR` path is retained. No local upstream code patches.
Upstream now uses GPL-3.0-or-later with Modding and Linking Exceptions; COPYING.txt,
EXCEPTIONS.md and retained MIT/HDE64 notices are included in source and the combined runtime
readme. Application code retains GPL-3.0-only. Source builds keep SE/AE enabled and VR disabled.
The library's default trampoline patch diagnostics use the upstream MinHook v1.3.4 HDE64 decoder;
the same upstream license is already included in the combined runtime readme.

Upstream changes relevant to this plugin:

- Runtime classification now recognizes minor versions 6 and later as AE.
- Address Library format 5 supports 1.7.99+ dense offset files, while retaining formats 1/2.
- Generated SKSE metadata declares Address Library v5 capability.
- PlayerCharacter's new BSSystemEvent sink shifts its later data by 8 bytes.
- The Actor runtime block and ActorState versioned accessors remain necessary.

## Review

| Area | Inspection and result | Remaining validation |
| --- | --- | --- |
| Plugin load and metadata | Address-library mode retained; actual DLL exports include the v5 and older AE flags, unchanged FaceLighting name and public API export. Startup logs candidate, game runtime and dependency pin. | SKSE startup with matching runtime dependencies. |
| Player update hook | Primary PlayerCharacter vtable resolved through upstream ID; slot 0xAD matches upstream Actor.cpp for flat runtimes. Synthetic dispatch regression covers SE, AE and both 1.7 versions. | Real update cadence and coexistence with other hooks. |
| Life state and death | AsActorState and explicit 0x99 flat death dispatch retained. Six-runtime fixtures poison obsolete/adjacent offsets. | Living, dead, dismissed and custom follower checks in-game. |
| Model and scene light | Uses upstream creation/attenuation/AddLight/RemoveLight APIs and light runtime accessors. Ownership, head refresh, invalid-transform cleanup and fade behavior retained. Flat light data and attenuation offsets checked. | Renderer allocation, character models, CS integration and actual light visibility. |
| Player/process data | GetActorRuntimeData used for currentProcess. HighProcessData.lightLevel retains the declared 0x3A8 offset. GetPlayerRuntimeData accounts for the 1.7 shift in upstream; native fixtures check all boundaries. | Freshness/meaning of the actual 1.7 light cache. |
| Input and target selection | Upstream device/control accessors and handle APIs retained. Console selected-ref uses upstream AE version-dependent ID; crosshair data has unchanged flat layout. Control-map boundary covered by native fixtures. | Keyboard, modifiers, gamepad, console and crosshair selection. |
| Follower roster and persistence | Existing ProcessLists handles and teammate APIs retained; no all-form scans, new record formats or preference resets added. | Team recruitment, waiting/dismissal, SKSE co-save load and missing plugins. |
| Dialogue and menu | MenuTopicManager speaker field remains at 0x68 upstream; framework ABI is external and unchanged by this migration. Current-speaker exclusivity and automatic dialogue policy retain regression tests. | A framework DLL that supports 1.7.x; speaker transitions and ambient decisions. |
| Public API | Fixed-width V1 table remains unchanged; session/thread/token guards and preference behavior tests pass with the new dependency. | Favorite Wheel regression with the experimental DLL. |
| Live ambient exclusion | Exact 1.6.1170 gate executes before AE-only IDs, byte validation or writes. No new offsets/signatures and no forced fallback introduced. | Continue existing 1.6.1170 in-game regression. 1.7 live exclusion is unavailable. |

Review found an undersized legacy test fixture: multi-runtime sizeof(Actor) describes a partial
C++ layout, not native object storage. ActorRuntimeTests now reserves 0x300 bytes for the native
fixture, including 1.7.99/104. This fixes test memory safety; it is not a new game runtime workaround.
No additional application-code adaptation blocker was found in the reviewed paths.

The build emits upstream macro-redefinition warnings in CombatBehaviorTreeConditionalNode and
an HDE64 potential-uninitialized warning. Those sources are retained unmodified; no build errors.
Passing fixtures demonstrates agreement with pinned upstream layouts, not independent binary
verification of every new engine field or function.

## Validation and rollout

- Full source build of the new static library and plugin, plus all 16 test executables.
- Final build and all 16 executables passed. All 2776 upstream source-snapshot files match
  the pinned commit byte-for-byte. Local/deployed DLL SHA256:
  `174CAB91FD44B0237FAEB9AC7791DFEB9C19854CFAEEB4CCA7AC6194840CBA38`.
- RuntimeCompatibilityTests exercises legacy format 1/2 fixtures and synthetic format 5 files
  for each 1.7 version, including AE ID selection. Synthetic offsets are not game addresses.
- Experimental installation/source packages have separate paths under build/experiments;
  existing 0.9.4 public ZIPs are untouched. No initial INI is bundled.
- Local deployment copies only DLL and the two bundled translations. Existing configurations
  and SKSE saves are not edited. A 0.9.4 DLL/language rollback copy exists under
  build/experiments/backup-0.9.4; the public 0.9.4 ZIP remains a complete rollback option.

First test locally on 1.6.1170: launch/menu, player and first-person lights, hotkeys, NPC selection,
followers and custom followers, dialogue suppression/restoration, brightness presets, sneak/death,
save/load, cell changes and Favorite Wheel. Test with CS disabled, then enabled where available.
Use your existing live-exclusion setup to check the old hook path still works.

After that passes, provide the candidate to 1.7.99/104 testers with matching SKSE64, Address Library
v5 and SKSE Menu Framework. Start with manual player/NPC lighting, then followers/dialogue,
save/load and fixed-compensation ambient control. Do not select live exclusion for 1.7 tests.
Collect FaceLighting.log and skse64.log with game/SKSE/address-library/framework/CS versions and
the concrete failing step. A framework that fails to load is separate from plugin lighting failure.
Only after those reports pass should the public compatibility claim and stable release change.
