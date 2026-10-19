#!/usr/bin/env python3
"""Losslessly compress each 16m distant tile for bounded ARM9 streaming."""
import struct,json
from pathlib import Path
from compact_assets import lz10,unpack
ROOT=Path(__file__).resolve().parents[1]
def compress(path):
 data=path.read_bytes()
 if data[:4] in (b'VFD2',b'VFO1'):return
 if data[:4]!=b'VFD1':raise ValueError('Not a distant mesh')
 count,=struct.unpack_from('<I',data,4);entries=list(struct.iter_unpack('<hhhHII',data[8:8+count*16]));blocks=[];maximum=rawmax=faces=0
 for number,(x,y,z,pad,offset,n) in enumerate(entries):
  tiles=[bytearray() for _ in range(64)]
  for i in range(n):
   record=data[offset+i*34:offset+(i+1)*34];coords=struct.unpack_from('<9h',record,4);tile=[min(3,max(0,int(sum(coords[k::3])/3)//1024)) for k in range(3)];tiles[tile[0]+tile[1]*4+tile[2]*16].extend(record)
  packed=[]
  for raw in tiles:
   if not raw:packed.append(b'');continue
   blob=lz10(bytes(raw))
   if unpack(blob)!=raw:raise ValueError('Compression mismatch')
   packed.append(blob);maximum=max(maximum,len(blob));rawmax=max(rawmax,len(raw));faces+=len(raw)//34
  blocks.append(struct.pack('<64I',*map(len,packed))+b''.join(packed))
  if number%400==0:print('Distant compression',number,count,flush=True)
 offset=16+count*16;index=[]
 for entry,block in zip(entries,blocks):index.append(struct.pack('<hhhHII',*entry[:4],offset,len(block)));offset+=len(block)
 path.write_bytes(struct.pack('<4sIII',b'VFD2',count,maximum,rawmax)+b''.join(index)+b''.join(blocks))
 report=json.loads((ROOT/'reports/distant.json').read_text());report.update({'uncompressed_bytes':len(data),'bytes':path.stat().st_size,'max_packed_tile':maximum,'max_raw_tile':rawmax,'lossless_faces':faces,'compression':'LZ10 per 16m tile; verified exact decode'})
 (ROOT/'reports/distant.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report),flush=True)
if __name__=='__main__':compress(ROOT/'sdcard/revc/world/distant.vcd')
