# Face Lighting SKSE

Public mod name: **Face Lighting SKSE**. The plugin filename ("FaceLighting.dll"),
configuration paths and SKSE Menu Framework registration name remain unchanged.

Adjustable face lighting for Skyrim Special Edition and Anniversary Edition,
with SKSE Menu Framework settings and optional Community Shaders integration.
No ESP or Papyrus scripts are required.

## Version 0.9

0.9 adds selected NPC lists, automatic follower lighting, optional first-person player lighting, automatic lighting in dark environments, and a redesigned configuration menu.

Version 0.9.4 adds a public plugin API, a configurable simultaneous NPC light limit, optional environment-based dialogue lighting, and three shared environment-check frequency presets. Player automatic lighting controls now reside on the Player page with shorter menu help. Live exclusion remains restricted to the verified 1.6.1170 layout.

See [Changelog](CHANGELOG.md) for this release and the backfilled 0.9.2 / 0.9.3 records.

## Features

- Separate settings for player, dialogue NPC, selected NPC and follower lighting: intensity, position, radius/range, head rotation and 2000–10000 K color temperature.
- Aim at an NPC and press **Shift+L** to add and enable its light; press again to toggle. Crosshair and console targets can also be added from the menu. Up to 32 selected NPCs per save.
- Automatically light recruited player teammates, with individual follower switches. Loaded followers share a configurable simultaneous light budget with selected NPCs (1-32, default 4). Custom followers need the player-teammate flag; use the selected list otherwise. Ashe – Crystal Heart was recognized in user testing; this is not a guarantee for all custom followers.
- A single light per NPC, with dialogue first, followers next and selected NPCs last. The NPC menu provides a shared simultaneous light limit of 1-32 (default 4), separate from player lighting and the 32-entry selected roster. During dialogue, only the player and current speaker lights remain.
- Optional first-person player light; smooth player transitions default to 0.2 seconds.
- Optional dark-environment player control: fixed compensation or live exclusion of this mod's player/NPC lights. Live exclusion requires the verified **Skyrim 1.6.1170 code layout**, saving settings and restarting. It is not available on 1.5.97. Unknown or stale readings pause decisions; there is no automatic fallback to compensation.
- Optional dialogue ambient control has independent mode, thresholds and delay (disabled by default), using the player-position lighting sample even when player automatic control is off. It keeps bright conversations off, fades on after sustained darkness, and prevents follower/selected sources from bypassing the decision. See [usage and sampling limits](docs/dialogue-ambient-control.md). This is not a direct NPC face/sunlight measurement.
- Shared environment-check frequency dropdown: Performance (1 second, default), Balanced (0.5 seconds), Responsive (0.2 seconds). This throttles cached brightness reads and threshold decisions while light fades and safety checks keep their frame cadence. It does not force engine light-cache updates; live exclusion capture still follows the engine queries.
- Dark control uses measured lighting, not time of day. Default thresholds: below 30 on, above 50 off, sustained for 2 seconds; transitions and their settling period temporarily suspend decisions.
- Player **L** hotkey, optional keyboard modifiers and independent controller bindings.
- Sneak hiding immediately suppresses player and teammate lights, including selected/dialogue sources on teammates, ahead of automatic control. Other NPCs remain independent.
- Roster notifications; English and Simplified Chinese; live preview, save and discard.
- Optional Community Shaders integration with inverse-square falloff and linear-light handling.

These are shadowless point lights, not skin-only lighting. They can illuminate nearby objects and affect sneak detection. Ambient exclusion changes this mod's automatic decision input, not the game's original light or stealth values.

## Requirements and validation

Windows x64, matching SKSE64 and Address Library. SKSE Menu Framework is needed for the in-game menu. Community Shaders is optional; ENB is not validated. VR and Skyrim 1.7.x are not supported.

Core lighting was previously tested on 1.5.97; the 0.9 feature testing was performed on 1.6.1170. Do not infer that every new feature has been retested on every SE/AE runtime. Live exclusion is restricted to the verified 1.6.1170 layout and may be unavailable if another plugin changes that code.

