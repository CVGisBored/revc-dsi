#!/usr/bin/env python3
"""Cross-check every packaged video frame with the production C++ decoder."""
import argparse,ctypes,hashlib,json,struct,subprocess
from pathlib import Path
from cinematic_codec import KEY,decode_frame,decode_motion
from compact_assets import unpack
ROOT=Path(__file__).resolve().parents[1]
def validate(root):
 work=ROOT/'build/cinematic-validation';work.mkdir(parents=True,exist_ok=True)
 wrapper=work/'wrapper.cpp';wrapper.write_text('#include "cinematic_frame.h"\nextern "C" bool decode(unsigned c,unsigned w,unsigned h,bool k,const unsigned char* p,size_t n,const unsigned char* old,unsigned char* out,unsigned char* scratch){return slice::decodeCinematicFrame(c,w,h,k,p,n,old,out,scratch);}\n')
 library=work/'decoder.so';subprocess.run(['g++','-std=c++11','-O2','-shared','-fPIC','-I'+str(ROOT/'ports/nds'),str(wrapper),str(ROOT/'ports/nds/cinematic_frame.cpp'),str(ROOT/'ports/nds/packed.cpp'),'-o',str(library)],check=True)
 decoder=ctypes.CDLL(str(library)).decode;decoder.argtypes=[ctypes.c_uint,ctypes.c_uint,ctypes.c_uint,ctypes.c_bool,ctypes.c_void_p,ctypes.c_size_t,ctypes.c_void_p,ctypes.c_void_p,ctypes.c_void_p];decoder.restype=ctypes.c_bool
 result=[]
 for path in sorted((root/'cutscenes').glob('*.vcv')):
  data=path.read_bytes();h=struct.unpack_from('<4s4H7I',data);magic,w,height,fps,subs,frames,rate,audio,duration,maximum,rawbytes,stride=h;codec=int(chr(magic[-1]));assert frames==(duration*fps+999)//1000 and rawbytes==512+w*height
  start=40+subs*stride;sizes=struct.unpack_from('<%dI'%frames,data,start);audio_start=start+frames*4;position=audio_start+audio
  old=(ctypes.c_ubyte*rawbytes)();out=(ctypes.c_ubyte*rawbytes)();scratch=(ctypes.c_ubyte*(rawbytes+144))();reference=None;digest=hashlib.sha256();chain=0
  for i,size in enumerate(sizes):
   key=codec==1 or bool(size&KEY);size&=~KEY;packed=data[position:position+size];position+=size;assert 4<=size<=maximum
   chain=0 if key else chain+1;assert key or (i>0 and chain<30)
   source=ctypes.create_string_buffer(packed);assert decoder(codec,w,height,key,source,size,old if i else None,out,scratch),(path.name,i)
   reference=unpack(packed) if codec==1 else decode_frame(packed,reference,key) if codec==2 else decode_motion(packed,reference,key)
   actual=bytes(out);assert actual==reference,(path.name,i,'C++/Python decoded pixels differ');digest.update(actual);old,out=out,old
  assert position==len(data)
  backup=ROOT/'build/cinematics-six-fps'/path.name
  pcm=data[audio_start:audio_start+audio]
  if backup.exists():
   original=backup.read_bytes();v=struct.unpack_from('<4s4H7I',original);a=40+v[4]*v[11]+v[5]*4
   assert pcm==original[a:a+v[7]],(path.name,'Speech changed')
   assert data[40:40+subs*stride]==original[40:40+v[4]*v[11]],(path.name,'Subtitle timing changed')
  entry={'name':path.stem,'codec':magic.decode(),'fps':fps,'frames':frames,'bytes':len(data),'duration_ms':duration,'decoded_video_sha256':digest.hexdigest(),'speech_sha256':hashlib.sha256(pcm).hexdigest(),'sha256':hashlib.sha256(data).hexdigest()};result.append(entry);print(path.stem,frames,'frames:',fps,'fps; C++/Python pixels match',flush=True)
 (ROOT/'reports/cinematic-decoder-validation.json').write_text(json.dumps(result,indent=2)+'\n');return result
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('root',type=Path);validate(p.parse_args().root)
