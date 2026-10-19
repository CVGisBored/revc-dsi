#!/usr/bin/env python3
"""Import original COL1 collision meshes independently of render simplification."""
import argparse
from collections import defaultdict
import json
import math
from pathlib import Path
import struct
from assets import Archive, instances, place, FormatError
import stage_world

def models(data):
    offset=0
    while offset+8<=len(data) and data[offset:offset+4]==b'COLL':
        size=struct.unpack_from('<I',data,offset+4)[0];body=data[offset+8:offset+8+size]
        if len(body)!=size or size<68:raise FormatError('Truncated COL1 model')
        name=body[:22].split(b'\0')[0].decode('ascii').lower();b=body[24:];p=40
        def count():
            nonlocal p
            n=struct.unpack_from('<H',b,p)[0];p+=4;return n
        vertices=[];faces=[]
        spheres=count()
        for _ in range(spheres):
            radius,x,y,z=struct.unpack_from('<4f',b,p);p+=20
            # Collision spheres become a circumscribed octahedron, never a hole.
            radius*=math.sqrt(3);start=len(vertices)
            vertices.extend([(x+radius,y,z),(x-radius,y,z),(x,y+radius,z),(x,y-radius,z),(x,y,z+radius),(x,y,z-radius)])
            faces.extend((start+a,start+c,start+d) for a in (0,1) for c in (2,3) for d in (4,5))
        lines=count();p+=lines*24
        boxes=count()
        for _ in range(boxes):
            lo=struct.unpack_from('<3f',b,p);hi=struct.unpack_from('<3f',b,p+12);p+=28;start=len(vertices)
            vertices.extend((x,y,z) for z in (lo[2],hi[2]) for y in (lo[1],hi[1]) for x in (lo[0],hi[0]))
            faces.extend(tuple(start+i for i in f) for f in [(0,1,3),(0,3,2),(4,6,7),(4,7,5),(0,4,5),(0,5,1),(2,3,7),(2,7,6),(0,2,6),(0,6,4),(1,5,7),(1,7,3)])
        n=count();start=len(vertices)
        vertices.extend(tuple(math.trunc(v*128)/128 for v in xyz) for xyz in struct.iter_unpack('<3f',b[p:p+n*12]));p+=n*12
        nt=count()
        for _ in range(nt):
            a,c,d=struct.unpack_from('<3I',b,p);p+=16
            if max(a,c,d)>=n:raise FormatError('Invalid COL1 face')
            faces.append((start+a,start+c,start+d))
        if p!=len(b):raise FormatError('COL1 length mismatch')
        yield name,(vertices,faces)
        offset+=8+size
    if any(data[offset:]):raise FormatError('Unexpected collision archive tail')

def stage(game,workspace):
    archive=Archive(game);definitions={}
    for name in sorted(x for x in archive.entries if x.endswith('.col')):
        definitions.update(models(archive.read(name)))
    definitions.update(models((game/'models/coll/generic.col').read_bytes()))
    stage_world.CELL=64;buckets=defaultdict(bytearray);missing=[];placed=triangles=0
    for obj in instances(game):
        if 'lod' in obj.name:continue
        if obj.name not in definitions:missing.append(obj.name);continue
        vertices,faces=definitions[obj.name];world=[place(obj,v) for v in vertices]
        world=[(v[0]-230,v[2],-(v[1]+1250)) for v in world];placed+=1
        for face in faces:
            points=[world[i] for i in face]
            for x,y,z,tri in stage_world.cells(points):
                coords=[round((v[k]-(x,y,z)[k]*64)*64) for v in tri for k in range(3)]
                buckets[x,y,z].extend(struct.pack('<H9h',0,*coords));triangles+=1
        if placed%1000==0:print(f'Original collision: {placed} placements, {triangles} clipped faces',flush=True)
    root=workspace/'sdcard/revc/world';index=[];offset=0
    with (root/'collision.bin').open('wb') as f:
        for x,y,z in sorted(buckets):
            payload=buckets[x,y,z];f.write(payload);index.append(struct.pack('<hhhHII',x,y,z,0,offset,len(payload)//20));offset+=len(payload)
    (root/'collision.vcc').write_bytes(struct.pack('<4sI',b'VCC1',len(index))+b''.join(index))
    from index_collision import build as index_blocks
    block_report=index_blocks(root)
    (workspace/'reports/collision-blocks.json').write_text(json.dumps(block_report,indent=2)+'\n')
    result={'models':len(definitions),'placements':placed,'chunks':len(index),'triangles':triangles,'bytes':offset,
            'missing_models':sorted(set(missing)), 'limitations':['Sphere primitives conservatively approximated by circumscribed octahedra; original triangle and box surfaces retained']}
    (workspace/'reports/collision.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps({k:v for k,v in result.items() if k!='missing_models'}),flush=True)

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('game',type=Path)
    stage(p.parse_args().game,Path(__file__).resolve().parents[1])
