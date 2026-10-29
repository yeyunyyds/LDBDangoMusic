"""Check static x86 TraceRay ABI evidence in the supported engine.dll (no execution)."""
import argparse
import hashlib
import json
from pathlib import Path
from verify_source_binaries import PE


def verify(path):
    image = PE(path)
    profile = json.loads((Path(__file__).resolve().parents[1] / 'docs/research/engine-trace-discovery.json').read_text())
    if hashlib.sha256(image.data).hexdigest() != profile['sha256']:
        raise ValueError('Unknown engine.dll; static Trace ABI evidence is build-specific')
    signatures = {
        'TraceRay entry': (0x179760, '538bdc83ec0883e4f0'),
        'TraceRay ret 16': (0x179e2b, 'c21000'),
        'ClearTrace 84 bytes': (0x14ba10, '558bec568b75086a54'),
        'Result fraction +44': (0x14ba29, 'f30f11462c'),
        'Result startsolid +55': (0x179978, '807f3700'),
        'Result surface +60': (0x14ba3b, '89463c'),
        'Ray isRay +68': (0x1505dc, '8a5744'),
        'Ray isSwept +69': (0x1505e5, '8a4745'),
        'Filter method index 1': (0x179932, '8b018b5004ffd2'),
    }
    for name, (rva, encoded) in signatures.items():
        expected = bytes.fromhex(encoded)
        if image.read(rva, len(expected)) != expected:
            raise ValueError('Static ABI mismatch: ' + name)
    return dict(sha256=profile['sha256'], evidence=list(signatures), static_abi_verified=True,
                runtime_dispatch_verified=False)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('engine')
    args = parser.parse_args()
    print(json.dumps(verify(args.engine), indent=2))
