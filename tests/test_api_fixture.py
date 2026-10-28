import importlib.util
import json
from pathlib import Path
import subprocess
import tempfile
import threading
import time
import urllib.request

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('mock', ROOT / 'scripts/mock_player.py')
mock = importlib.util.module_from_spec(spec)
spec.loader.exec_module(mock)
server = mock.ThreadingHTTPServer(('127.0.0.1', 0), mock.Handler)
thread = threading.Thread(target=server.serve_forever, daemon=True)
thread.start()
opener = urllib.request.build_opener(urllib.request.ProxyHandler({}))
try:
    with tempfile.TemporaryDirectory() as directory:
        paths = [Path(directory) / name for name in ('status.json', 'lyrics.json')]
        for phase, title, playing, position in [(2, 'Fixture A', 'playing', 2000), (8, 'Fixture A', 'paused', 6000), (15, 'Fixture A', 'playing', 17000), (22, 'Fixture B', 'playing', 2000)]:
            mock.START = time.monotonic() - phase
            for path, endpoint in zip(paths, ('status', 'lyric-all')):
                with opener.open(f'http://127.0.0.1:{server.server_port}/{endpoint}', timeout=2) as response:
                    path.write_bytes(response.read())
            output = subprocess.check_output([str(ROOT / 'build/api_fixture'), *map(str, paths), title], text=True)
            actual_title, actual_playing, actual_position = output.strip().split('|')
            if actual_title != title or actual_playing != playing or abs(int(actual_position) - position) > 250:
                raise RuntimeError(output)
    print('Passed: HTTP fixtures -> C++ status/LX parsing, play/pause/seek/track switch')
finally:
    server.shutdown()
    server.server_close()
    thread.join()
