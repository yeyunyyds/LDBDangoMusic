"""Package built Windows artifacts, a ready-to-copy demo layout, and matching source."""
import hashlib
import json
from pathlib import Path
import zipfile
import struct

ROOT = Path(__file__).resolve().parents[1]
VERSION = '0.7.2'
files = {}
for folder in ('src', 'tests', 'scripts', 'vendor', 'docs', 'presets'):
    for path in (ROOT / folder).rglob('*'):
        if path.is_file() and '__pycache__' not in path.parts:
            files['source/' + path.relative_to(ROOT).as_posix()] = path
for name in ('README.md', 'NOTICE.md', 'LICENSE', 'config.example.json', 'config.demo.json', 'config.camera-demo.json', 'config.world-demo.json', '.gitignore'):
    files['source/' + name] = ROOT / name
for name in ('L4D2DangoMusic.dll', 'l4n_smoke.exe', 'core_test.exe', 'interface_inventory_test.exe'):
    path = ROOT / 'build' / name
    blob=path.read_bytes()
    if blob[:2] != b'MZ':
        raise RuntimeError(f'Not a Windows artifact: {name}')
    pe=struct.unpack_from('<I',blob,0x3c)[0]
    if struct.unpack_from('<H',blob,pe+4)[0] != 0x14c:
        raise RuntimeError(f'Pure L4N artifacts must be x86: {name}')
    files['source/build/' + name] = path
files['bin/neko/plugins/L4D2DangoMusic.dll'] = ROOT / 'build/L4D2DangoMusic.dll'
files['bin/neko/plugins/L4D2DangoMusic/config.json'] = ROOT / 'config.example.json'
for preset in (ROOT / 'presets').glob('*.json'):
    files['presets/' + preset.name] = preset
files['README.md'] = ROOT / 'README.md'
files['docs/0.3.0-visuals.md'] = ROOT / 'docs/0.3.0-visuals.md'
files['config.demo.json'] = ROOT / 'config.demo.json'
files['config.camera-demo.json'] = ROOT / 'config.camera-demo.json'
files['config.world-demo.json'] = ROOT / 'config.world-demo.json'
files['docs/l4n-world-lyrics.md'] = ROOT / 'docs/l4n-world-lyrics.md'
files['docs/0.4.2-source-build.md'] = ROOT / 'docs/0.4.2-source-build.md'
files['docs/0.4.3-callback-chain.md'] = ROOT / 'docs/0.4.3-callback-chain.md'
files['docs/peak-style.md'] = ROOT / 'docs/peak-style.md'
files['docs/0.6.0-menu-and-ghosting.md'] = ROOT / 'docs/0.6.0-menu-and-ghosting.md'
files['docs/0.6.1-stale-lyrics.md'] = ROOT / 'docs/0.6.1-stale-lyrics.md'
files['docs/0.7.0-layout-editor.md'] = ROOT / 'docs/0.7.0-layout-editor.md'
files['docs/0.7.1-player-session.md'] = ROOT / 'docs/0.7.1-player-session.md'
files['docs/0.7.2-motion-and-placement.md'] = ROOT / 'docs/0.7.2-motion-and-placement.md'
files['docs/camera-diagnostics.md'] = ROOT / 'docs/camera-diagnostics.md'
files['LICENSE'] = ROOT / 'LICENSE'
files['NOTICE.md'] = ROOT / 'NOTICE.md'
if json.loads((ROOT / 'config.example.json').read_text())['offsetMs'] != 500:
    raise RuntimeError('Visual package must include 500ms compensation')
output = ROOT / 'releases' / f'L4D2DangoMusic-{VERSION}-l4n.zip'
output.parent.mkdir(exist_ok=True)
manifest = {name: hashlib.sha256(path.read_bytes()).hexdigest() for name, path in files.items()}
with zipfile.ZipFile(output, 'w', zipfile.ZIP_DEFLATED) as archive:
    for name, path in sorted(files.items()):
        archive.write(path, name)
    archive.writestr('SHA256-MANIFEST.json', json.dumps(manifest, indent=2) + '\n')
with zipfile.ZipFile(output) as archive:
    for name, expected in manifest.items():
        if hashlib.sha256(archive.read(name)).hexdigest() != expected:
            raise RuntimeError(f'Archive mismatch: {name}')
output.with_suffix('.zip.sha256').write_text(hashlib.sha256(output.read_bytes()).hexdigest() + '  ' + output.name + '\n')
print(f'Packaged and verified {len(files)} files: {output.name}')
