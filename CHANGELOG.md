# Changelog

## 0.9.7

- Added API V2 for temporary conversation lighting, with per-actor color, brightness and placement controls.
- Added session pause/resume and automatic cleanup; temporary requests preserve saved settings.
- Retained API V1 compatibility for Favorite Wheel and existing integrations.

## 0.9.6

- Unified individual NPC/follower switches and hotkeys to prevent overlapping sources from keeping lights on.
- Fixed configuration save-path handling, including missing folders and Unicode paths.
- Dialogue lighting remains independent; existing settings and saves are retained.

## 0.9.5

- Updated CommonLibSSE-NG to v11.0.0 for Skyrim 1.7.99 / 1.7.104 compatibility; user testing on 1.7.x is pending.
- Retained SE 1.5.97 support; Skyrim 1.6.1170 and Favorite Wheel integration passed local testing.
- Live ambient exclusion remains limited to Skyrim 1.6.1170.

## 0.9.5-experimental.1 (local test candidate)

- Updated the vendored CommonLibSSE-NG snapshot to v11.0.0 for Skyrim 1.7.99 / 1.7.104.
- Added cross-runtime layout, Address Library v5 and exported SKSE metadata regression checks.
- Retained SE / AE behavior, public API V1 and the 1.6.1170-only live ambient exclusion gate.
- Gameplay validation is pending; this candidate has not replaced the public 0.9.4 release.

## 0.9.4

- Added a public API for Favorite Wheel and other SKSE plugins.
- Added an adjustable simultaneous NPC light limit (1–32, default 4), shared by followers and selected NPCs.
- Added optional environment-based dialogue lighting with independent thresholds and delay.
- Added Performance (1 s), Balanced (0.5 s) and Responsive (0.2 s) environment-check presets.
- Moved player automatic lighting settings to the Player page and shortened menu help.

Live exclusion remains limited to the verified Skyrim 1.6.1170 layout.

## 0.9.3

- Fixed runtime-dependent actor-state detection that could reject living NPCs or disable player and NPC lighting, reported on Skyrim SE 1.5.97.
- Added native-layout regression checks for SE 1.5.97 and AE 1.6.353 / 1.6.629 / 1.6.1170.
- Thanks to [jinx60](https://next.nexusmods.com/profile/jinx60) for discovering and helping diagnose the issue.

## 0.9.2

- Fixed player lighting turning off nearby NPC and follower lights.
- Made follower lighting take priority over selected NPC lighting.
- Temporarily suppress other NPC lights during dialogue, then restore them within the light budget without changing saved preferences.

## 0.9.1

- Added player and dialogue light priority protection and improved full-pool handling.
- Improved diagnostic logging.
- Removed the initial configuration INI from installation archives; existing user settings are retained.

## 0.9.0

- Added selected NPC lists, automatic follower lighting and individual follower switches.
- Added optional first-person player lighting and automatic dark-environment player lighting.
- Added smooth player light transitions, roster notifications, sneak hiding for teammates and death/model safeguards.
- Redesigned the settings menu and simplified Community Shaders integration.

The 0.9.2 and 0.9.3 release notes were backfilled when synchronizing the 0.9.4 source. The corresponding runtime packages were prepared earlier; these notes do not imply that separate historical source snapshots were recreated.
