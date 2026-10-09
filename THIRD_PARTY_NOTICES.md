# Third-party notices

Original Face Lighting SKSE code is distributed under GPL-3.0-only. Third-party files
retain their own copyright and license notices; they are not relicensed by
the root license.

## CommonLibSSE NG (alandtse fork)

- Source: https://github.com/alandtse/CommonLibSSE-NG
- Vendored directory: `extern/CommonLibVR` (historical directory name).
- Pinned release: **v11.0.0**, commit `94faaed0c60eddd8347767f2d4d29a97c93bde8c`.
- Current upstream license: [GPL-3.0-or-later](extern/CommonLibVR/COPYING.txt)
  with the [Modding Exception and GPL-3.0 Linking Exception](extern/CommonLibVR/EXCEPTIONS.md).
  The original [MIT notice](extern/CommonLibVR/licenses/LICENSE-MIT.txt) and
  [upstream HDE64 notice](extern/CommonLibVR/licenses/LICENSE-hde64.txt) are retained.
- Face Lighting's original code remains GPL-3.0-only; the upstream component
  retains its own license and additional permissions.
- This is a vendored source snapshot, not a submodule. The upstream commit is
  recorded above; the exact source is included here without local library patches.
- SE and AE are enabled; VR is disabled. The experimental build targets
  Skyrim 1.7.99 and 1.7.104 using upstream runtime accessors and Address Library v5.
  Gameplay validation remains separate from source/build compatibility.

## SKSE Menu Framework API

- Source: https://github.com/Thiago099/SKSE-Menu-Framework
- Included API: `extern/SKSEMenuFrameworkAPI/SKSEMenuFramework.h`.
- License text supplied with this copy:
  [GNU Lesser General Public License 2.1](extern/SKSEMenuFrameworkAPI/LICENSE).
- The API header is included as source; the framework DLL is a separate runtime
  dependency and is not distributed in this repository.

## Build-time packages

Xmake resolves DirectXMath, DirectXTK and spdlog from the versions declared in
`extern/CommonLibVR/xmake.lua`. Their upstream licenses apply:

- https://github.com/microsoft/DirectXMath (MIT)
- https://github.com/microsoft/DirectXTK (MIT)
- https://github.com/gabime/spdlog (MIT and bundled component notices)

## Color temperature

The color-temperature approximation is based on Tanner Helland's published
algorithm: https://tannerhelland.com/2012/09/18/convert-temperature-rgb-algorithm-code.html
Face Lighting SKSE normalizes the result around 6500 K and applies its own lighting
pipeline handling. See `include/ColorTemperature.h`.

## Menu design references

The layout was informed by Cinematic Idle Camera and Cinematic Conversation
Camera. Their UI templates, artwork and source code were not copied into the
Face Lighting SKSE menu implementation. Reference links are recorded in
[the selected NPC documentation](docs/selected-npc-lighting.md).

## HDE64 instruction decoder

The diagnostic call-site validator includes HDE64 from MinHook v1.3.4:
https://github.com/TsudaKageyu/minhook/tree/v1.3.4/src/hde
Copyright (c) 2008-2009 Vyacheslav Patkov. The included files are unmodified.
The BSD-style upstream license and bundled notices are reproduced in
`licenses/hde64/LICENSE.txt` in the package and `extern/hde64/LICENSE.txt` in source.
Only the instruction decoder is compiled; MinHook detour installation is not used.
