#!/usr/bin/env python3
"""Convert all IPL placements into clipped 64-metre chunks and a player mesh."""
import argparse
from collections import defaultdict
import json
import math
from pathlib import Path
import struct
from assets import Archive, FormatError, dff, instances, place, timed_models, visible_at
from prepare import face_color

CELL = 64
RECORD = struct.Struct('<HH9h')  # RGB15, inactive flags, local GPU positions


def clip(poly, axis, boundary, keep_high):
    result = []
    for a, b in zip(poly, poly[1:] + poly[:1]):
        inside_a = a[axis] >= boundary if keep_high else a[axis] <= boundary
        inside_b = b[axis] >= boundary if keep_high else b[axis] <= boundary
        if inside_a:
            result.append(a)
        if inside_a != inside_b:
            fraction = (boundary-a[axis])/(b[axis]-a[axis])
            result.append(tuple(a[k]+fraction*(b[k]-a[k]) for k in range(len(a))))
    return result


def cells(points):
    ranges=[range(math.floor(min(p[k] for p in points)/CELL),
                  math.floor(max(p[k] for p in points)/CELL)+1) for k in range(3)]
    for x in ranges[0]:
        for y in ranges[1]:
            for z in ranges[2]:
                poly=points
                for axis,low in enumerate((x*CELL,y*CELL,z*CELL)):
                    poly=clip(poly,axis,low,True)
                    poly=clip(poly,axis,low+CELL,False)
                    if len(poly)<3:
                        break
                for i in range(1,len(poly)-1):
                    tri=[poly[0],poly[i],poly[i+1]]
                    u=[tri[1][k]-tri[0][k] for k in range(3)]
                    v=[tri[2][k]-tri[0][k] for k in range(3)]
                    cross=(u[1]*v[2]-u[2]*v[1],u[2]*v[0]-u[0]*v[2],u[0]*v[1]-u[1]*v[0])
                    if sum(n*n for n in cross)>1e-8:
                        yield x,y,z,tri


def player(archive, output):
    groups=dff(archive.read('player.dff'))
    vertices=[p for v,_ in groups for p in v]
    # The decoded player's root frame already presents its vertical axis as Y.
    base=min(p[1] for p in vertices)
    records=[]; seen=set()
    for vertices,triangles in groups:
        clusters={}; remap=[]
        for p in vertices:
            key=tuple(round(k/.1) for k in p)
            if key not in clusters:
                clusters[key]=tuple(k*.1 for k in key)
            remap.append(clusters[key])
        for indices in triangles:
            points=[remap[i] for i in indices]
            if len(set(points))!=3:
                continue
            coords=tuple(round(v*64) for p in points for v in (p[0],p[1]-base,p[2]))
            if coords in seen:
                continue
            seen.add(coords)
            h=sum(p[1]-base for p in points)/3
            color=(25|18<<5|12<<10) if h>1.45 else (6|12<<5|24<<10) if h<.8 else (8|25<<5|22<<10)
            records.append(struct.pack('<H9h',color,*coords))
    if not 0<len(records)<=550:
        raise FormatError(f'Player exceeds reserved 550-polygon budget: {len(records)}')
    (output/'player.vcp').write_bytes(struct.pack('<4sI',b'VCP1',len(records))+b''.join(records))
    return len(records)


def stage(game):
    workspace=Path(__file__).resolve().parents[1]
    output=workspace/'sdcard/revc/world'
    output.mkdir(parents=True,exist_ok=True)
    archive=Archive(game); objects=instances(game); timed=timed_models(game)
    buckets=defaultdict(bytearray); groups=defaultdict(list); skipped=[]
    for obj in objects:
        groups[obj.name].append(obj)
    exported=0; source_triangles=0; active_triangles=0
    for number,(name,placements) in enumerate(sorted(groups.items()),1):
        try:
            geometry=dff(archive.read(name+'.dff'))
            if not geometry:
                raise FormatError('No visible atomics')
            for obj in placements:
                flags=(1 if 'lod' in name else 0) | (2 if obj.model_id in timed and
                       not visible_at(timed[obj.model_id],12) else 0)
                for vertices,indices in geometry:
                    world=[place(obj,p) for p in vertices]
                    world=[(p[0]-230,p[2],-(p[1]+1250)) for p in world]
                    for indices3 in indices:
                        points=[world[i] for i in indices3]
                        color=face_color(points,obj.model_id)
                        if color is None:
                            continue
                        source_triangles+=1
                        for x,y,z,tri in cells(points):
                            coords=tuple(round(v*64) for p in tri for v in (p[0]-x*CELL,p[1]-y*CELL,p[2]-z*CELL))
                            if any(not -32768<=v<=32767 for v in coords):
                                raise FormatError('Chunk height exceeds GPU coordinate range')
                            buckets[(x,y,z)].extend(RECORD.pack(color,flags,*coords))
                            active_triangles+=not flags
                exported+=1
        except (KeyError,FormatError) as error:
            skipped.append({'name':name,'placements':len(placements),'reason':str(error)})
        if number%100==0:
            print(f'Map conversion: {number}/{len(groups)} models, {exported}/{len(objects)} placements',flush=True)
    index=[]; offset=0
    with (output/'chunks.bin').open('wb') as file:
        for (x,y,z),data in sorted(buckets.items()):
            count=len(data)//RECORD.size
            index.append(struct.pack('<hhhHII',x,y,z,0,offset,count))
            file.write(data); offset+=len(data)
    (output/'index.vcw').write_bytes(struct.pack('<4sIffI',b'VCW1',len(index),230.,-1250.,CELL)+b''.join(index))
    player_count=player(archive,output)
    report={'placements_total':len(objects),'placements_converted':exported,'skipped':skipped,
            'chunks':len(index),'source_triangles':source_triangles,'active_chunk_triangles':active_triangles,
            'chunk_bytes':offset,'player_triangles':player_count,'cell_metres':CELL,
            'origin':[230,-1250], 'runtime_hour':12,
            'limitations':['Untextured static render geometry; not original COL files',
                           'LOD and nighttime alternatives are packaged but inactive at runtime',
                           'Player mesh is simplified, static bind pose; no original animation or skinning']}
    (workspace/'reports/world.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps({k:v for k,v in report.items() if k not in ('skipped','limitations')}),flush=True)
    print(f'Skipped models: {len(skipped)}; see reports/world.json',flush=True)


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('game',type=Path)
    stage(parser.parse_args().game.resolve())
