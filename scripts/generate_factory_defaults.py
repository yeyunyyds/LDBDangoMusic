"""Keep reset defaults identical to the distributed configuration."""
import argparse,json
from pathlib import Path
root=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--check',action='store_true');args=p.parse_args()
config=json.loads((root/'config.example.json').read_text())
text='#pragma once\n// Generated from config.example.json; do not hand edit.\n#include "lyrics_settings.hpp"\nnamespace dango {\ninline Config installedDefaults(){return decodeConfig(nlohmann::json::parse(R"DANGO_DEFAULTS('+json.dumps(config,ensure_ascii=False,separators=(',',':'))+')DANGO_DEFAULTS"));}\n}\n'
out=root/'src/factory_defaults.hpp'
if args.check:
    assert out.read_text()==text,'Factory defaults are stale: run scripts/generate_factory_defaults.py'
else:out.write_text(text)
print('Verified reset defaults match the distributed configuration')
