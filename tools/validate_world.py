#!/usr/bin/env python3
"""Validate every packaged world record, index boundary and conversion coverage."""
import json
from pathlib import Path
import struct


def validate(root,report):
    info=json.loads(report.read_text())
    if info['skipped'] or info['placements_converted']!=info['placements_total']:
        raise ValueError('Map conversion coverage is incomplete')
    index=(root/'index.vcw').read_bytes()
    magic,count,cx,cy,cell=struct.unpack_from('<4sIffI',index)
    if magic!=b'VCW1' or (cx,cy,cell)!=(230.,-1250.,64) or len(index)!=20+count*16:
        raise ValueError('Invalid world header or index length')
    entries=list(struct.iter_unpack('<hhhHII',index[20:]))
    if len(entries)!=info['chunks'] or entries!=sorted(entries) or len({r[:3] for r in entries})!=count:
        raise ValueError('Invalid index coverage or ordering')
    offset=0;active=0;total=0
    with (root/'chunks.bin').open('rb') as file:
        for x,y,z,reserved,start,records in entries:
            if reserved or start!=offset or not records:
                raise ValueError('Invalid chunk offset/count')
            data=file.read(records*22)
            if len(data)!=records*22:
                raise ValueError('Truncated chunk')
            for color,flags,*coords in struct.iter_unpack('<HH9h',data):
                if color>0x7fff or flags>3 or any(v<0 or v>4096 for v in coords):
                    raise ValueError(f'Invalid chunk record at {x,y,z}')
                active+=not flags
            offset+=len(data);total+=records
        if file.read(1) or offset!=info['chunk_bytes'] or active!=info['active_chunk_triangles']:
            raise ValueError('Chunk bytes/count differs from conversion report')
    player=(root/'player.vcp').read_bytes()
    magic,count=struct.unpack_from('<4sI',player)
    if magic!=b'VCP1' or not 0<count<=550 or count!=info['player_triangles'] or len(player)!=8+20*count:
        raise ValueError('Invalid original player conversion')
    print(f"World coverage: {info['placements_converted']}/{info['placements_total']} placements; "
          f"{info['chunks']} chunks; {total} triangle records validated; player {count} triangles")


if __name__=='__main__':
    workspace=Path(__file__).resolve().parents[1]
    validate(workspace/'sdcard/revc/world',workspace/'reports/world.json')
