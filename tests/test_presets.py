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
