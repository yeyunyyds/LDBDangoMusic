"""Locate registered EngineTrace factories in an x86 engine.dll without executing it.
The resulting method slots are research evidence, not an approved runtime ABI.
"""
import argparse
import hashlib
import json
import re
import struct
from verify_source_binaries import PE


def discover(path):
    image = PE(path)

    def rva(offset):
        for start, length, raw, size in image.sections:
            if raw <= offset < raw + size:
                return start + offset - raw
        raise ValueError('File offset outside section data')

    interfaces = []
    for name in re.finditer(rb'EngineTrace(?:Client|Server)\d{3}\0', image.data):
        name_rva = rva(name.start())
        signature = b'\x68' + struct.pack('<I', image.base + name_rva) + b'\x68'
        for registration in re.finditer(re.escape(signature), image.data):
            factory = struct.unpack_from('<I', image.data, registration.start() + 6)[0] - image.base
            code = image.read(factory, 6)
            if code[0] != 0xb8 or code[5] != 0xc3:
                continue
            obj = struct.unpack_from('<I', code, 1)[0] - image.base
            vtable = image.pointer(obj) - image.base
            interfaces.append(dict(name=name.group()[:-1].decode(), name_rva=hex(name_rva),
                                   registration_rva=hex(rva(registration.start())), factory_rva=hex(factory),
                                   object_rva=hex(obj), vtable_rva=hex(vtable),
                                   first_8_method_rvas=[hex(image.pointer(vtable + i * 4) - image.base) for i in range(8)]))
    return dict(sha256=hashlib.sha256(image.data).hexdigest(), timestamp=image.timestamp,
                image_size=image.image_size, interfaces=interfaces, runtime_dispatch_verified=False,
                sdk_trace_ray_candidate_slot=5,
                note='Public SDK candidate only. Validate Ray_t, trace_t, filter ABI and game-thread invocation before dispatch.')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('engine')
    args = parser.parse_args()
    print(json.dumps(discover(args.engine), indent=2))
