#!/usr/bin/env python3
"""Check bounded runtime tiles independently of the runtime loader."""
from pathlib import Path
import struct
import json

def validate(root,report):
    data=(root/'index.vcf').read_bytes()
    magic,count,x,z,cell=struct.unpack_from('<4sIffI',data)
    if (magic,x,z,cell)!=(b'VCF2',230.,-1250.,64) or len(data)!=20+count*16:
        raise ValueError('Invalid fast index')
    original=list(struct.iter_unpack('<hhhHII',(root/'index.vcw').read_bytes()[20:]))
    entries=list(struct.iter_unpack('<hhhHII',data[20:]))
    if [r[:3] for r in entries]!=[r[:3] for r in original]:
        raise ValueError('Fast index loses map regions')
    offset=0;total=0
    with (root/'fast.bin').open('rb') as file:
        for x,y,z,pad,start,length in entries:
            if pad or start!=offset or length<512:
                raise ValueError('Invalid fast chunk')
            block=file.read(length)
            if len(block)!=length:
                raise ValueError('Truncated fast chunk')
            counts=block[:512]
            if max(counts)>64 or 512+sum(counts)*11!=length:
                raise ValueError('Runtime tile exceeds budget')
            for color,*positions in struct.iter_unpack('<H9B',block[512:]):
                if color>32767 or max(positions)>128:
                    raise ValueError('Invalid runtime geometry')
            offset+=length;total+=sum(counts)
        if file.read(1):
            raise ValueError('Unexpected fast data')
    info=json.loads(report.read_text())
    if total!=info['runtime_triangles'] or offset!=info['bytes']:
        raise ValueError('Fast conversion report mismatch')
    print(f'Runtime tiles: {count} map regions; {total} records; max 64 triangles/tile; valid')

if __name__=='__main__':
    workspace=Path(__file__).resolve().parents[1]
    validate(workspace/'sdcard/revc/world',workspace/'reports/fast.json')
