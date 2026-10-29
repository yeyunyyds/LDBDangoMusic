"""Build a minimal install ZIP: bin/ and README.md only."""
import argparse
import hashlib
import struct
import subprocess
from pathlib import Path
import zipfile

ROOT = Path(__file__).resolve().parents[1]
VERSION = '0.9.1'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output-dir', type=Path, default=ROOT / 'build/packages')
    args = parser.parse_args()
    dll = ROOT / 'build/L4D2DangoMusic.dll'
    blob = dll.read_bytes()
    if len(blob) < 64 or blob[:2] != b'MZ':
        raise RuntimeError('Build the Windows DLL before packaging')
    pe = struct.unpack_from('<I', blob, 0x3c)[0]
    if pe + 6 > len(blob) or blob[pe:pe+4] != b'PE\0\0' or struct.unpack_from('<H', blob, pe+4)[0] != 0x14c:
        raise RuntimeError('The install DLL must be Windows x86')
    commit = subprocess.check_output(['git', 'rev-parse', '--verify', 'HEAD^{commit}'], cwd=ROOT, text=True).strip()
    source_url = f'https://github.com/yeyunyyds/LDBDangoMusic/tree/{commit}'
    readme = (ROOT / 'packaging/README.md').read_text(encoding='utf-8').replace('@VERSION@', VERSION).replace('@SOURCE_URL@', source_url)
    for title, file in (('项目及参考代码声明', 'NOTICE.md'), ('GPL-3.0 许可全文', 'LICENSE'), ('nlohmann/json MIT 许可', 'vendor/JSON-LICENSE.MIT')):
        readme += f'\n\n## {title}\n\n' + (ROOT / file).read_text(encoding='utf-8')
    files = {
        'bin/neko/plugins/L4D2DangoMusic.dll': blob,
        'bin/neko/plugins/L4D2DangoMusic/config.json': (ROOT / 'config.example.json').read_bytes(),
        'README.md': readme.encode('utf-8'),
    }
    args.output_dir.mkdir(parents=True, exist_ok=True)
    output = args.output_dir / f'L4D2DangoMusic-{VERSION}-l4n.zip'
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
