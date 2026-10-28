"""Package built Windows artifacts, a ready-to-copy demo layout, and matching source."""
import hashlib
import json
from pathlib import Path
import zipfile

ROOT = Path(__file__).resolve().parents[1]
VERSION = '0.2.0'
files = {}
for folder in ('src', 'tests', 'scripts', 'vendor', 'docs'):
    for path in (ROOT / folder).rglob('*'):
        if path.is_file() and '__pycache__' not in path.parts:
            files['source/' + path.relative_to(ROOT).as_posix()] = path
for name in ('README.md', 'NOTICE.md', 'LICENSE', 'config.example.json', '.gitignore'):
    files['source/' + name] = ROOT / name
for name in ('LDBDangoMusic.dll', 'host_smoke.exe', 'core_test.exe'):
    path = ROOT / 'build' / name
    if path.read_bytes()[:2] != b'MZ':
        raise RuntimeError(f'Not a Windows artifact: {name}')
    files['source/build/' + name] = path
files['bin/.l4d2bridge/plugins/LDBDangoMusic.dll'] = ROOT / 'build/LDBDangoMusic.dll'
files['bin/.l4d2bridge/plugins/LDBDangoMusic/config.json'] = ROOT / 'config.example.json'
files['README.md'] = ROOT / 'README.md'
files['docs/0.2.0-validation.md'] = ROOT / 'docs/0.2.0-validation.md'
files['LICENSE'] = ROOT / 'LICENSE'
files['NOTICE.md'] = ROOT / 'NOTICE.md'
if json.loads((ROOT / 'config.example.json').read_text())['demo'] is not True:
    raise RuntimeError('Demo package must enable demo')
output = ROOT / 'releases' / f'LDBDangoMusic-{VERSION}-demo.zip'
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
