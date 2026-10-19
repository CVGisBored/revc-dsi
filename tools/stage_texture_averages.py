#!/usr/bin/env python3
"""Palette-weighted source texture colors for distant and cache-miss rendering."""
from pathlib import Path
import struct,numpy as np
ROOT=Path(__file__).resolve().parents[1]
def main():
 p=ROOT/'sdcard/revc/world/textures.vct';data=p.read_bytes();n,=struct.unpack_from('<I',data,4)
 offsets=struct.unpack_from('<'+str(n)+'I',data,12) if data[:4]==b'VTX2' else [8+i*4612 for i in range(n)]
 cache={};out=[]
 for offset in offsets:
  if offset not in cache:
   page=data[offset:offset+4612];palette=np.frombuffer(page,dtype='<u2',count=256,offset=4);pixels=np.frombuffer(page,dtype='u1',count=4096,offset=516);colors=palette[pixels]
   if struct.unpack_from('<I',page)[0]&1:colors=colors[pixels!=0]
   channels=[int(np.rint(((colors>>shift)&31).mean())) if len(colors) else 31 for shift in (0,5,10)];cache[offset]=sum(v<<s for v,s in zip(channels,(0,5,10)))
  out.append(cache[offset])
 (ROOT/'sdcard/revc/world/averages.vcc').write_bytes(struct.pack('<4sI',b'VTC1',n)+struct.pack('<'+str(n)+'H',*out));print('Texture averages',n,len(cache))
if __name__=='__main__':main()