User testing covered selected NPCs across characters and saves, followers, dark-environment control, and sneak/death safeguards. Player decapitation could not be reproduced and remains unverified. Missing head nodes or invalid actor/model state cause light cleanup; these guards cannot guarantee that external mods or engine death sequences never crash.

## Installation and upgrade

Version 0.9.3 fixes runtime-dependent actor life-state detection that could reject living NPCs or disable face lights. Thanks to [jinx60](https://next.nexusmods.com/profile/jinx60) for discovering and helping diagnose the issue. The fix uses the SE/AE runtime accessor and has native-layout regression coverage; full gameplay validation across both runtimes remains separate.

Install the archive with your mod manager and launch through SKSE. No ESP or Papyrus scripts. Preserve your existing INI; if using Mod Organizer, check its overwrite folder for the effective settings. New options use defaults when absent.

Starting with 0.9.1, installation archives do not include an initial `FaceLighting.ini`. Missing settings use built-in defaults; saving settings in the menu creates the file. The repository's INI is a reference only.

Version 0.9.2 revises light priority protection: outside dialogue, player lighting coexists with up to four secondary NPC lights, with followers ahead of selected NPCs. Player proximity no longer forces other lights off. During dialogue, only player and current speaker lighting are allowed; other NPC lights resume within the budget afterwards without changing saved preferences. Existing user switches and visibility safeguards still apply. This reduces competition from this mod, but cannot guarantee visibility against engine or external lighting limits.

The release defaults leave the player light initially off, first-person lighting off, automatic darkness control off and diagnostics off. Dialogue activation, NPC lighting groups and sneak hiding are enabled. To test automatic control, enable the player master switch and choose a mode under Player face light; live exclusion needs a restart after saving settings. Shared environment-check frequency remains under General.

Settings are stored in INI. Selected NPC lists and per-follower preferences are stored in the SKSE co-save when saving the game; preserve the matching `.skse` file with your saves.

## Build

Use Visual Studio 2022 with Desktop development with C++, a Windows SDK,
C++23 support, and Xmake 3.0 or later.

```powershell
git clone https://github.com/BlackMesa79/Face-Lighting-SKSE.git
cd Face-Lighting-SKSE
xmake f -p windows -a x64 -m releasedbg --toolchain=msvc --vs=2022
xmake build FaceLighting
```

The CommonLib snapshot and menu API header are vendored in `extern`; no
submodule checkout is needed for this SE/AE build. Xmake downloads its package
dependencies on the first build. The DLL is produced under
`build/windows/x64/releasedbg`; the local install/package directory is
`build/package/FaceLighting`. Build outputs are excluded from Git.

Automatic game deployment is **disabled by default**. To enable it locally:

```powershell
xmake f -p windows -a x64 -m releasedbg --toolchain=msvc --vs=2022 --deploy_dir="C:/YourModFolder/SKSE/Plugins"
xmake build FaceLighting
```

This copies only the DLL and bundled language files, preserving the installed
FaceLighting.ini and custom translation files. To disable it, configure
`--deploy_dir=""`. The option is stored in the ignored local Xmake configuration.

## Tests

```powershell
xmake build -a
$tests = @('SettingsTests', 'LightPlacementTests', 'CSLightingTests',
    'LocalizationTests', 'PlayerDialogueTests', 'NpcLightManagerTests',
    'SelectedNPCRecordTests', 'ActorRuntimeTests', 'FollowerRosterTests',
    'AmbientPolicyTests', 'LightCallScanTests', 'PlayerTransitionTests', 'ExclusionTotalsTests',
    'PublicAPITests', 'DialogueAmbientTests')
foreach ($test in $tests) {
    & "./build/windows/x64/releasedbg/$test.exe"
    if ($LASTEXITCODE -ne 0) { throw "$test failed" }
}
```

These tests cover configuration, light placement, CS calculations, localization,
dialogue policy, NPC request management and serialized-record validation.
They do not replace in-game rendering and SKSE save/load testing.

## Plugin integration

Version 0.9.4 exposes the versioned `FaceLighting_GetAPI` DLL export.
[FaceLightingAPI.h](include/FaceLightingAPI.h) provides player, target NPC and follower
controls, caller-owned follower pagination and preference/runtime status queries.
Discover the optional module with GetModuleHandle/GetProcAddress and invoke callbacks
on the game thread. See [API contract and examples](docs/public-api.md). Favorite Wheel
integration was tested successfully by the author; its client is distributed separately.

## Configuration and translations

Open **FaceLighting → General / Player face light / NPC face light** in SKSE
Menu Framework. Lighting changes support preview, save and discard.

Selected NPC list edits apply immediately and persist when **saving the game**;
"Save all settings" saves lighting configuration, not the game. Keep the
matching `.skse` co-save when copying saves.

- [Translation guide](docs/LOCALIZATION.md)
- [Selected NPC usage and test checklist](docs/selected-npc-lighting.md)
- [Community Shaders compatibility](docs/CS-compatibility.md)
- [Development history (Chinese)](docs/DEVELOPMENT_HISTORY.md)

## License

Copyright (C) 2026 BlackMesa79. Face Lighting SKSE is released under [GPL-3.0](LICENSE.txt).
Modification and redistribution are permitted under GPL-3.0. The software is provided without warranty; see [README.txt](README.txt) for the copyright and license notice.
Vendored dependencies retain their respective licenses; see
[Third-party notices](THIRD_PARTY_NOTICES.md).

## 中文说明

0.9 正式发布内容包括：指定 NPC 快捷键名单、自动随从面光与逐人开关、可选第一人称面光、暗环境自动控制、0.2 秒玩家过渡、新版配置菜单及名单通知。潜行隐藏优先于自动开灯，并同步隐藏队友所有来源的面光。玩家和 NPC 共用死亡清理保护；玩家斩首场景尚未复现验证。

实时排除面光的自动模式仅支持已核验的 Skyrim 1.6.1170 代码布局，首次启用需保存设置并重启。固定补偿旧方案保留。发布包默认关闭自动控制和诊断，不包含作者个人测试设置。范围内所有 NPC 自动照明、VR 与 Skyrim 1.7.x 支持不在此版本中。

升级保留现有 INI 和随存档的 `.skse` 文件。名单操作需保存游戏；菜单“保存所有设置”只保存参数。详情见发布包内 `docs/0.9-user-guide.md`。


CS integration uses a single on/off switch and requires a loaded CommunityShaders.dll; no version whitelist is used. Legacy CSMode=0 (automatic) and 1 (manual) migrate to enabled (1); 2 stays disabled. DLL and ISL version information remains visible. Direct CS testing covers Jiaye build 0516; other builds have user reports, not individual certification. Release menus omit diagnostic checkboxes, sample dumps, MARK buttons and observation-only ambient mode. INI diagnostic keys remain available for requested troubleshooting; ordinary automatic control does not emit per-second diagnostic dumps.

CS 适配改为开关，不再按版本限制；旧自动和手动开启迁移为开启，旧关闭保持关闭。界面保留 DLL/ISL 版本信息。正式菜单移除调试数据、记录和标记按钮，仅在需要排查时手动通过 INI 启用诊断。关闭诊断不影响实时排除自动开灯。


## Public archive packaging

Run `python scripts/package-release.py` after building the release DLL. The public installation ZIP contains only runtime files under SKSE and one root `readme.txt`. Installation notes, full GPL text and third-party licenses are consolidated there. Do not zip the general build/package staging directory for public distribution: it may contain development documentation. Source archives, changelogs, covers and publishing materials are supplied separately.

发布约定：运行必需文件之外，安装压缩包只附带一个 readme.txt，合并使用说明、GPL 全文和第三方许可；不要另附 docs、licenses、更新日志或宣传素材。正式打包使用 scripts/package-release.py，不直接压缩可能含历史文件的 build/package 暂存目录。
