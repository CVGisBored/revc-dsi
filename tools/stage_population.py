#!/usr/bin/env python3
"""Import detached original pedestrian/car path segments and bounded actor meshes."""
import argparse
import json
from pathlib import Path
import struct
import math
from assets import Archive,resolve,FormatError
from stage_animation import bake,ifp
from stage_gameplay import convert

def edges(game):
    rows=[line.split('#')[0].strip() for line in resolve(game,'data/maps/paths.ipl').read_text().splitlines()]
    rows=[r for r in rows if r];start=rows.index('path')+1;result=[]
    while rows[start]!='end':
        kind,model=map(int,rows[start].split(',')[:2]);start+=1
        nodes=[[float(x.strip()) for x in r.split(',')] for r in rows[start:start+12]];start+=12
        if model!=-1 or kind not in (0,1):continue
        for index,node in enumerate(nodes):
            nxt=int(node[1])
            if not int(node[0]) or not 0<=nxt<12 or not int(nodes[nxt][0]):continue
            if int(node[10])&1 or int(nodes[nxt][10])&1:continue
            def point(n):return (n[3]/16-230,n[5]/16,-(n[4]/16+1250))
            a,b=point(node),point(nodes[nxt])
            if sum((a[k]-b[k])**2 for k in range(3))<.25:continue
            result.append((kind,*a,*b))
    return result

def stage(game,workspace):
    root=workspace/'sdcard/revc/world';segments=edges(game)
    buckets={}
    for kind,*coords in segments:
        a,b=coords[:3],coords[3:];delta=[b[k]-a[k] for k in range(3)]
        for x in range(math.floor(min(a[0],b[0])/64),math.floor(max(a[0],b[0])/64)+1):
            for z in range(math.floor(min(a[2],b[2])/64),math.floor(max(a[2],b[2])/64)+1):
                low,high=0.,1.
                for k,cell in ((0,x),(2,z)):
                    if abs(delta[k])<1e-8:
                        if not cell*64<=a[k]<=cell*64+64:high=-1
                    else:
                        u,v=(cell*64-a[k])/delta[k],(cell*64+64-a[k])/delta[k]
                        low=max(low,min(u,v));high=min(high,max(u,v))
                if high-low<1e-5:continue
                p=[a[k]+delta[k]*low for k in range(3)];q=[a[k]+delta[k]*high for k in range(3)]
                buckets.setdefault((x,z),bytearray()).extend(struct.pack('<I6f',kind,*p,*q))
    offset=8+len(buckets)*12;index=[];payload=[]
    for (x,z),block in sorted(buckets.items()):
        index.append(struct.pack('<hhII',x,z,offset,len(block)//28));payload.append(block);offset+=len(block)
    (root/'paths.vcp').write_bytes(struct.pack('<4sI',b'VPT2',len(index))+b''.join(index)+b''.join(payload))
    archive=Archive(game);animations=ifp((game/'anim/ped.ifp').read_bytes())
    animations.update(ifp(archive.read('colt45.ifp')))
    actors=[bake(archive,animations,name,80,root/(name+'.vca')) for name in ('male01','hfyst')]
    car=convert(archive,'taxi',root/'traffic.vcm',420,.2,'car')
    r={'path_segments':len(segments),'ped_segments':sum(s[0]==0 for s in segments),'car_segments':sum(s[0]==1 for s in segments),
       'actors':actors,'traffic':car,'limitations':['Bounded population; no full upstream population, traffic-light or police simulation']}
    (workspace/'reports/population.json').write_text(json.dumps(r,indent=2)+'\n');print(json.dumps({k:v for k,v in r.items() if k not in ('actors',)}))

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('game',type=Path)
    stage(p.parse_args().game,Path(__file__).resolve().parents[1])
