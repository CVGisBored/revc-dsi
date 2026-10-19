#!/usr/bin/env python3
"""Losslessly order existing COL triangles into spatially bounded read blocks."""
import collections
import hashlib
import json
from pathlib import Path
import struct

BLOCK = 128
FACE = struct.Struct('<H9h')

def build(root):
    index = (root / 'collision.vcc').read_bytes()
    magic, count = struct.unpack_from('<4sI', index)
    assert magic in (b'VCC1', b'VCC2') and len(index) == 8 + count * 16
    source = (root / 'collision.bin').read_bytes()
    output, bounds = bytearray(), bytearray()
    def spatial(face):
        xyz = FACE.unpack(face)[1:]
        center = [max(0, min(63, sum(xyz[k::3]) // 192)) for k in range(3)]
        return sum(((center[k] >> bit) & 1) << (bit * 3 + k) for bit in range(6) for k in range(3)), face
    for x, y, z, pad, offset, faces in struct.iter_unpack('<hhhHII', index[8:]):
        assert not pad and offset == len(output)
        original = [source[p:p+20] for p in range(offset, offset + faces * 20, 20)]
        ordered = sorted(original, key=spatial)
        assert collections.Counter(original) == collections.Counter(ordered)
        output.extend(b''.join(ordered))
        for start in range(0, faces, BLOCK):
            coords = [FACE.unpack(face)[1:] for face in ordered[start:start+BLOCK]]
            low = [min(v for face in coords for v in face[k::3]) for k in range(3)]
            high = [max(v for face in coords for v in face[k::3]) for k in range(3)]
            assert all(0 <= lo <= hi <= 4096 for lo, hi in zip(low, high))
            bounds.extend(struct.pack('<6B', *[v//64 for v in low], *[(v+63)//64 for v in high]))
    assert len(output) == len(source)
    (root / 'collision.bin').write_bytes(output)
    (root / 'collision-bounds.vcb').write_bytes(struct.pack('<4sII', b'VCB2', len(bounds)//6, BLOCK) + bounds)
    (root / 'collision.vcc').write_bytes(b'VCC2' + index[4:])
    report = {'triangles': len(source)//20, 'blocks': len(bounds)//6,
              'bounds_bytes': len(bounds)+12, 'triangle_multisets_preserved': True,
              'collision_sha256': hashlib.sha256(output).hexdigest()}
    return report

if __name__ == '__main__':
    workspace = Path(__file__).resolve().parents[1]
    report = build(workspace / 'sdcard/revc/world')
    (workspace / 'reports/collision-blocks.json').write_text(json.dumps(report, indent=2)+'\n')
    print(json.dumps(report))
