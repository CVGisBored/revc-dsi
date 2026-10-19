#!/usr/bin/env python3
"""Build bounded 8-metre runtime tiles while retaining the full source map pack."""
import heapq
import json
from pathlib import Path
import struct
import stage_world

LIMIT=64

def keep(heap,item):
    if len(heap)<LIMIT:
        heapq.heappush(heap,item)
    elif item[0]>heap[0][0]:
        heapq.heapreplace(heap,item)

def build(root,report,resume=False):
    stage_world.CELL=8
    source=(root/'index.vcw').read_bytes()
    entries=list(struct.iter_unpack('<hhhHII',source[20:]))
    index=[];offset=0;total=0;completed=0
    if resume and (root/'fast.bin').exists():
        size=(root/'fast.bin').stat().st_size
        with (root/'fast.bin').open('rb') as old:
            for x,y,z,_,_,_ in entries:
                old.seek(offset);header=old.read(512)
                if len(header)!=512:
                    break
                end=512;records_total=0;valid=True
                for count in header:
                    if count>LIMIT:
                        valid=False;break
                    end+=count*11;records_total+=count
                if not valid or offset+end>size:
                    break
                index.append(struct.pack('<hhhHII',x,y,z,0,offset,end))
                offset+=end;total+=records_total;completed+=1
        print(f'Resuming at {completed}/{len(entries)} chunks',flush=True)
    mode='r+b' if resume and (root/'fast.bin').exists() else 'wb'
    with (root/'chunks.bin').open('rb') as src,(root/'fast.bin').open(mode) as dst:
        dst.truncate(offset);dst.seek(offset)
        for number,(x,y,z,_,start,count) in enumerate(entries[completed:],completed):
            src.seek(start);data=src.read(count*22)
            tiles={};sequence=0
            for color,flags,*coords in struct.iter_unpack('<HH9h',data):
                if flags:
                    continue
                points=[tuple(v/64 for v in coords[i:i+3]) for i in (0,3,6)]
                for tx,ty,tz,tri in stage_world.cells(points):
                    if not all(0<=v<8 for v in (tx,ty,tz)):
                        continue  # Zero-area pieces on the outer boundary.
                    values=tuple(round(v*64) for p in tri for v in p)
                    a,b,c=[values[i:i+3] for i in (0,3,6)]
                    u=[b[k]-a[k] for k in range(3)];v=[c[k]-a[k] for k in range(3)]
                    n=(u[1]*v[2]-u[2]*v[1],u[2]*v[0]-u[0]*v[2],u[0]*v[1]-u[1]*v[0])
                    area=sum(k*k for k in n)
                    if not area:
                        continue
                    floor=n[1]*n[1]>=n[0]*n[0]+n[2]*n[2]
                    heaps=tiles.setdefault(tx+ty*8+tz*64,([],[]))
                    keep(heaps[int(floor)],(area,sequence,struct.pack('<H9h',color,*values)))
                    sequence+=1
            directory=[];payload=bytearray()
            for tile in range(512):
                walls,floors=tiles.get(tile,([],[]))
                walls=sorted(walls,reverse=True);floors=sorted(floors,reverse=True)
                selected=walls[:32]+floors[:32]
                selected+=sorted(walls[32:]+floors[32:],reverse=True)[:LIMIT-len(selected)]
                directory.append(bytes([len(selected)]))
                for _,_,record in selected:
                    color,*values=struct.unpack('<H9h',record)
                    base=(tile%8*512,(tile//8)%8*512,tile//64*512)
                    packed=bytes(round((value-base[k%3])/4) for k,value in enumerate(values))
                    payload.extend(struct.pack('<H',color)+packed)
                total+=len(selected)
            block=b''.join(directory)+payload
            index.append(struct.pack('<hhhHII',x,y,z,0,offset,len(block)))
            dst.write(block);offset+=len(block)
            if number%250==0:
                print(f'Fast tiles: {number+1}/{len(entries)} coarse chunks',flush=True)
    (root/'index.vcf').write_bytes(struct.pack('<4sIffI',b'VCF2',len(index),230.,-1250.,64)+b''.join(index))
    result={'coarse_chunks':len(index),'tile_metres':8,'triangles_per_tile':LIMIT,
            'runtime_triangles':total,'bytes':offset,'record_bytes':11,'coordinate_metres':1/16,'source_bytes':(root/'chunks.bin').stat().st_size,
            'limitations':['Largest surfaces prioritized; small decorative geometry is omitted',
                           'Full-detail map remains in chunks.bin; runtime uses simplified fast.bin']}
    report.write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps(result),flush=True)

if __name__=='__main__':
    workspace=Path(__file__).resolve().parents[1]
    import argparse
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--resume',action='store_true')
    build(workspace/'sdcard/revc/world',workspace/'reports/fast.json',parser.parse_args().resume)
