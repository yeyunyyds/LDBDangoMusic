import json
import subprocess
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]
actual = json.loads(subprocess.check_output([str(ROOT / 'build/settings_lifecycle_test'), '--dump'], text=True))
for index, name in enumerate(('warm-white', 'soft', 'neon-pink', 'ice-blue', 'amber', 'clean')):
    preset = json.loads((ROOT / 'presets' / (name + '.json')).read_text())
    for key, expected in preset.items():
        value = actual[index][key]
        if isinstance(expected, (float, int)) and not isinstance(expected, bool):
            if abs(value - expected) > 1e-5:
                raise RuntimeError((name, key, value, expected))
        elif value != expected:
            raise RuntimeError((name, key, value, expected))
print('Passed: all distributed preset files match runtime presets')
for name in ('config.example.json', 'config.demo.json', 'config.camera-demo.json', 'config.world-demo.json'):
    config = json.loads((ROOT / name).read_text())
    if config['layout'] != actual[0]['layout'] or config['reanchorMs'] != 0 or config['defaultsVersion'] != 2 or config['verticalPosition'] != 0.44 or config['wallBackdrop'] is not False or config['wallReuse'] is not False or config['wallFloorFollow'] is not False or config['wallSurface'] != 0 or any(abs(config['wallLayout'][part][key] - actual[0]['wallLayout'][part][key]) > 1e-5 for part in ('title', 'artist', 'lyrics') for key in ('x', 'y', 'size', 'depth')):
        raise RuntimeError(('Stale bundled layout/defaults', name))
print('Passed: bundled configs use elevated lyric layout and disabled reanchor defaults')
