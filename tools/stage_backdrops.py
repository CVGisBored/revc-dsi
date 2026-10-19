#!/usr/bin/env python3
"""Identify distant island backdrops, including their untextured faces."""
import argparse
import json
from pathlib import Path
import struct
from assets import Archive,instances,place
from materials import detailed_dff
import stage_world

def fingerprint(values):
    result=2166136261
    for v in values:
        for byte in struct.pack('<i',round(v/4)):result=((result^byte)*16777619)&0xffffffff
    return result

def stage(game,workspace):
    report=json.loads((workspace/'reports/materials.json').read_text())
    names={'islandmanext1','islandlolodb','islandlolodm'}
    ids=[t['id'] for t in report['textures'] if t['dictionary'] in names];hashes=set();archive=Archive(game)
    for obj in instances(game):
        if 'islandlod' not in obj.name:continue
        for vertices,faces,uv,prelight,materials in detailed_dff(archive.read(obj.name+'.dff')):
            points=[place(obj,p) for p in vertices];points=[(p[0]-230,p[2],-(p[1]+1250)) for p in points]
            for a,b,c,mat in faces:
                if materials[mat][1]:continue
                stage_world.CELL=64
                for x,y,z,tri in stage_world.cells([points[a],points[b],points[c]]):
                    coarse=(x*64,y*64,z*64)
                    local=[tuple(round((p[k]-coarse[k])*64)/64 for k in range(3)) for p in tri]
                    stage_world.CELL=8
                    for tx,ty,tz,small in stage_world.cells(local):
                        if not all(0<=i<8 for i in (tx,ty,tz)):continue
                        hashes.add(fingerprint([round(p[k]*64)+round(coarse[k]*64) for p in small for k in range(3)]))
                    stage_world.CELL=64
    (workspace/'sdcard/revc/world/backdrops.vcb').write_bytes(struct.pack('<4sII',b'VBD2',len(ids),len(hashes))+
        struct.pack('<'+str(len(ids))+'H',*ids)+struct.pack('<'+str(len(hashes))+'I',*sorted(hashes)))
    result={'models':['islandlodmainland','islandlodbeach','islandlod1','islandlod2'],'texture_ids':len(ids),'untextured_face_hashes':len(hashes)}
    (workspace/'reports/backdrops.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result))

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('game',type=Path)
    stage(p.parse_args().game,Path(__file__).resolve().parents[1])
