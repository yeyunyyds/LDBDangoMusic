"""Build a minimal install ZIP: bin/ and README.md only."""
import argparse
import hashlib
import json
import struct
import subprocess
from pathlib import Path
import zipfile

ROOT = Path(__file__).resolve().parents[1]
VERSION = '0.9.9'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--variant', choices=['native-hud', 'netease-native', 'steam-sync', 'combined', 'auto-listening', 'visual-preview', 'menu-preview', 'folia-stage'], help='Name a preview package separately from stable installs')
    parser.add_argument('--output-dir', type=Path, default=ROOT / 'build/packages')
    args = parser.parse_args()
    dll = ROOT / 'build/L4D2DangoMusic.dll'
    blob = dll.read_bytes()
    if len(blob) < 64 or blob[:2] != b'MZ':
        raise RuntimeError('Build the Windows DLL before packaging')
    diagnostic_blob = b'DANGO_DIAGNOSTICS_ENABLED' in blob
    if diagnostic_blob:
        raise RuntimeError('Diagnostics are archived and cannot be packaged on main')
    if not diagnostic_blob and b'DANGO_PRODUCTION_NO_DIAGNOSTICS' not in blob:
        raise RuntimeError('Missing production build marker')
    pe = struct.unpack_from('<I', blob, 0x3c)[0]
    if pe + 6 > len(blob) or blob[pe:pe+4] != b'PE\0\0' or struct.unpack_from('<H', blob, pe+4)[0] != 0x14c:
        raise RuntimeError('The install DLL must be Windows x86')
    commit = subprocess.check_output(['git', 'rev-parse', '--verify', 'HEAD^{commit}'], cwd=ROOT, text=True).strip()
    source_url = f'https://github.com/yeyunyyds/LDBDangoMusic/tree/{commit}'
    template = 'README-folia-stage.md' if args.variant == 'folia-stage' else 'README-menu-preview.md' if args.variant == 'menu-preview' else 'README-visual-preview.md' if args.variant == 'visual-preview' else 'README-combined.md'
    readme = (ROOT / 'packaging' / template).read_text(encoding='utf-8').replace('@VERSION@', VERSION).replace('@SOURCE_URL@', source_url)
    disclaimer = (ROOT / 'DISCLAIMER.md').read_text(encoding='utf-8')
    if '@DISCLAIMER@' in readme:
        readme = readme.replace('@DISCLAIMER@', disclaimer)
    else:
        readme += '\n\n' + disclaimer
    notices = [('项目及参考代码声明', 'NOTICE.md'), ('GPL-3.0 许可全文', 'LICENSE'), ('nlohmann/json MIT 许可', 'vendor/JSON-LICENSE.MIT'), ('Bridge 原生菜单 MIT 许可', 'vendor/BRIDGE-LICENSE.MIT'), ('Dear ImGui MIT 许可', 'vendor/imgui/LICENSE.txt'), ('Desktop Lyric MIT 许可', 'vendor/DESKTOP-LYRIC-LICENSE.MIT'), ('网易云 elog 参考代码 MIT 许可', 'vendor/NETEASE-ELOG-LICENSE.MIT')]
    for title, file in notices:
        readme += f'\n\n## {title}\n\n' + (ROOT / file).read_text(encoding='utf-8')
    config = (ROOT / 'config.example.json').read_bytes()
    if args.variant in ('netease-native', 'folia-stage'):
        defaults = json.loads(config)
        defaults['musicSource'] = 8 if args.variant == 'folia-stage' else 3
        defaults['demo'] = False
        config = (json.dumps(defaults, ensure_ascii=False, indent=2) + '\n').encode('utf-8')
    files = {
        'bin/neko/plugins/L4D2DangoMusic.dll': blob,
        'bin/neko/plugins/L4D2DangoMusic/config.json': config,
        'README.md': readme.encode('utf-8'),
    }
    args.output_dir.mkdir(parents=True, exist_ok=True)
    variant = args.variant or 'l4n'
    output = args.output_dir / f'L4D2DangoMusic-{VERSION}-{variant}.zip'
    with zipfile.ZipFile(output, 'w', zipfile.ZIP_DEFLATED) as archive:
        for name, data in sorted(files.items()):
            archive.writestr(name, data)
    with zipfile.ZipFile(output) as archive:
        if set(archive.namelist()) != set(files):
            raise RuntimeError('Install ZIP contains unexpected files')
        for name, data in files.items():
            if archive.read(name) != data:
                raise RuntimeError(f'Archive mismatch: {name}')
    digest = hashlib.sha256(output.read_bytes()).hexdigest()
    output.with_suffix('.zip.sha256').write_text(digest + '  ' + output.name + '\n', encoding='ascii')
    print(f'Packaged and verified {len(files)} files: {output}')
    print(f'SHA256: {digest}')


if __name__ == '__main__':
    main()
