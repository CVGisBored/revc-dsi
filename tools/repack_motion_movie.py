#!/usr/bin/env python3
"""Repack a regenerated 30 fps VCV2 cache with the VCV3 motion codec."""
from pathlib import Path
import struct,json
from cinematic_codec import KEY,decode_frame,encode_motion,decode_motion

def repack(source,dest):
 data=source.read_bytes();h=list(struct.unpack_from('<4s4H7I',data));assert h[0]==b'VCV2' and h[1:4]==[128,72,30]
 start=40+h[4]*88;sizes=struct.unpack_from('<%dI'%h[5],data,start);audio_start=start+h[5]*4;p=audio_start+h[7];parts=[];encoded=[];original=None;previous=None
 for i,size in enumerate(sizes):
  size,key=size&~KEY,bool(size&KEY);raw=decode_frame(data[p:p+size],original,key);original=raw;p+=size
  packed,is_key,stable=encode_motion(raw,previous,i%30==0)
  assert decode_motion(packed,previous,is_key)==stable;previous=stable;parts.append(packed);encoded.append(len(packed)|(KEY if is_key else 0))
  if i%100==0:print(source.stem,i,h[5],flush=True)
 h[0]=b'VCV3';h[9]=max(map(len,parts));dest.parent.mkdir(parents=True,exist_ok=True);dest.write_bytes(struct.pack('<4s4H7I',*h)+data[40:start]+struct.pack('<%dI'%h[5],*encoded)+data[audio_start:audio_start+h[7]]+b''.join(parts))
 print(json.dumps({'name':source.stem,'before':len(data),'after':dest.stat().st_size,'fps':30}),flush=True)
if __name__=='__main__':
 import sys;repack(Path(sys.argv[1]),Path(sys.argv[2]))
