#!/usr/bin/env python3
"""Retain every original walkable render face so tile caps cannot remove ground."""
import json
from pathlib import Path
import struct
import math
from stage_quality import SOURCE
import stage_world
from collections import defaultdict

def build(root,report):
    stage_world.CELL=8
    entries=list(struct.iter_unpack('<hhhHII',(root/'index.vcq').read_bytes()[20:]));index=[];offset=total=fallback=0
    with (root/'quality-source.bin').open('rb') as src,(root/'terrain.bin').open('wb') as out:
        for x,y,z,_,start,count in entries:
            src.seek(start);tiles=defaultdict(bytearray)
            for color,texture,flags,*values in SOURCE.iter_unpack(src.read(count*SOURCE.size)):
                if flags:continue
                coords=values[:9];a,b,c=[coords[i:i+3] for i in (0,3,6)]
                u=[b[k]-a[k] for k in range(3)];v=[c[k]-a[k] for k in range(3)]
                n=(u[1]*v[2]-u[2]*v[1],u[2]*v[0]-u[0]*v[2],u[0]*v[1]-u[1]*v[0])
                if not n[1]*n[1] or n[1]*n[1]<n[0]*n[0]+n[2]*n[2]:continue
                points=[(*[v/64 for v in coords[i*3:i*3+3]],*values[9+i*2:11+i*2]) for i in range(3)]
                for tx,ty,tz,tri in stage_world.cells(points):
                    if not all(0<=k<8 for k in (tx,ty,tz)):continue
                    xyz=[round((p[k]-(tx,ty,tz)[k]*8)*64) for p in tri for k in range(3)]
                    phase=[math.floor((min(p[k] for p in tri)+max(p[k] for p in tri))/2) for k in (3,4)]
                    uv=[round((p[k]-phase[k-3])*1024) for p in tri for k in (3,4)];tex=texture
                    if any(not -32768<=v<=32767 for v in uv):tex=0;uv=[0]*6;fallback+=1
                    tiles[tx+ty*8+tz*64].extend(struct.pack('<HH9H6h',color,tex,*xyz,*uv));total+=1
            if tiles:
                counts=[len(tiles[i])//34 for i in range(512)]
                if max(counts)>65535:raise ValueError('Terrain tile exceeds count ABI')
                block=struct.pack('<512H',*counts)+b''.join(tiles[i] for i in range(512))
                index.append(struct.pack('<hhhHII',x,y,z,0,offset,len(block)));out.write(block);offset+=len(block)
    (root/'terrain.vct').write_bytes(struct.pack('<4sI',b'VTR2',len(index))+b''.join(index))
    info={'chunks':len(index),'triangles':total,'bytes':offset,'uv_fallbacks':fallback,'per_tile_triangle_limit':None}
    report.write_text(json.dumps(info,indent=2)+'\n');print(json.dumps(info))

if __name__=='__main__':
    w=Path(__file__).resolve().parents[1];build(w/'sdcard/revc/world',w/'reports/terrain.json')
