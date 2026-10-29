"""Verify diagnostic read layout and call profiles against user DLLs; no execution."""
import argparse
import hashlib
import json
import re
from pathlib import Path
from verify_source_binaries import PE


def verify(client, engine):
    root = Path(__file__).resolve().parents[1]
    header = (root/'src/source_entity_profile.hpp').read_text()
    images = dict(client=PE(client), engine=PE(engine))
    expected = json.loads((root/'vendor/source-binary-profile.json').read_text())
    for name, image in images.items():
        if hashlib.sha256(image.data).hexdigest() != expected[name]['sha256']:
            raise ValueError(f'{name} does not match the supported build')
    def values(name):
        data = re.search(r'\b'+name+r'\[\]\s*=\s*\{([^}]+)\}', header).group(1)
        return [int(n,16) for n in re.findall(r'0x([0-9a-fA-F]+)', data)]
    c, e = images['client'], images['engine']
    assert c.read(0xb8cf0,6) == b'\xb8'+(c.base+0x74a574).to_bytes(4,'little')+b'\xc3'
    # RTTI COL proves this interface is the +0x10014 subobject, not the base.
    col = c.pointer(0x564b60-4)-c.base
    assert int.from_bytes(c.read(col+4,4),'little') == 0x10014
    assert c.read(0xbb518,10) == bytes.fromhex('c78614000100')+(c.base+0x564b60).to_bytes(4,'little')
    for slot, address in enumerate(values('Methods')):assert c.pointer(0x564b60+slot*4) == c.base+address
    for slot, address in enumerate(values('ModelMethods')):assert e.pointer(0x3902e4+slot*4) == e.base+address
    checks={}
    for name,image,address in [('HighestCode',c,0xb8db0),('HandleCode',c,0xbb4c0),('ModelCode',e,0x1b0420),('NameCode',e,0x1afba0)]:
        code=bytes(values(name));assert image.read(address,len(code))==code
        checks[name]=dict(rva=hex(address),bytes=code.hex())
    assert b'\xc1\xea\x0c\x39\x50\x04' in c.read(0xbb4c0,48)
    assert bytes.fromhex('05ffefffff03c08b0cc1') in c.read(0xb8dc0,48)
    return dict(static_profile_verified=True,runtime_dispatch_verified=False,entity_interface='VClientEntityList003',
                entity_object_rva='0x74a574',entity_vtable_rva='0x564b60',interface_subobject_offset='0x10014',
                entry_base_from_interface='-0x10010',entry_size=16,entry_bits=12,serial_bits=20,
                networkable_cache_offset='0x28',cache_stride=8,networked_limit=2048,
                model_interface='VModelInfoClient004',checks=checks,
                notes=['No entity virtuals are dispatched: tiny class/data-base/dormancy getters are decoded or reported unknown.',
                       'RecvTable/RecvProp interpretation is bounded and shape-checked at runtime; offsets are network properties, not render poses.',
                       'Only the profiled model-info GetModel/GetModelName calls are dispatched; real game validation is pending.'])

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--client',required=True);p.add_argument('--engine',required=True);a=p.parse_args()
    print(json.dumps(verify(a.client,a.engine),indent=2))
