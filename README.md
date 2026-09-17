# FaceLighting / 面部光照

Adjustable face lighting for Skyrim Special Edition and Anniversary Edition,
with SKSE Menu Framework settings and optional Community Shaders integration.
No ESP or Papyrus scripts are required.

## Development status

This repository contains the **current development source**, including work
following the published 0.8.2 release. The DLL version remains 0.8.2 during
this development cycle; it does not imply that the source matches the
published 0.8.2 archive.

- Player and dialogue NPC lighting have passed user testing.
- Unified NPC light management has passed user testing.
- Selected NPC lighting and the redesigned menu are implemented and awaiting
  in-game validation.
- Automatic follower and nearby-NPC lighting are planned, not implemented.
- Skyrim **1.7.x is not supported by this build**. Compatibility work and
  external testing are planned for a future release.

## Features

- Independent player and dialogue NPC lighting, with radius, intensity,
  position and 2000–10000 K color-temperature controls.
- Optional head-bone orientation tracking and dialogue fades.
- Keyboard shortcuts (including function keys), modifier keys and controller
  bindings for player lighting.
- Optional player-light activation on dialogue and hiding while sneaking.
- Community Shaders inverse-square falloff, linear lighting and automatic or
  manual range, with automatic detection and a manual override.
- English and Simplified Chinese language files; automatic Windows UI language
  detection and support for community translations.
- **In development:** selected NPC list with crosshair/console selection,
  individual enable/remove controls and independent shared lighting settings.
  The list is stored per save in the SKSE co-save, with a 32-NPC limit.

These are native point lights: they can illuminate nearby objects and affect
sneak detection. Player sneak hiding does not hide NPC lights.

## Requirements

- Windows x64, Skyrim SE/AE with matching SKSE and Address Library.
- SKSE Menu Framework for the in-game settings menu.
- Community Shaders is optional. ENB integration has not been validated.
- VR is not supported. Skyrim 1.5.97 has passed user testing; do not infer
  compatibility with every runtime from the SE/AE build options.

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
    'SelectedNPCRecordTests')
foreach ($test in $tests) {
    & "./build/windows/x64/releasedbg/$test.exe"
    if ($LASTEXITCODE -ne 0) { throw "$test failed" }
}
```

These tests cover configuration, light placement, CS calculations, localization,
dialogue policy, NPC request management and serialized-record validation.
They do not replace in-game rendering and SKSE save/load testing.

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

Copyright (C) 2026 BlackMesa79. FaceLighting is released under [GPL-3.0](LICENSE.txt).
Modification and redistribution are permitted under GPL-3.0. The software is provided without warranty; see [README.txt](README.txt) for the copyright and license notice.
Vendored dependencies retain their respective licenses; see
[Third-party notices](THIRD_PARTY_NOTICES.md).

## 中文说明

这是面部光照模组的当前开发源码，包含正式 0.8.2 发布后的开发内容。
统一 NPC 光源管理已通过用户测试；指定 NPC 面光和新版菜单等待游戏内验证。
随从自动面光、范围 NPC 面光和 Skyrim 1.7.x 支持尚未完成。

构建需要 Windows x64、Visual Studio 2022 C++ 工具链和 Xmake 3.0 以上。
构建命令见上文。默认不复制文件到游戏目录，可通过本地 `deploy_dir` 配置开启自动部署。

指定 NPC 名单支持准星／控制台添加，按存档保存，最多 32 名；名单操作需保存游戏
才能持久化，菜单中的保存按钮只保存光照参数。翻译文件位于 `languages/`，
正式附带英文和简体中文；测试目录中的其他语言样例不属于发布语言包。