#!/usr/bin/env python3
"""Retain original map materials/UVs and add a bounded, textured detail layer."""
import argparse
from collections import defaultdict
import heapq
import json
import math
from pathlib import Path
import struct
from assets import Archive,instances,place,timed_models,visible_at,FormatError
from materials import detailed_dff,model_textures,TextureRegistry
import stage_world

SOURCE=struct.Struct('<HHH9h6f')
PACKED=struct.Struct('<HH9B6h')
LIMIT=128

def source(game,root,report):
    stage_world.CELL=64
    archive=Archive(game);mapping=model_textures(game);textures=TextureRegistry(game,archive)
    objects=instances(game);timed=timed_models(game);groups=defaultdict(list)
    for obj in objects:groups[obj.name].append(obj)
    buckets=defaultdict(bytearray);exported=0;triangles=0
    for number,(name,placements) in enumerate(sorted(groups.items()),1):
        geometry=detailed_dff(archive.read(name+'.dff'))
        for obj in placements:
            flags=(1 if 'lod' in name else 0)|(2 if obj.model_id in timed and not visible_at(timed[obj.model_id],12) else 0)
            for vertices,faces,uv,prelight,materials in geometry:
                world=[place(obj,p) for p in vertices];world=[(p[0]-230,p[2],-(p[1]+1250)) for p in world]
                ids=[textures.get(mapping.get(obj.model_id,'generic'),name) for _,name in materials]
                for a,b,c,material in faces:
                    indices=(a,b,c);points=[(*world[i],*uv[i]) for i in indices]
                    u=[points[1][k]-points[0][k] for k in range(3)];v=[points[2][k]-points[0][k] for k in range(3)]
                    cross=(u[1]*v[2]-u[2]*v[1],u[2]*v[0]-u[0]*v[2],u[0]*v[1]-u[1]*v[0])
                    if sum(n*n for n in cross)<1e-10:continue
                    rgba,_=materials[material]
                    channels=[min(31,round(rgba[k]*sum(prelight[i][k] for i in indices)/3/255/255*31)) for k in range(3)]
                    color=channels[0]|channels[1]<<5|channels[2]<<10;triangles+=1
                    for x,y,z,tri in stage_world.cells(points):
                        coords=tuple(round((p[k]-(x,y,z)[k]*64)*64) for p in tri for k in range(3))
                        texcoords=tuple(p[k] for p in tri for k in (3,4))
                        buckets[x,y,z].extend(SOURCE.pack(color,ids[material],flags,*coords,*texcoords))
            exported+=1
        if number%100==0:print(f'Original materials: {number}/{len(groups)} models; {exported}/{len(objects)} placements; {len(textures.records)} textures',flush=True)
    # Keep the exact original region index, including inactive LOD/night regions.
    original=(root/'index.vcw').read_bytes();keys=[entry[:3] for entry in struct.iter_unpack('<hhhHII',original[20:])]
    if set(keys)!=set(buckets):raise FormatError('Material conversion changes original map coverage')
    offset=0;index=[]
    with (root/'quality-source.bin').open('wb') as file:
        for x,y,z in keys:
            data=buckets[x,y,z];file.write(data);index.append(struct.pack('<hhhHII',x,y,z,0,offset,len(data)//SOURCE.size));offset+=len(data)
    (root/'index.vcq').write_bytes(struct.pack('<4sIffI',b'VCQ1',len(keys),230.,-1250.,64)+b''.join(index))
    textures.write(root/'textures.vct')
    result={'placements_total':len(objects),'placements_converted':exported,'coarse_chunks':len(keys),
            'source_triangles':triangles,'source_bytes':offset,'texture_size':64,'texture_colors':256,
            'textures':textures.report,'missing_textures':[list(x) for x in sorted(textures.missing)]}
    report.write_text(json.dumps(result,indent=2)+'\n')
    print(f'Original material source complete: {exported} placements, {len(textures.records)} textures, {len(textures.missing)} unresolved references',flush=True)

def retained(walls,floors,limit):
    half=limit//2;result=walls[:half]+floors[:half]
    return result+sorted(walls[half:]+floors[half:],reverse=True)[:limit-len(result)]

def build(root,report,resume=False,max_chunks=0):
    stage_world.CELL=8
    entries=list(struct.iter_unpack('<hhhHII',(root/'index.vcq').read_bytes()[20:]));index=[];offset=total=solid=completed=uv_fallback=0
    output=root/'textured.bin'
    if resume and output.exists():
        with output.open('rb') as old:
            length=output.stat().st_size
            for x,y,z,_,_,_ in entries:
                old.seek(offset);header=old.read(512)
                if len(header)!=512 or max(header)>LIMIT:break
                size=512+sum(header)*PACKED.size
                if offset+size>length:break
                payload=old.read(size-512)
                index.append(struct.pack('<hhhHII',x,y,z,0,offset,size));offset+=size;total+=sum(header)
                solid+=sum(bool(record[1]&0x8000) for record in PACKED.iter_unpack(payload));completed+=1
        print(f'Resume textured map: {completed}/{len(entries)}',flush=True)
    mode='r+b' if output.exists() and resume else 'wb'
    with (root/'quality-source.bin').open('rb') as src,output.open(mode) as dst:
        dst.truncate(offset);dst.seek(offset)
        stop=min(len(entries),completed+max_chunks) if max_chunks else len(entries)
        for number,(x,y,z,_,start,count) in enumerate(entries[completed:stop],completed):
            src.seek(start);data=src.read(count*SOURCE.size);tiles={};sequence=0
            for color,texture,flags,*values in SOURCE.iter_unpack(data):
                if flags:continue
                coords=values[:9];uv=values[9:]
                points=[(*[v/64 for v in coords[i*3:i*3+3]],*uv[i*2:i*2+2]) for i in range(3)]
                for tx,ty,tz,tri in stage_world.cells(points):
                    if not all(0<=v<8 for v in (tx,ty,tz)):continue
                    positions=tuple(round(v*64) for p in tri for v in p[:3])
                    a,b,c=[positions[i:i+3] for i in (0,3,6)]
                    u=[b[k]-a[k] for k in range(3)];v=[c[k]-a[k] for k in range(3)]
                    n=(u[1]*v[2]-u[2]*v[1],u[2]*v[0]-u[0]*v[2],u[0]*v[1]-u[1]*v[0]);area=sum(k*k for k in n)
                    if not area:continue
                    floor=n[1]*n[1]>=n[0]*n[0]+n[2]*n[2]
                    phase=[math.floor((min(p[k] for p in tri)+max(p[k] for p in tri))/2) for k in (3,4)]
                    texcoords=tuple(round((p[k]-phase[k-3])*1024) for p in tri for k in (3,4))
                    texid=texture
                    if any(not -32768<=v<=32767 for v in texcoords):texid=0;texcoords=(0,)*6;uv_fallback+=1
                    base=(tx*512,ty*512,tz*512)
                    packed=tuple(round((v-base[k%3])/4) for k,v in enumerate(positions))
                    record=PACKED.pack(color,texid,*packed,*texcoords)
                    heaps=tiles.setdefault(tx+ty*8+tz*64,([],[],[],[]));item=(area,sequence,record)
                    for heap,limit in ((heaps[int(floor)],64),(heaps[2+int(floor)],LIMIT)):
                        if len(heap)<limit:heapq.heappush(heap,item)
                        elif item[0]>heap[0][0]:heapq.heapreplace(heap,item)
                    sequence+=1
            directory=bytearray();payload=bytearray()
            for tile in range(512):
                oldwalls,oldfloors,walls,floors=tiles.get(tile,([],[],[],[]))
                base=retained(sorted(oldwalls,reverse=True),sorted(oldfloors,reverse=True),64);base_ids={r[1] for r in base}
                detail=retained(sorted(walls,reverse=True),sorted(floors,reverse=True),LIMIT)
                selected=base+[r for r in detail if r[1] not in base_ids][:LIMIT-len(base)]
                if len(selected)>LIMIT:raise FormatError('Detail tile overflow')
                directory.append(len(selected))
                for _,sequence,record in selected:
                    if sequence in base_ids:
                        record=record[:2]+struct.pack('<H',struct.unpack_from('<H',record,2)[0]|0x8000)+record[4:];solid+=1
                    payload.extend(record)
                total+=len(selected)
            block=directory+payload;index.append(struct.pack('<hhhHII',x,y,z,0,offset,len(block)));dst.write(block);offset+=len(block)
            if number%100==0:print(f'Textured detail: {number+1}/{len(entries)} chunks',flush=True)
    if len(index)!=len(entries):
        print(f'Batch saved: {len(index)}/{len(entries)}; resume to complete',flush=True);return
    (root/'index.vct').write_bytes(struct.pack('<4sIffI',b'VCF3',len(index),230.,-1250.,64)+b''.join(index))
    result={'coarse_chunks':len(index),'tile_metres':8,'triangles_per_tile':LIMIT,'record_bytes':PACKED.size,
            'runtime_triangles':total,'collision_triangles':solid,'bytes':offset,'last_batch_uv_overflow_fallbacks':uv_fallback,
            'limitations':['Texture resolution reduced to 64x64 and 256 colors','Up to 128 original triangles retained per 8m tile; nearest resident/draw caps still apply',
                           'Partial texture alpha approximated by cutouts; original COL, water simulation and distant LOD remain unintegrated']}
    report.write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result),flush=True)

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('game',type=Path);p.add_argument('--resume',action='store_true')
    p.add_argument('--source-only',action='store_true');p.add_argument('--max-chunks',type=int,default=0);args=p.parse_args()
    workspace=Path(__file__).resolve().parents[1];root=workspace/'sdcard/revc/world'
    if not args.resume or not (root/'quality-source.bin').exists():source(args.game,root,workspace/'reports/materials.json')
    if not args.source_only:build(root,workspace/'reports/quality.json',args.resume,args.max_chunks)
