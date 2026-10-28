"""Deterministic BLF/LX fixture for Windows plugin validation; loopback only.
Run: python scripts/mock_player.py. Uses port 23330; never contacts a music service.
40-second cycle: play, pause at 6-10s, seek at 14s, switch song at 20s.
"""
import json
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import urlsplit

START = time.monotonic()


def payload(path, elapsed):
    t = elapsed % 40
    second = t >= 20
    position = t - 20 if second else (6 if 6 <= t < 10 else t - 4 if t >= 10 else t)
    if not second and t >= 14:
        position += 6
    if path == '/status':
        return dict(status='paused' if 6 <= t < 10 else 'playing',
                    name='Fixture B' if second else 'Fixture A', singer='Local test',
                    duration=40, progress=position, playbackRate=1)
    if path == '/lyric-all':
        label = '第二首' if second else '第一首'
        return dict(lyric='[00:00.000]开始\n[00:06.000]暂停与恢复\n[00:15.000]进度跳转',
                    lxlyric=f'[00:00.000]<0,800>{label}<800,1000>逐字歌词\n[00:06.000]<6000,800>暂停<6800,800>与恢复\n[00:15.000]<15000,800>进度<15800,800>跳转',
                    tlyric='[00:00.000]Start\n[00:06.000]Pause and resume\n[00:15.000]Seek')
    return None


class Handler(BaseHTTPRequestHandler):
    def do_GET(self):
        value = payload(urlsplit(self.path).path, time.monotonic() - START)
        body = json.dumps(value, ensure_ascii=False).encode('utf-8')
        self.send_response(200 if value is not None else 404)
        self.send_header('Content-Type', 'application/json; charset=utf-8')
        self.send_header('Content-Length', str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def log_message(self, *args):
        pass


if __name__ == '__main__':
    print('Fixture listening on loopback port 23330; Ctrl+C to stop', flush=True)
    ThreadingHTTPServer(('127.0.0.1', 23330), Handler).serve_forever()
