"""Static PE validation of the supported game build; never loads/executes a DLL.
Usage: python scripts/verify_source_binaries.py --client client.dll --engine engine.dll
Uses only Python standard library. Game DLLs are not bundled.
"""
import argparse
import hashlib
import json
import struct
from pathlib import Path


class PE:
    def __init__(self, path):
        self.data = Path(path).read_bytes()
        if self.data[:2] != b'MZ':
            raise ValueError('Not a PE')
        pe = self.u32(0x3c)
        if self.data[pe:pe + 4] != b'PE\0\0' or self.u16(pe + 4) != 0x14c:
            raise ValueError('Expected x86 PE')
        self.timestamp = self.u32(pe + 8)
        optional = pe + 24
        self.base = self.u32(optional + 28)
        self.image_size = self.u32(optional + 56)
        first = optional + self.u16(pe + 20)
        self.sections = []
        for i in range(self.u16(pe + 6)):
            offset = first + i * 40
            virtual_size, rva, size, raw = struct.unpack_from('<4I', self.data, offset + 8)
            self.sections.append((rva, max(virtual_size, size), raw, size))

    def u16(self, offset):
        return struct.unpack_from('<H', self.data, offset)[0]

    def u32(self, offset):
        return struct.unpack_from('<I', self.data, offset)[0]

    def read(self, rva, size):
        for start, length, raw, raw_size in self.sections:
            if start <= rva and rva + size <= start + raw_size:
                return self.data[raw + rva - start:raw + rva - start + size]
        raise ValueError(f'Unmapped file RVA {rva:x}')

    def pointer(self, rva):
        return struct.unpack('<I', self.read(rva, 4))[0]


def verify(path, profile):
    image = PE(path)
    if hashlib.sha256(image.data).hexdigest() != profile['sha256']:
        raise ValueError('File SHA-256 does not match the verified build')
    if (image.timestamp, image.image_size) != (profile['timestamp'], profile['image_size']):
        raise ValueError('PE identity mismatch')
    factory = image.read(profile['factory_rva'], 6)
    if factory != b'\xb8' + struct.pack('<I', image.base + profile['object_rva']) + b'\xc3':
        raise ValueError('Interface constructor return object mismatch')
    if image.pointer(profile['object_rva']) != image.base + profile['vtable_rva']:
        raise ValueError('Object vtable mismatch')
    name=profile['interface'].encode()+b'\0'
    if image.read(profile['interface_name_rva'],len(name)) != name:
        raise ValueError('Interface name absent at registered address')
    registration=b'\x68'+struct.pack('<I',image.base+profile['interface_name_rva'])+b'\x68'+struct.pack('<I',image.base+profile['factory_rva'])
    if image.read(profile['registration_rva'],10) != registration:
        raise ValueError('Interface registration/factory mismatch')
    for name, method in profile['methods'].items():
        actual = image.pointer(profile['vtable_rva'] + method['slot'] * 4) - image.base
        if actual != method['rva']:
            raise ValueError(f'Method mismatch: {name}')
    return dict(interface=profile['interface'], timestamp=image.timestamp, image_size=image.image_size, methods=profile['methods'])


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--client', required=True)
    parser.add_argument('--engine', required=True)
    args = parser.parse_args()
    profiles = json.loads((Path(__file__).resolve().parents[1] / 'vendor/source-binary-profile.json').read_text())
    for name in ('client', 'engine'):
        print(name, json.dumps(verify(getattr(args, name), profiles[name]), indent=2))
