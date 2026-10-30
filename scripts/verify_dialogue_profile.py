"""Verify NPC subtitle read/hook evidence against uploaded binaries, without execution."""
import argparse
import hashlib
import json
import struct
from pathlib import Path
from verify_source_binaries import PE


def verify(path):
    root = Path(__file__).resolve().parents[1]
    c = PE(path)
    profile = json.loads((root/'vendor/source-binary-profile.json').read_text())['client']
    assert hashlib.sha256(c.data).hexdigest() == profile['sha256']
    assert c.pointer(profile['vtable_rva']+35*4) == c.base+0xa9160
    assert c.read(0xa9160,15) == bytes.fromhex('558bec8b0d')+struct.pack('<I',c.base+0x6de3a4)+bytes.fromhex('5de9f12e1000')
    # Tail-call dispatch bounds-checks id, reads 24-byte nodes and copies exactly
    # 9 dwords of bf_read before calling each registered cdecl callback.
    assert c.read(0x1ac071,3) == bytes.fromhex('3b4114')
    assert c.read(0x1ac07a,11) == bytes.fromhex('8b49048d1440578b7cd114')
    assert c.read(0x1ac172,12) == bytes.fromhex('b9090000008bf38d7dd8f3a5')
    assert c.read(0x120b39,10) == b'\x68'+struct.pack('<I',c.base+0x11fbc0)+b'\x68'+struct.pack('<I',c.base+0x57125c)
    assert c.read(0x120b4e,10) == b'\x68'+struct.pack('<I',c.base+0x11fbf0)+b'\x68'+struct.pack('<I',c.base+0x571248)
    # 32-bit hash, 15-bit duration scaled by 0.1 seconds, then one flag.
    assert c.read(0x11f172,3) == bytes.fromhex('83fe20')
    assert c.read(0x11f25e,3) == bytes.fromhex('83f90f')
    assert c.read(0x11f351,3) == bytes.fromhex('83f901')
    assert abs(struct.unpack('<f',c.read(0x53d228,4))[0]-.1) < 1e-7
    assert c.read(0x11f504,3) == bytes.fromhex('83fe20')
    assert c.read(0x11f5de,3) == bytes.fromhex('83f90f')
    assert c.read(0x11f6cd,3) == bytes.fromhex('83f901')
    # Four-byte m_survivorCharacter RecvProp. Runtime uses metadata, not bd0.
    assert c.read(0x2226e9,12) == bytes.fromhex('6a0468d00b000068')+struct.pack('<I',c.base+0x59e858)
    roles=[]
    for i in range(8):
        case=c.pointer(0x2866d4+i*4)-c.base
        assert c.read(case,1)==b'\xb8'
        name=c.pointer(case+1)-c.base
        roles.append(c.read(name,40).split(b'\0',1)[0].decode('ascii'))
    assert roles==['Nick','Rochelle','Coach','Ellis','Bill','Zoey','Francis','Louis']
    # Native caption dictionary loader reads a 24-byte VCCD v1 header, dir count
    # at +16 and a 12-byte directory stride.
    assert c.read(0xd5ecc,6)==bytes.fromhex('813e56434344')
    assert c.read(0xd5f16,4)==bytes.fromhex('837f3c01')
    assert c.read(0xd5f27,3)==bytes.fromhex('8b4748')
    return {'static_profile_verified':True,'runtime_verified':False,'sha256':profile['sha256'],
            'interface':'VClient016','dispatch_slot':35,'dispatch_rva':'0xa9160',
            'manager_pointer_rva':'0x6de3a4','node_bytes':24,'node_value_offset':20,
            'message_name_offset':4,'bf_read_bytes':36,
            'caption_header_bits':[32,15,1],'duration_unit_ms':100,'roles':roles,
            'caption_file':{'magic':'VCCD','version':1,'header_bytes':24,'directory_entry_bytes':12},
            'notes':['No uploaded DLL is loaded or executed.',
                     'Runtime binding validates the exact callback/profile before modifying one vtable slot.',
                     'Role prefix presence, dictionary priority and live matching still require game validation.',
                     'The legacy-campaign conversion preceding the role-name switch is not dispatched; raw network character values are logged for verification.']}

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--client',required=True);a=p.parse_args()
    print(json.dumps(verify(a.client),indent=2))
