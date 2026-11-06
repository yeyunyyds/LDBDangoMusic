import json, subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[1]
actual=json.loads(subprocess.check_output([root/'build/visual_presets_test','--dump']))
for i,name in enumerate(('minimal-subtitles','neon-night','sunset-film','ice-crystal','ink','breathing','gallery')):
    assert json.loads((root/f'presets/visual-{name}.json').read_text())==actual[i],name
config=json.loads((root/'config.example.json').read_text())
assert not any(config['visualStyle'][k] for k in ('tone','dynamicGlow','smoothScale','bob','wallEffects'))
assert not any(config['steamSync'][k] for k in ('enabled','discover','autoSearch','apply'))
print('Passed: seven exported visual presets match runtime; distributed visual and networking defaults remain off')
