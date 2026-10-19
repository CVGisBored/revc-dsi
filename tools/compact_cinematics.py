#!/usr/bin/env python3
"""Repack cached cinematics at 6 fps, preserving full speech and subtitle timing."""
from pathlib import Path
import struct,math,shutil,json
import numpy as np
from PIL import Image
from compact_assets import unpack,lz10
ROOT=Path(__file__).resolve().parents[1]
def compact(path):
 backup=ROOT/'build/cinematic-originals'/path.name;backup.parent.mkdir(exist_ok=True,parents=True)
 if not backup.exists():shutil.copy2(path,backup)
 data=backup.read_bytes()
 if data[:4]!=b'VCV1':raise ValueError('Legacy repacker only accepts VCV1; regenerate VCV3 from original source animation')
 h=list(struct.unpack_from('<4s4H7I',data));_,w,height,fps,subs,frames,rate,audio,duration,maximum,raw,stride=h
 start=40+subs*88;ss=data[40:start];sizes=struct.unpack_from('<%dI'%frames,data,start);audio_start=start+frames*4;pcm=data[audio_start:audio_start+audio];offsets=[audio_start+audio]
 for size in sizes:offsets.append(offsets[-1]+size)
 count=math.ceil(duration*6/1000);parts=[]
 for i in range(count):
  f=min(frames-1,i*fps//6);r=unpack(data[offsets[f]:offsets[f+1]])
  pal=np.frombuffer(r[:512],dtype='<u2');ix=np.frombuffer(r[512:],dtype='u1').reshape(height,w)
  colors=np.stack(((pal&31)*255//31,((pal>>5)&31)*255//31,((pal>>10)&31)*255//31),axis=1).astype('u1')
  im=Image.fromarray(colors[ix]).resize((128,72),Image.Resampling.BOX).quantize(colors=256)
  rgb=np.array(im.getpalette(),dtype='uint16').reshape(-1,3);pal=((rgb[:,0]>>3)|((rgb[:,1]>>3)<<5)|((rgb[:,2]>>3)<<10)|0x8000).astype('<u2').tobytes().ljust(512,b'\0');r=pal+im.tobytes();b=lz10(r);assert unpack(b)==r;parts.append(b)
  if i%100==0:print(path.name,i,count,flush=True)
 h[1]=128;h[2]=72;h[3]=6;h[5]=count;h[10]=9728;h[9]=max(map(len,parts));path.write_bytes(struct.pack('<4s4H7I',*h)+ss+struct.pack('<%dI'%count,*map(len,parts))+pcm+b''.join(parts))
 return {'name':path.name,'before':len(data),'after':path.stat().st_size,'fps':6,'speech_and_timing':'unchanged','image':'128x72 doubled to 256x144'}
if __name__=='__main__':
 import argparse
 parser=argparse.ArgumentParser(description=__doc__)
 parser.add_argument('--only',nargs='+',choices=('int_a','int_b','int_d','int_m','departure','logo','gtatitles','law_1a','law_2a','law_2b','law_2c','law_3','law_4','tex_1'))
 args=parser.parse_args()
 result=[]
 for name in args.only or ('int_a','int_b','int_d','int_m','departure','logo','gtatitles','law_1a','law_2a','law_2b','law_2c','law_3','law_4','tex_1'):
  result.append(compact(ROOT/'sdcard/revc/cutscenes'/(name+'.vcv')))
 (ROOT/'reports/compact-cinematics.json').write_text(json.dumps(result,indent=2)+'\n')
