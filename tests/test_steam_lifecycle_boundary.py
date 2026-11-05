"""Guard the in-game boundary that previously crashed the live game.

ISteamClient's user/pipe lifecycle calls are documented NOT THREADSAFE and
cannot be made safe by a plugin-only mutex while the game is using Steam.
"""
from pathlib import Path
import re

root = Path(__file__).resolve().parents[1]
source = (root/'src/sync/steam_transport.hpp').read_text(encoding='utf-8')
# SteamClient lifecycle slots: pipe create/release/global user/local user/user release.
# Networking slots with the same numbers are unrelated and remain permitted.
assert not re.search(r'invoke\s*<[^>]+>\s*\(\s*client\s*,\s*[0-4]\s*[,)]', source), 'In-game SteamClient lifecycle operation reintroduced'
assert 'createLocalTest' not in source, 'Unsafe transport method reintroduced'
runtime = (root/'src/sync/runtime.hpp').read_text(encoding='utf-8')
blocked = re.search(r'if\(r\.action==2\)\{([^}]+)\}', runtime)
assert blocked and 'BLOCKED' in blocked[1] and 'continue;' in blocked[1], 'User test must stop before Steam binding'
assert not any(name in blocked[1] for name in ('ensure(', 'invoke<', 'transport->')), 'Blocked entry still touches the Steam SDK'
print('Passed: game plugin cannot create/release Steam users/pipes; blocked user diagnostic never binds Steam')
