#!/usr/bin/env python3
"""Stage owned PC frontend artwork and complete startup movies for the DS."""
import argparse,json,math,struct,subprocess
from pathlib import Path
import numpy as np
from PIL import Image
from assets import resolve
from materials import txd
from compact_assets import lz10,unpack
ROOT=Path(__file__).resolve().parents[1]

def movie(game,name,dest):
    source=resolve(game,'movies/'+name+'.mpg');fps=6
    duration=float(subprocess.check_output(['ffprobe','-v','error','-show_entries','format=duration','-of','csv=p=0',str(source)]))
    frames=math.ceil(duration*fps)
    # Native 128x72 frames are doubled for a 256x144 letterbox. Six FPS and
    # bounded LZ10 frames keep both complete movies below the ROM size limit.
    process=subprocess.Popen(['ffmpeg','-v','error','-i',str(source),'-vf',f'fps={fps},scale=128:72','-f','rawvideo','-pix_fmt','rgb24','-'],stdout=subprocess.PIPE)
    parts=[]
    while True:
        raw=process.stdout.read(128*72*3)
        if not raw:break
        if len(raw)!=128*72*3:raise ValueError('Truncated movie frame')
        image=Image.frombytes('RGB',(128,72),raw).quantize(colors=128,method=0)
        rgb=np.asarray(image.getpalette(),dtype=np.uint16).reshape(-1,3)
        palette=((rgb[:,0]>>3)|((rgb[:,1]>>3)<<5)|((rgb[:,2]>>3)<<10)|0x8000).astype('<u2').tobytes().ljust(512,b'\0')
        raw=palette+image.resize((256,144),Image.Resampling.NEAREST).tobytes();packed=lz10(raw)
        if unpack(packed)!=raw:raise ValueError('Movie compression differs')
        parts.append(packed)
        if len(parts)%100==0:print(name,len(parts),frames,flush=True)
    if process.wait()!=0 or not parts:raise ValueError('Movie decode failed')
    parts=parts[:frames]+[parts[-1]]*max(0,frames-len(parts))
    pcm=subprocess.check_output(['ffmpeg','-v','error','-i',str(source),'-f','s8','-ac','1','-ar','16384','-'])
    pcm=pcm[:math.ceil(duration*16384)].ljust(math.ceil(duration*16384),b'\0')
    header=struct.pack('<4s4H7I',b'VCV1',256,144,fps,0,frames,16384,len(pcm),math.ceil(duration*1000),max(map(len,parts)),37376,88)
    dest.write_bytes(header+struct.pack('<'+str(frames)+'I',*map(len,parts))+pcm+b''.join(parts))
    return dict(source=str(source),duration=duration,frames=frames,fps=fps,bytes=dest.stat().st_size,resolution=[128,72])

def stage(game):
    out=ROOT/'sdcard/revc/cutscenes';out.mkdir(exist_ok=True)
    textures=txd(resolve(game,'models/fronten1.txd').read_bytes())
    image=textures['background'].convert('RGB').resize((256,192),Image.Resampling.LANCZOS)
    logo=textures['vc_logo'].convert('RGBA');logo.thumbnail((144,144),Image.Resampling.LANCZOS)
    image.paste(logo,((256-logo.width)//2,14),logo)
    image.save(ROOT/'reports/frontend-art.png')
    pixels=np.asarray(image,dtype=np.uint16)
    pixels=((pixels[:,:,0]>>3)|((pixels[:,:,1]>>3)<<5)|((pixels[:,:,2]>>3)<<10)|0x8000).astype('<u2')
    (out/'frontend.bin').write_bytes(pixels.tobytes())
    report=[movie(game,name,out/(name.lower()+'.vcv')) for name in ('Logo','GTAtitles')]
    (ROOT/'reports/frontend.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps(report),flush=True)

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('game',type=Path);stage(p.parse_args().game)
