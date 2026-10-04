# Changelog

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
