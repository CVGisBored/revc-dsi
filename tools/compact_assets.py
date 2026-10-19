#!/usr/bin/env python3
"""Losslessly pack terrain tiles and share identical texture pages."""
from collections import deque
from pathlib import Path
import struct
import json

def lz10(data):
    out=bytearray(struct.pack('<I',len(data)<<8|0x10));history={};position=0
    while position<len(data):
        flagpos=len(out);out.append(0);flags=0
        for bit in range(8):
            if position>=len(data):break
            key=data[position:position+3];best=0;back=0
            if len(key)==3:
                for old in reversed(history.get(key,())):
                    if position-old>4096:break
                    n=3
                    while n<18 and position+n<len(data) and data[old+n]==data[position+n]:n+=1
                    if n>best:best=n;back=position-old
                    if best==18:break
            amount=best if best>=3 else 1
            if best>=3:
                flags|=1<<(7-bit);value=(best-3)<<12|(back-1);out.extend((value>>8,value&255))
            else:out.append(data[position])
            for p in range(position,position+amount):
                key=data[p:p+3]
                if len(key)==3:history.setdefault(key,deque(maxlen=32)).append(p)
            position+=amount
        out[flagpos]=flags
    return bytes(out)

def unpack(data):
    header=struct.unpack_from('<I',data)[0]
    if header&255!=0x10:raise ValueError('Not LZ10')
    size=header>>8;result=bytearray();p=4
    while len(result)<size:
        flag=data[p];p+=1
        for bit in range(8):
            if len(result)==size:break
            if flag&(1<<(7-bit)):
                value=data[p]<<8|data[p+1];p+=2;length=(value>>12)+3;distance=(value&4095)+1
                if distance>len(result) or len(result)+length>size:raise ValueError('Invalid LZ10 backreference')
                for _ in range(length):result.append(result[-distance])
            else:result.append(data[p]);p+=1
    if p!=len(data):raise ValueError('Unexpected LZ10 tail')
    return bytes(result)

def textures(root):
    target=root/'textures.vct';raw=target.read_bytes();magic,count=struct.unpack_from('<4sI',raw)
    if magic==b'VTX2':return target.stat().st_size
    if magic!=b'VTX1' or len(raw)!=8+count*4612:raise ValueError('Invalid texture source')
    unique={};pages=[];offsets=[];start=12+count*4
    for i in range(count):
        page=raw[8+i*4612:8+(i+1)*4612]
        if page not in unique:unique[page]=len(pages);pages.append(page)
        offsets.append(start+unique[page]*4612)
    output=struct.pack('<4sII',b'VTX2',count,len(pages))+struct.pack('<'+str(count)+'I',*offsets)+b''.join(pages)
    (root/'textures-compact.vct').write_bytes(output);target.rename(root/'textures-raw.vct');(root/'textures-compact.vct').rename(target)
    print(f'Textures: {count} IDs share {len(pages)} pages; {len(raw)} -> {len(output)} bytes',flush=True)
    return len(output)

def terrain(root):
    idx=(root/'terrain.vct').read_bytes()
    if idx[:4]==b'VTR3':return
    if idx[:4]!=b'VTR2':raise ValueError('Expected tiled terrain')
    index=[];offset=largest=compressedLargest=total=0
    with (root/'terrain.bin').open('rb') as source,(root/'terrain-compressed.bin').open('wb') as out:
        for number,(x,y,z,_,start,size) in enumerate(struct.iter_unpack('<hhhHII',idx[8:])):
            source.seek(start);counts=struct.unpack('<512H',source.read(1024));directory=[];payload=[]
            for count in counts:
                if not count:directory.append(0);continue
                raw=source.read(count*34);compressed=lz10(raw)
                if unpack(compressed)!=raw:raise ValueError('Terrain compression changes geometry')
                largest=max(largest,len(raw));compressedLargest=max(compressedLargest,len(compressed))
                directory.append(len(compressed));payload.append(compressed);total+=count
            block=struct.pack('<512I',*directory)+b''.join(payload)
            index.append(struct.pack('<hhhHII',x,y,z,0,offset,len(block)));out.write(block);offset+=len(block)
            if number%250==0:print(f'Lossless terrain: {number+1}/{(len(idx)-8)//16}',flush=True)
    (root/'terrain.bin').rename(root/'terrain-raw.bin');(root/'terrain-compressed.bin').rename(root/'terrain.bin')
    (root/'terrain.vct').rename(root/'terrain-raw.vct')
    (root/'terrain.vct').write_bytes(struct.pack('<4sIII',b'VTR3',len(index),compressedLargest,largest)+b''.join(index))
    info={'chunks':len(index),'triangles':total,'bytes':offset,'compression':'LZ10 per tile, lossless',
          'max_compressed_tile':compressedLargest,'max_raw_tile':largest,'per_tile_triangle_limit':None}
    (root.parents[2]/'reports/terrain.json').write_text(json.dumps(info,indent=2)+'\n');print(json.dumps(info),flush=True)

def decorations(root):
    idx=(root/'index.vct').read_bytes()
    if idx[:4]==b'VCF4':return
    if idx[:4]!=b'VCF3':raise ValueError('Expected original detailed map')
    entries=[];offset=kept=removed=0
    with (root/'textured.bin').open('rb') as source,(root/'decorations.bin').open('wb') as out:
        for x,y,z,_,start,size in struct.iter_unpack('<hhhHII',idx[20:]):
            source.seek(start);block=source.read(size);counts=block[:512];p=512;directory=[];parts=[]
            for count in counts:
                payload=bytearray()
                for i in range(count):
                    record=block[p:p+25];p+=25;a,b,c=[record[k:k+3] for k in (4,7,10)]
                    u=[b[k]-a[k] for k in range(3)];v=[c[k]-a[k] for k in range(3)]
                    nx=u[1]*v[2]-u[2]*v[1];ny=u[2]*v[0]-u[0]*v[2];nz=u[0]*v[1]-u[1]*v[0]
                    if ny and ny*ny>=nx*nx+nz*nz:removed+=1
                    else:payload.extend(record);kept+=1
                directory.append(len(payload)//25);parts.append(payload)
            new=bytes(directory)+b''.join(parts);out.write(new);entries.append(struct.pack('<hhhHII',x,y,z,0,offset,len(new)));offset+=len(new)
    (root/'textured.bin').rename(root/'textured-full.bin');(root/'decorations.bin').rename(root/'textured.bin')
    (root/'index.vct').rename(root/'index-detail-full.vct')
    (root/'index.vct').write_bytes(b'VCF4'+idx[4:20]+b''.join(entries))
    info={'records':kept,'bytes':offset,'redundant_ground_records_removed':removed,'ground_source':'uncapped terrain pack'}
    (root.parents[2]/'reports/decorations.json').write_text(json.dumps(info,indent=2)+'\n');print(json.dumps(info),flush=True)

if __name__=='__main__':
    root=Path(__file__).resolve().parents[1]/'sdcard/revc/world';textures(root);terrain(root);decorations(root)
