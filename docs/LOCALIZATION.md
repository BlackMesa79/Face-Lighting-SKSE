# FaceLighting language files

FaceLighting 0.8.0 includes English (en.ini) and Simplified Chinese (zh-CN.ini). Earlier development packages without the Languages directory do not load translation files.

## Create a translation

1. Copy `SKSE/Plugins/FaceLighting/Languages/en.ini` to a new file in the same directory, such as `fr.ini`, `de.ini`, `ja.ini`, or `pt-BR.ini`.
2. Save as UTF-8 (with or without BOM). Use a language/locale code as the filename, not the language name. Codes contain ASCII letters/digits separated by hyphens, up to 31 characters; `auto` is reserved.
3. Under `[Language]`, change `Name` to the language's native display name.
4. Under `[Strings]`, translate only the text after `=`. Keep section names and keys unchanged. Each value stays on one line. Do not add quotation marks, `###` widget IDs, or `#` characters. Do not use `/` in `sectionName`, `basicEntry`, `playerEntry`, or `npcEntry`: the menu framework treats it as a submenu separator. `%` is safe as literal text. Lines starting with `;` are comments; trailing comments are not supported.
5. Restart the game. Select the new language in FaceLighting → General and save. Page text previews immediately; sidebar names change after saving and restarting.

Example of a partial French file (missing strings use English):

```ini
[Language]
Name=Français
[Strings]
sectionName=FaceLighting
basicEntry=Général
playerEntry=Éclairage du joueur
npcEntry=Éclairage des PNJ
languageLabel=Langue
enabled=Activer l’éclairage du joueur
```

Distribute only your translation, with this archive structure:

```text
SKSE/
  Plugins/
    FaceLighting/
      Languages/
        fr.ini
```

Do not include or replace FaceLighting.dll or the user's FaceLighting.ini. Install the translation alongside the main mod. A translation does not require recompilation.

## Selection and fallback

`Language=auto` in `Data/SKSE/Plugins/FaceLighting.ini` uses the Windows UI language, not Skyrim's language. Matching tries the full locale code, then the base language, then English. For example, Windows `fr-FR` matches `fr-FR.ini` first, then `fr.ini`. Chinese locales without a matching file fall back to `zh-CN.ini` when present, preserving the previous behavior.

You can select a file manually in the menu or set `Language=fr` in the settings INI. Matching is case-insensitive. Explicit selections are preserved even when their file is missing; displayed text falls back to English. Existing `auto`, `en`, and `zh-CN` settings continue to work.

Missing, empty (except `empty`), or invalid individual strings fall back to English. Malformed files and invalid UTF-8 files are skipped. Unknown keys are ignored. File size is limited to 256 KiB and each line to 4096 bytes. English has an emergency fallback in the DLL so a missing language directory does not make the menu unusable.

Files load at game startup; restart after editing or installing a translation. Check all three pages, dropdowns, CS status, save/discard notices, and sidebar labels. A translation requires a menu-framework font and glyph configuration that supports its characters. Chinese still requires the appropriate Chinese font configuration. FaceLighting does not install fonts or configure glyph ranges automatically.

# 中文说明

0.8.0 正式包提供英文 en.ini 和简体中文 zh-CN.ini。之前没有 Languages 文件夹的开发测试包不能加载翻译文件。

复制 `en.ini`，按语言代码命名（如 `fr.ini`、`ja.ini`、`pt-BR.ini`），以 UTF-8 保存。在 `[Language]` 下填写该语言的 `Name`；在 `[Strings]` 下只翻译等号右侧，保持键名不变。每条文字占一行，不添加引号或 `###` ID，不使用 `#`。模组入口及三个页面名称不能包含 `/`，否则会被菜单框架当成子菜单。

把文件放在 `Data/SKSE/Plugins/FaceLighting/Languages/`，重启游戏后在“基本”页选择并保存。页面文字支持预览；侧栏名称保存后重启更新。缺少翻译项回退英文，缺少文件不会丢失手动选择。自动模式按 Windows 界面语言匹配，先完整地区代码，再基础语言；中文另保留简体中文回退，最后回退英文。

制作翻译压缩包时仅包含自己的语言文件及上述目录结构，不附带 DLL 或用户配置 INI。无需重新编译。请在菜单框架中配置支持目标语言的字体/字形范围；本模组不会自动安装字体。修改语言文件后需要重启游戏。


