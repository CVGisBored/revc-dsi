#!/usr/bin/env python3
"""Build ROM-embedded streamed opening cinematics from the owned game's assets."""
import argparse,hashlib,json,struct,subprocess,textwrap,math,time
from pathlib import Path
import numpy as np
from assets import resolve
from scm_cutscenes import scenes,commands
from cutscene_render import Cuts,Renderer,Actor,cameras
from stage_animation import ifp
from stage_navigation import gxt_table
from compact_assets import lz10,unpack
ROOT=Path(__file__).resolve().parents[1]
FPS=10

def subtitles(spec,texts,duration):
    events=[]
    source=[]
    for sub in spec['subtitles']:
        # Some scripts include a second regional edit of the same scene.
        # Use the first complete timeline, matching the supplied DAT/IFP,
        # instead of concatenating overlapping branches (porn_4).
        if source and sub['time']<source[-1]['time']:break
        source.append(sub)
    for i,sub in enumerate(source):
        # Missing retail GXT keys produce no text in the original engine too.
        if sub['key'] not in texts:continue
        start=sub['time'];end=min(math.ceil(duration*1000),start+sub['duration'],source[i+1]['time'] if i+1<len(source) else math.ceil(duration*1000))
        if end<=start:continue
        lines=textwrap.wrap(texts[sub['key']].encode('ascii','replace').decode(),32)
        pages=['\n'.join(lines[p:p+2]) for p in range(0,len(lines),2)]
        for j,page in enumerate(pages):events.append((start+(end-start)*j//len(pages),start+(end-start)*(j+1)//len(pages),page))
    return events

def stage(game,preview=False,only=None,indices=None,fps=10,compact=False,interframe=False,output=None,motion=False):
    global FPS
    FPS=fps
    scm=resolve(game,'data/main.scm').read_bytes()
    if hashlib.sha256(scm).hexdigest()!='0fb3fd45b6a6dc21870cb48d9abe983510284b2b09b0c3d0eeac51d5dff338c7':raise ValueError('Unreviewed SCM version')
    specs=[]
    for i in (indices or [1,2]):
        table=next((v[0] for _,n,v in commands(scm,i) if n=='LOAD_MISSION_TEXT'), 'MAIN')
        for spec in scenes(scm,i):spec['table']=table;specs.append(spec)
    initial={v[0]:v[1] for _,n,v in commands(scm,0) if n=='SET_VAR_FLOAT' and isinstance(v[1],(int,float))}
    cuts=Cuts(game);renderer=Renderer(game);out=output or ROOT/'sdcard/revc/cutscenes';out.mkdir(exist_ok=True,parents=True)
    report_path=(out/'conversion.json') if output else ROOT/('reports/mission-cutscenes.json' if indices else 'reports/cutscenes.json')
    report=json.loads(report_path.read_text()) if report_path.exists() else [];gxt=resolve(game,'TEXT/american.gxt').read_bytes()
    for spec in specs:
        name=spec['name']
        if only and name!=only:continue
        
        # Yacht scenes refer to main-script global coordinates.
        for j,v in enumerate(spec['origin']):
            if isinstance(v,tuple):
                if v in initial:spec['origin'][j]=initial[v];continue
                if v!=('global',(1792,1796,1800)[j]):raise ValueError(f'Unresolved cutscene origin: {v}')
                # Main SCM CREATE_OBJECT_NO_OFFSET at 44250: yacht anchor (-8).
                # Verified opcode/model, coordinates and destination global 2856.
                at=44252
                if scm[at:at+4]!=bytes.fromhex('9b0204f8') or scm[44271:44274]!=bytes.fromhex('02280b'):raise ValueError('Changed yacht anchor')
                base=[struct.unpack_from('<f',scm,k)[0] for k in (44257, 44262, 44267)]
                spec['origin'][j]=base[j]+(.003,-1.381,3.149)[j]
        print('Preparing',name,flush=True);paths=cameras(cuts.read(name+'.dat'));duration=float(paths[2][-1,0]);animations=ifp(cuts.read(name+'.ifp'))
        actors=[Actor(renderer.archive,obj,animations,renderer) for obj in spec['objects']];renderer.background(spec)
        table=spec['table']
        texts=gxt_table(gxt,table);subs=subtitles(spec,texts,duration)
        for sample in (min(2.,duration/2),min(15.,duration/2)):
            image=renderer.render(spec,paths,actors,sample);image.save(ROOT/f'reports/cutscene-{name}-{int(sample):02}.png')
        if preview:continue
        count=math.ceil(duration*FPS);compressedmax=0;rawmax=9728 if compact else 37376
        # Header: magic, dimensions/fps/subtitle count, frame count/audio rate/audio size/duration.
        # Audio filename differs from the SCM scene name (sampman.h).
        audio_name='fin2' if name=='fin_2' else name
        pcm=subprocess.check_output(['ffmpeg','-v','error','-i',str(resolve(game,'Audio/'+audio_name+'.mp3')),'-f','s8','-ac','1','-ar','16384','-t',str(duration),'-'])
        pcm=pcm[:math.ceil(duration*16384)].ljust(math.ceil(duration*16384),b'\0')
        frameparts=[];sizes=[];previous=None;palette=None
        if interframe:
            from cinematic_codec import palette_image,raw_frame,encode_frame,decode_frame,encode_motion,decode_motion,KEY
            # A shared scene palette keeps static texture pixels stable between
            # frames; temporal compression is lossless after quantization.
            samples=[renderer.render(spec,paths,actors,duration*i/24) for i in range(24)]
            palette=palette_image(samples)
            del samples

        for frame in range(count):
            image=renderer.render(spec,paths,actors,frame/FPS)
            if interframe:
                raw=raw_frame(image,palette)
                if motion:
                    packed,key,stable=encode_motion(raw,previous,frame%30==0)
                    if decode_motion(packed,previous,key)!=stable:raise ValueError('Motion frame differs')
                    previous=stable
                else:
                    packed,key=encode_frame(raw,previous,frame%15==0)
                    if decode_frame(packed,previous,key)!=raw:raise ValueError('Delta frame differs')
                    previous=raw
                sizes.append(len(packed)|(KEY if key else 0))
            else:
                if compact:
                    from PIL import Image
                    image=image.resize((128,72),Image.Resampling.BOX)
                quantized=image.quantize(colors=256,method=0)
                rgb=np.asarray(quantized.getpalette(),dtype=np.uint16).reshape(-1,3)
                palette_bytes=((rgb[:,0]>>3)|((rgb[:,1]>>3)<<5)|((rgb[:,2]>>3)<<10)|0x8000).astype('<u2').tobytes().ljust(512,b'\0')
                raw=palette_bytes+np.asarray(quantized,dtype=np.uint8).tobytes();packed=lz10(raw)
                if unpack(packed)!=raw:raise ValueError('Cutscene compression differs')
                sizes.append(len(packed))
            compressedmax=max(compressedmax,len(packed));frameparts.append(packed)
            if frame%100==0:print(f'{name}: {frame}/{count} frames',flush=True)
        header=struct.pack('<4s4H7I',b'VCV3' if motion else b'VCV2' if interframe else b'VCV1',128 if compact else 256,72 if compact else 144,FPS,len(subs),count,16384,len(pcm),math.ceil(duration*1000),compressedmax,rawmax,88)
        subtitles_blob=b''.join(struct.pack('<II80s',a,b,text.encode()[:79]) for a,b,text in subs)
        sizes=struct.pack('<'+str(count)+'I',*sizes)
        dest=out/(name+'.vcv');temporary=dest.with_suffix('.vcv.tmp');temporary.write_bytes(header+subtitles_blob+sizes+pcm+b''.join(frameparts));temporary.replace(dest)
        entry={'name':name,'duration':duration,'frames':count,'fps':FPS,'codec':'VCV3 motion/XOR/LZ10' if motion else 'VCV2 XOR/LZ10' if interframe else 'VCV1 LZ10','bytes':dest.stat().st_size,'max_compressed':compressedmax,'subtitles':len(subs),'objects':len(actors),'static_groups':len(renderer.static),'audio_bytes':len(pcm),'source':'original SCM objects, IFP poses, DAT Bezier cameras, TXD/DFF geometry, GXT timings and MP3 speech'}
        entry['missing_subtitle_keys']=[s['key'] for s in spec['subtitles'] if s['key'] not in texts]
        entry['unanimated_objects']=[o['model'] for o in spec['objects'] if o.get('animation') not in animations]
        report=[e for e in report if e['name']!=name];report.append(entry);print(json.dumps(entry),flush=True)
        report_path.write_text(json.dumps(report,indent=2)+'\n')

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('game',type=Path);p.add_argument('--preview',action='store_true');p.add_argument('--only');p.add_argument('--missions',type=int,nargs='+');p.add_argument('--fps',type=int,default=10);p.add_argument('--compact',action='store_true');p.add_argument('--interframe',action='store_true');p.add_argument('--output',type=Path);p.add_argument('--motion',action='store_true');a=p.parse_args();
    if a.motion:a.interframe=True
    if a.interframe and not a.compact:p.error('--interframe requires --compact')
    if not 1<=a.fps<=30:p.error('--fps must be 1..30')
    stage(a.game,a.preview,a.only,a.missions,a.fps,a.compact,a.interframe,a.output,a.motion)
