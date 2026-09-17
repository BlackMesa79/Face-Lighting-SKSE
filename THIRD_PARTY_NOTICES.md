# Third-party notices

Original Face Lighting SKSE code is distributed under GPL-3.0-only. Third-party files
retain their own copyright and license notices; they are not relicensed by
the root license.

## CommonLibSSE NG (alandtse fork)

- Source: https://github.com/alandtse/CommonLibSSE-NG
- Vendored directory: `extern/CommonLibVR` (historical directory name).
- Version metadata: **4.39.3** in CMakeLists.txt and vcpkg.json.
- License: [MIT](extern/CommonLibVR/LICENSE), copyright Ryan-rsm-McKenzie and
  other notices retained in the upstream files.
- This is a vendored source snapshot, not a submodule. Its original Git commit
  was not retained. The exact source used by this project is included here.
- SE and AE are enabled; VR is disabled. This dependency has not been upgraded
  for Skyrim 1.7.x.

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
