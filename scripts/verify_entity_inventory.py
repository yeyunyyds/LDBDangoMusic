"""Verify diagnostic read layout and call profiles against user DLLs; no execution."""
import argparse
import hashlib
import json
import re
import struct
from pathlib import Path
from verify_source_binaries import PE


def verify(client, engine, replay=None):
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
    assert e.read(0x1b0950,6) == b'\xb8'+(e.base+0x432248).to_bytes(4,'little')+b'\xc3'
    assert e.pointer(0x432248) == e.base+0x3902e4
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
    # Actual RecvProp construction: four-byte parent handle, byte attachment.
    assert c.read(0x244f1-9,9)==bytes.fromhex('6a006a046834010000')
    assert c.read(0x244f1,5)==b'\x68'+struct.pack('<I',c.base+0x540048)
    assert c.read(0x4b3fa-11,11)==bytes.fromhex('6a006a006a0168f8010000')
    assert c.read(0x4b3fa,5)==b'\x68'+struct.pack('<I',c.base+0x544c2c)
    # Receive proxy expands wire packing into the same 12-bit-index handle.
    assert c.read(0x18bfc2,15)==bytes.fromhex('c1f90bc1e10c25ff0700000bc8890a')
    def slot(name):
        return int(re.search(r'\b' + name + r'\s*=\s*(\d+)', header).group(1))
    slots = dict(client_class=slot('ClassSlot'), dormant=slot('DormantSlot'), data_base=slot('DataBaseSlot'))
    assert slots == dict(client_class=1, dormant=7, data_base=10)
    # These four function RVAs were reported as class getters by the failed
    # runtime capture. RTTI identifies their tables as the +8 networkable
    # subobject. Verify the real getter positions in every matching table.
    tables = set()
    samples = []
    dormancy_patterns = {}
    for hint in (0x4cc20, 0x226ae0, 0x2e5a0, 0x41690):
        position = 0
        family = []
        while True:
            position = c.data.find(struct.pack('<I', c.base + hint), position)
            if position < 0:
                break
            found = position
            position += 4
            section = next((s for s in c.sections if s[2] <= found < s[2] + s[3]), None)
            if section is None:
                continue
            vt = section[0] + found - section[2] - 8
            try:
                col = c.pointer(vt - 4) - c.base
                if struct.unpack('<2I', c.read(col, 8)) != (0, 8):
                    continue
                methods = [c.pointer(vt + i * 4) - c.base for i in range(12)]
            except ValueError:
                continue
            getter = c.read(methods[slots['client_class']], 6)
            assert getter[0] == 0xb8 and getter[5] == 0xc3
            metadata = struct.unpack_from('<I', getter, 1)[0] - c.base
            name_ptr = c.pointer(metadata + 8) - c.base
            name = c.read(name_ptr, 128).split(b'\0', 1)[0].decode('ascii')
            assert re.fullmatch(r'[A-Za-z_][A-Za-z0-9_]*', name), (hex(vt), name)
            assert c.read(methods[slots['data_base']], 4) == bytes.fromhex('8d41f8c3')
            dormant = c.read(methods[slots['dormant']], 16)
            pattern = 'index_guarded_boolean' if dormant == bytes.fromhex('837950ff74078a81d5000000c332c0c3') else 'constant_false' if dormant[:3] == bytes.fromhex('32c0c3') else 'unknown'
            dormancy_patterns[pattern] = dormancy_patterns.get(pattern, 0) + 1
            assert c.read(methods[8], 4) == bytes.fromhex('8b4150c3')  # index, not dormancy
            if replay is not None:
                addresses = [methods[i] for i in (1, 2, 7, 8, 10, 11)]
                segments = [[c.base + address, list(c.read(address, 16))] for address in addresses]
                segments += [[c.base + vt, list(c.read(vt, 48))],
                             [c.base + metadata, list(c.read(metadata, 24))],
                             [c.base + name_ptr, list(c.read(name_ptr, 128))]]
                replay.append(dict(name=name, vtable=c.base + vt, segments=segments,
                                   constant_dormant_false=pattern == 'constant_false'))
            tables.add(vt)
            family.append(dict(table_rva=hex(vt), class_getter_rva=hex(methods[1]), class_name=name))
        assert family, f'No RTTI-confirmed networkable table for log hint {hint:x}'
        samples.append(dict(previous_class_hint_rva=hex(hint), verified_tables=len(family), sample=family[0]))
    return dict(static_profile_verified=True,runtime_dispatch_verified=False,entity_interface='VClientEntityList003',
                entity_object_rva='0x74a574',entity_vtable_rva='0x564b60',interface_subobject_offset='0x10014',
                entry_base_from_interface='-0x10010',entry_size=16,entry_bits=12,serial_bits=20,
                networkable_cache_offset='0x28',cache_stride=8,networked_limit=2048,
                model_interface='VModelInfoClient004',checks=checks,networkable_slots=slots,
                network_parent_layout={'moveparent': {'offset': '0x134', 'bytes': 4}, 'm_iParentAttachment': {'offset': '0x1f8', 'bytes': 1}},
                network_tables_verified=len(tables),dormancy_patterns=dormancy_patterns,runtime_hint_evidence=samples,
                notes=['No entity virtuals are dispatched: tiny class/data-base/dormancy getters are decoded or reported unknown.',
                       'RecvTable/RecvProp interpretation is bounded and shape-checked at runtime; offsets are network properties, not render poses.',
                       'Only the profiled model-info GetModel/GetModelName calls are dispatched; real game validation is pending.'])

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--client',required=True);p.add_argument('--engine',required=True);p.add_argument('--replay-output',type=Path);a=p.parse_args()
    replay=[] if a.replay_output else None
    result=verify(a.client,a.engine,replay)
    if a.replay_output:a.replay_output.write_text(json.dumps(replay),encoding='utf-8')
    print(json.dumps(result,indent=2))
