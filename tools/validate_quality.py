#!/usr/bin/env python3
"""Validate textured geometry and compare collision records to the original pack."""
from pathlib import Path
from collections import Counter
import struct
import json

def validate(root,report):
    tex=(root/'textures.vct').read_bytes();magic,pages=struct.unpack_from('<4sI',tex)
    if not 0<pages<32768:raise ValueError('Invalid texture package')
    if magic==b'VTX1':unique=pages;start=8
    elif magic==b'VTX2':
        unique=struct.unpack_from('<I',tex,8)[0];start=12+pages*4
        offsets=struct.unpack_from('<'+str(pages)+'I',tex,12)
        if not 0<unique<=pages or any(x<start or x>=start+unique*4612 or (x-start)%4612 for x in offsets):raise ValueError('Invalid texture aliases')
    else:raise ValueError('Invalid texture package')
    if len(tex)!=start+unique*4612:raise ValueError('Texture length mismatch')
    for i in range(unique):
        flags,=struct.unpack_from('<I',tex,start+i*4612)
        if flags&~1 or any(c[0]>32767 for c in struct.iter_unpack('<H',tex[start+4+i*4612:start+516+i*4612])):
            raise ValueError('Invalid palette or texture flags')
    compact=(root/'index.vct').read_bytes()[:4]==b'VCF4'
    index=(root/('index-detail-full.vct' if compact else 'index.vct')).read_bytes();magic,n,x,z,cell=struct.unpack_from('<4sIffI',index)
    if (magic,x,z,cell)!=(b'VCF3',230.,-1250.,64) or len(index)!=20+n*16:raise ValueError('Invalid quality index')
    entries=list(struct.iter_unpack('<hhhHII',index[20:]));old=list(struct.iter_unpack('<hhhHII',(root/'index.vcf').read_bytes()[20:]))
    if [r[:3] for r in entries]!=[r[:3] for r in old]:raise ValueError('Original region coverage changed')
    total=solid=offset=0
    with (root/('textured-full.bin' if compact else 'textured.bin')).open('rb') as stream,(root/'fast.bin').open('rb') as legacy:
        for entry,reference in zip(entries,old):
            x,y,z,pad,start,length=entry
            if pad or start!=offset or length<512:raise ValueError('Invalid quality block offsets')
            block=stream.read(length);counts=block[:512]
            if len(block)!=length or max(counts)>128 or 512+sum(counts)*25!=length:raise ValueError('Tile budget or payload mismatch')
            legacy.seek(reference[4]);original=legacy.read(reference[5]);position=oldposition=512
            for number,count in enumerate(counts):
                selected=[]
                for j in range(count):
                    record=block[position:position+25];position+=25;color,id=struct.unpack_from('<HH',record)
                    if color>32767 or (id&0x7fff)>pages or max(record[4:13])>128:raise ValueError('Invalid material/geometry reference')
                    if id&0x8000:selected.append(record[4:13]);solid+=1
                expected=[original[oldposition+j*11+2:oldposition+j*11+11] for j in range(original[number])]
                oldposition+=original[number]*11
                if Counter(selected)!=Counter(expected):raise ValueError(f'Collision changed at {entry[:3]}, tile {number}')
            total+=sum(counts);offset+=length
        if stream.read(1):raise ValueError('Unexpected textured payload')
    info=json.loads(report.read_text())
    if total!=info['runtime_triangles'] or solid!=info['collision_triangles'] or offset!=info['bytes']:raise ValueError('Quality report mismatch')
    print(f'Historical material pack: {pages} texture pages; {n} map regions; {total} detail records; {solid} legacy collision tags unchanged')
    if compact:
        active=(root/'index.vct').read_bytes();records=offset=0
        if active[4:20]!=index[4:20]:raise ValueError('Compact map header changed')
        with (root/'textured.bin').open('rb') as stream:
            for x,y,z,pad,start,size in struct.iter_unpack('<hhhHII',active[20:]):
                if pad or start!=offset:raise ValueError('Compact map offsets changed')
                block=stream.read(size)
                if len(block)!=size or size!=512+sum(block[:512])*25 or max(block[:512])>128:raise ValueError('Invalid compact map payload')
                for color,id,*coords in struct.iter_unpack('<HH9B6h',block[512:]):
                    if color>32767 or (id&32767)>pages or max(coords[:9])>128:raise ValueError('Invalid compact map material')
                offset+=size;records+=sum(block[:512])
            if stream.read(1):raise ValueError('Extra compact map bytes')
        print(f'Active decorative map: {records} records, {unique} unique texture pages; independent terrain and COL supply ground/collision')

if __name__=='__main__':
    root=Path(__file__).resolve().parents[1];validate(root/'sdcard/revc/world',root/'reports/quality.json')
