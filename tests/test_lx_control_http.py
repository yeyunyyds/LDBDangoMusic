"""Drive production C++ Executor against a real local HTTP fixture, never a user's player."""
import http.server
import json
import subprocess
import threading
import urllib.request
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
state = dict(status='playing', name='Song', singer='Artist', duration=120, progress=12, playbackRate=1)
requests = []
noop = False
class Handler(http.server.BaseHTTPRequestHandler):
    def do_GET(self):
        global state
        requests.append(self.path)
        if self.path.startswith('/status'):
            body=json.dumps(state).encode()
        else:
            if not noop:
                if self.path == '/play': state['status']='playing'
                elif self.path == '/pause': state['status']='paused'
                elif self.path.startswith('/seek?offset='): state['progress']=float(self.path.split('=')[1])
                else: self.send_error(404); return
            body=b'OK'
        self.send_response(200);self.end_headers();self.wfile.write(body)
    def log_message(self,*args): pass
server=http.server.ThreadingHTTPServer(('127.0.0.1',0),Handler)
threading.Thread(target=server.serve_forever,daemon=True).start()
def get(path):
    with urllib.request.urlopen(f'http://127.0.0.1:{server.server_port}{path}',timeout=2) as r:return r.read().decode()
def run(action,position=0):
    p=subprocess.Popen([str(ROOT/'build/lx_control_driver')],stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True)
    def send(o):p.stdin.write(json.dumps(o)+'\n');p.stdin.flush()
    send(dict(tick=1000,status=json.loads(get('/status')),action=action,position=position))
    while True:
        msg=json.loads(p.stdout.readline())
        if 'request' not in msg:break
        send(dict(body=get(msg['request'])))
    assert msg['accepted']
    send(dict(tick=1100,status=json.loads(get('/status'))))
    result=json.loads(p.stdout.readline())
    if not result['done']:
        send(dict(tick=3100,status=json.loads(get('/status'))));result=json.loads(p.stdout.readline())
    p.stdin.close();assert p.wait(timeout=3)==0,p.stderr.read()
    return result
try:
    state['status']='stopped'
    assert run(0)['verified']  # explicit Play can start an inactive selected track
    assert run(1)['verified']  # pause
    assert run(0)['verified']  # play
    assert run(2,30_125)['verified'] and state['progress']==30.125
    assert run(3)['verified'] and state['status']=='paused' and state['progress']==0
    noop=True
    assert not run(0)['verified']  # HTTP 200/OK cannot fake a successful playback callback
    assert requests.count('/pause')>=2 and '/seek?offset=30.125' in requests
    print('Passed: real HTTP fixture -> production C++ play/pause/seek/stop callbacks; OK-without-change times out')
finally:
    server.shutdown();server.server_close()
