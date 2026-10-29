"""Static entity/scene interface evidence; never loads or executes a game DLL."""
import argparse
import hashlib
import json
import re
import struct
from verify_source_binaries import PE


def discover(path):
    image = PE(path)

    def rva(offset):
        for start, _, raw, size in image.sections:
            if raw <= offset < raw + size:
                return start + offset - raw
        raise ValueError('Offset outside section')

    interfaces = []
    pattern = rb'(?:VClientEntityList|VModelInfoClient|StaticPropMgrClient|VClient|EngineTraceClient)\d{3}\0'
    for name in re.finditer(pattern, image.data):
        address = rva(name.start())
        signature = b'\x68' + struct.pack('<I', image.base + address) + b'\x68'
        for registration in re.finditer(re.escape(signature), image.data):
            factory = struct.unpack_from('<I', image.data, registration.start() + 6)[0] - image.base
            try:
                code = image.read(factory, 6)
            except ValueError:
                continue
            row = dict(name=name.group()[:-1].decode(), name_rva=hex(address),
                       registration_rva=hex(rva(registration.start())), factory_rva=hex(factory),
                       factory_bytes=code.hex())
            if code[0] == 0xb8 and code[5] == 0xc3:
                obj = struct.unpack_from('<I', code, 1)[0] - image.base
                row['object_rva'] = hex(obj)
                try:
                    table = image.pointer(obj) - image.base
                    methods = [image.pointer(table + i*4)-image.base for i in range(9)]
                    if all(0 <= method < image.image_size for method in methods):
                        row['vtable_rva'] = hex(table)
                        row['first_9_method_rvas'] = list(map(hex, methods))
                except ValueError:
                    row['vtable_note'] = 'Object is initialized at runtime; no file-backed vtable proof.'
            interfaces.append(row)
    properties = sorted(set(m.group()[:-1].decode() for m in re.finditer(
        rb'm_(?:hActiveWeapon|hOwnerEntity|vecOrigin|angRotation|iTeamNum|nModelIndex|hWeapon|hViewModel)\0', image.data)))
    classes = sorted(set(m.group()[:-1].decode() for m in re.finditer(
        rb'DT_(?:BaseEntity|BasePlayer|BaseCombatCharacter|BaseCombatWeapon|BaseViewModel|MolotovProjectile|PipeBombProjectile|VomitJarProjectile)\0', image.data)))
    return dict(sha256=hashlib.sha256(image.data).hexdigest(), timestamp=image.timestamp,
                image_size=image.image_size, interfaces=interfaces, receive_table_names=classes,
                property_names=properties, runtime_dispatch_verified=False,
                note='Registration/string evidence only. No validated entity, bone, attachment or static-prop dispatch ABI yet.')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--client', required=True)
    parser.add_argument('--engine', required=True)
    args = parser.parse_args()
    print(json.dumps(dict(client=discover(args.client), engine=discover(args.engine)), indent=2))
