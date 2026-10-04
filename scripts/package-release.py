"""Build the minimal public installation archive. Run from any directory."""
from pathlib import Path
import argparse
import hashlib
import re
import subprocess
import zipfile

ROOT = Path(__file__).resolve().parents[1]

def package_source(version, destination):
    # Read current files, including new implementation files not yet committed.
    paths = subprocess.check_output(
        ['git', 'ls-files', '--cached', '--others', '--exclude-standard', '-z'],
        cwd=ROOT).decode('utf-8').split('\0')
    directories = {'src', 'include', 'extern', 'tests', 'scripts', 'docs', 'languages'}
    root_files = {'.gitignore', 'xmake.lua', 'FaceLighting.ini', 'LICENSE',
                  'LICENSE.txt', 'README.md', 'README.txt', 'THIRD_PARTY_NOTICES.md',
                  'HANDOFF.md', 'CHANGELOG.md'}
    selected = sorted({p for p in paths if p and (p in root_files or
        p.split('/')[0] in directories or p.startswith(f'release-materials/{version}/'))
        and (ROOT / p).is_file()})
    prefix = f'FaceLighting-SKSE-{version}-source/'
    archive = destination / f'FaceLighting-SKSE-{version}-source.zip'
    with zipfile.ZipFile(archive, 'w', zipfile.ZIP_DEFLATED, compresslevel=9) as z:
        for name in selected:
            z.write(ROOT / name, prefix + name)
    with zipfile.ZipFile(archive) as z:
        assert z.testzip() is None
        for name in selected:
            assert z.read(prefix + name) == (ROOT / name).read_bytes()
        for required in ('xmake.lua', 'src/PublicAPI.cpp', 'include/FaceLightingAPI.h',
                         'LICENSE.txt', 'extern/CommonLibVR/LICENSE'):
            assert prefix + required in z.namelist()
    print(f'{archive}: {len(selected)} source files, verified')

def package(version, dll):
    destination = ROOT / 'build/releases' / (version.removesuffix('.0'))
    destination.mkdir(parents=True, exist_ok=True)
    intro = f'''Face Lighting SKSE {version}
Copyright (C) 2026 BlackMesa79
SPDX-License-Identifier: GPL-3.0-only

Install with your mod manager; launch through matching SKSE64.
Requires matching Address Library; SKSE Menu Framework provides the menu.
Community Shaders is optional. Preserve existing INI and SKSE co-saves on upgrade.
No initial FaceLighting.ini is bundled. Missing settings use built-in defaults;
saving settings in the menu creates the INI. Existing configurations are retained.
L toggles player lighting; Shift+L adds/toggles the NPC under the crosshair.
Settings save to INI; NPC lists and follower preferences require a game save.
Live-exclusion ambient control requires the verified Skyrim 1.6.1170 layout.
No VR or Skyrim 1.7.x support. See the mod page for features and compatibility.

安装后通过 SKSE 启动，需要匹配的 Address Library，菜单需要 SKSE Menu Framework。
升级保留原 INI 与 .skse 存档。L 切换玩家面光，Shift+L 添加/切换准星 NPC。
安装包不附带初始配置 INI；缺失时使用内置默认值，在菜单保存设置后生成。
名单与随从开关需保存游戏。实时排除自动面光仅支持已核验的 1.6.1170 布局。

This program is free software under GNU GPL version 3, without any warranty.
The complete GPL text and third-party license notices follow below.
Third-party components retain their own licenses and are not relicensed.
Public source: https://github.com/BlackMesa79/Face-Lighting-SKSE
Use the corresponding source archive supplied separately with this release;
the repository main branch may contain later development.

本程序按 GPL 第三版发布，不提供担保。完整协议和第三方许可合并收录于下文。
对应源码包单独提供；安装包仅保留运行文件和本 readme.txt。
'''
    sections = [('GNU GENERAL PUBLIC LICENSE v3', 'LICENSE.txt'),
                ('THIRD-PARTY ATTRIBUTIONS (paths refer to source checkout)', 'THIRD_PARTY_NOTICES.md'),
                ('CommonLibSSE NG / CommonLibVR — MIT', 'extern/CommonLibVR/LICENSE'),
                ('SKSE Menu Framework API — LGPL 2.1', 'extern/SKSEMenuFrameworkAPI/LICENSE'),
                ('HDE64 / MinHook — upstream notices', 'extern/hde64/LICENSE.txt')]
    readme = intro
    for title, path in sections:
        readme += '\n\n' + '=' * 72 + '\n' + title + '\n' + '=' * 72 + '\n\n'
        readme += (ROOT / path).read_text(encoding='utf-8')
    files = {'readme.txt': readme.encode('utf-8'),
             'SKSE/Plugins/FaceLighting.dll': dll.read_bytes()}
    for path in sorted((ROOT / 'languages').glob('*.ini')):
        files['SKSE/Plugins/FaceLighting/Languages/' + path.name] = path.read_bytes()
    archive = destination / f'FaceLighting-SKSE-{version}.zip'
    with zipfile.ZipFile(archive, 'w', zipfile.ZIP_DEFLATED, compresslevel=9) as z:
        for name, contents in files.items():
            z.writestr(name, contents)
    with zipfile.ZipFile(archive) as z:
        assert z.testzip() is None
        assert 'SKSE/Plugins/FaceLighting.ini' not in z.namelist()
        assert {n for n in z.namelist() if not n.startswith('SKSE/')} == {'readme.txt'}
        for name, contents in files.items():
            assert z.read(name) == contents
    package_source(version, destination)
    (destination / 'SHA256SUMS.txt').write_text(''.join(
        hashlib.sha256(p.read_bytes()).hexdigest() + '  ' + p.name + '\n'
        for p in sorted(destination.glob('*.zip'))), encoding='ascii')
    print(f'{archive}: {len(files)} files, verified')

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--dll', type=Path, default=ROOT / 'build/windows/x64/release/FaceLighting.dll')
    args = parser.parse_args()
    version = re.search(r'set_version\("([\d.]+)"\)', (ROOT / 'xmake.lua').read_text(encoding='utf-8')).group(1)
    package(version, args.dll)
