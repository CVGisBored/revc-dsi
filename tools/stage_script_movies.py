#!/usr/bin/env python3
"""Bake the two non-DAT mission endings from the reviewed successful SCM paths."""
import argparse,bisect,json,struct,subprocess
from pathlib import Path
import numpy as np
from PIL import Image
from assets import resolve,manifest,lines
from cutscene_render import Renderer,GL
from stage_departure import StagedActor,transform
from stage_animation import ifp
from stage_navigation import gxt_table
from stage_cutscenes import subtitles
from scm_cutscenes import commands
from stage_scripts import inspect
from materials import detailed_dff,txd
from cinematic_codec import palette_image,raw_frame,encode_motion,decode_motion,KEY
ROOT=Path(__file__).resolve().parents[1]

def stage(game,name,out):
    scm=resolve(game,'data/main.scm').read_bytes();info=inspect(scm)
    if info['sha256']!='0fb3fd45b6a6dc21870cb48d9abe983510284b2b09b0c3d0eeac51d5dff338c7':raise ValueError('Unreviewed SCM')
    concert=name=='fist';rows=list(commands(scm,65 if concert else 30));idx={a:(n,v)for a,n,v in rows}
    def at(a,n):
        cmd,v=idx[a]
        if cmd!=n:raise ValueError((a,cmd,n))
        return v
    actor_start=5159 if concert else 10219
    origin=np.asarray(at(actor_start,'SET_PLAYER_COORDINATES')[1:4])
    renderer=Renderer(game);spec={'name':name,'origin':origin}
    if concert:renderer.placements=[p for p in renderer.placements if p.source.lower().endswith('concerth.ipl')]
    # The second rooftop camera sits inside the back faces of the lamp housing.
    if not concert:GL.glEnable(GL.GL_CULL_FACE)
    renderer.background(spec);background=renderer.static[:]
    anim=ifp(resolve(game,'anim/ped.ifp').read_bytes());idle=anim['idle_stance'];chat=anim['idle_chat']
    actors=[];shots=[];subs=[];props={};events=[];pose_events={}
    def ped(model,pos,heading,handle):
        actor=StagedActor(renderer,model,idle,lambda t:transform(pos-origin,heading),lambda t:t%2)
        actors.append((handle,actor));return actor
    tommy=ped('player',origin,at(actor_start+20,'SET_PLAYER_HEADING')[1],12)
    pcm=subprocess.check_output(['ffmpeg','-v','error','-i',str(resolve(game,'Audio/'+('FIST' if concert else 'Glight')+'.mp3')),'-f','s8','-ac','1','-ar','16384','-'])
    if concert:
        for a,model in [(5210,'igdick'),(5292,'igpercy')]:
            v=at(a,'SET_CHAR_COORDINATES');ped(model,np.asarray(v[1:4]),at(a+20,'SET_CHAR_HEADING')[1],v[0][1])
        time=0
        for a,n,v in rows:
            if not 5358<=a<=8851:continue
            if n=='IS_NUMBER_GREATER_THAN_INT_VAR' and v[1]==('global',496):time=v[0]
            if n=='SET_FIXED_CAMERA_POSITION':eye=v[:3]
            if n=='POINT_CAMERA_AT_POINT':shots.append((time,eye,v[:3]))
            if n=='SET_CHAR_WAIT_STATE':pose_events.setdefault(v[0][1],[]).append((time,v[1]==19))
            if n=='PRINT_NOW' and v[0].startswith('ROK3_'):subs.append(dict(time=time,key=v[0],duration=v[1]))
        duration=time
        subs=subtitles({'subtitles':subs},gxt_table(resolve(game,'TEXT/american.gxt').read_bytes(),'ROCK3'),duration/1000)
    else:
        # WAIT 0 inside the rotating searchlight loop is frame-dependent on PC.
        # Fit that rotation between the original timed beats and the music ending.
        duration=round(len(pcm)*1000/16384);start,end=10310,10865
        waits=sum(v[0] for a,n,v in rows if start<a<end and n=='WAIT')
        rotation=max(0,duration-waits);time=0;rot_start=0
        setup=list(commands(scm,0))
        init=[(a,v) for a,n,v in setup if n=='CREATE_OBJECT_NO_OFFSET' and v[-1] in (('global',7220),('global',7224))]
        def prop(v):
            model=scm[info['globals_bytes']+12-24*v[0]:info['globals_bytes']+36-24*v[0]].split(b'\0')[0].decode().lower()
            mid=next(int(s.split(',')[0]) for f in manifest(game,'IDE') for _,s in lines(f) if ',' in s and s.split(',')[1].strip().lower()==model)
            tex=txd(renderer.archive.read(renderer.texture_names[mid]+'.txd'))
            return [(np.asarray(p),renderer.batches(f,uv,l,m,tex)) for p,f,uv,l,m in detailed_dff(renderer.archive.read(model+'.dff'))]
        for _,v in init:
            h=v[-1][1];props[h]=(prop(v),np.asarray(v[1:4])-origin);events.append((0,h,True))
        for a,n,v in rows:
            if not 10249<=a<=end:continue
            if n=='WAIT':time+=v[0]
            if n=='ROTATE_OBJECT':rot_start=time;time+=rotation
            if n=='SET_FIXED_CAMERA_POSITION':eye=v[:3]
            if n=='POINT_CAMERA_AT_POINT':shots.append((time,eye,v[:3]))
            if n=='SET_PLAYER_COORDINATES':move_time=time;move_pos=np.asarray(v[1:4])-origin
            if n=='CREATE_OBJECT_NO_OFFSET':
                h=v[-1][1]
                if h not in props:props[h]=(prop(v),np.asarray(v[1:4])-origin)
                events.append((time,h,True))
            if n=='DELETE_OBJECT':events.append((time,v[0][1],False))
        tommy.placement=lambda t:transform(move_pos if t*1000>=move_time else np.zeros(3),at(10239,'SET_PLAYER_HEADING')[1])
    if not shots or shots[0][0]!=0:raise ValueError('Missing initial scripted shot')
    def keys(p):return np.array([[0,*p,*p,*p],[duration/1000,*p,*p,*p]])
    paths=[[keys([70]),keys([0]),keys(np.asarray(e)-origin),keys(np.asarray(t)-origin)] for _,e,t in shots]
    def render(t):
        ms=t*1000
        for h,actor in actors:
            states=pose_events.get(h,[]);actor.nodes=chat if next((state for at,state in reversed(states) if ms>=at),False) else idle
        if not concert:
            renderer.static=list(background)
            for h,(groups,pos) in props.items():
                if not next((on for at,k,on in reversed(events) if k==h and ms>=at),False):continue
                angle=160+200*np.clip((ms-rot_start)/max(1,rotation),0,1) if h==7220 else 0
                mat=transform(pos,angle)
                renderer.static.extend((v@mat[:3,:3].T+pos,b)for v,b in groups)
        shot=bisect.bisect_right([s[0]for s in shots],ms)-1
        im=renderer.render(spec,paths[shot],[a for _,a in actors],t)
        if concert and ms>=49873:im=Image.blend(Image.new('RGB',im.size),im,max(0,1-(ms-49873)/1500))
        return im
    fps=30;frames=(duration*fps+999)//1000
    samples=[render(duration/1000*i/24) for i in range(24)]
    samples[10].save(ROOT/f'reports/script-movie-{name}.png')
    palette=palette_image(samples);parts=[];sizes=[];old=None
    for i in range(frames):
        p,key,stable=encode_motion(raw_frame(render(i/fps),palette),old,i%30==0)
        if decode_motion(p,old,key)!=stable:raise ValueError(f'Frame {i} failed')
        parts.append(p);sizes.append(len(p)|(KEY if key else 0));old=stable
    samples=(duration*16384+999)//1000;pcm=pcm[:samples].ljust(samples,b'\0')
    out.mkdir(parents=True,exist_ok=True);tmp=out/(name+'.vcv.tmp')
    tmp.write_bytes(struct.pack('<4s4H7I',b'VCV3',128,72,fps,len(subs),frames,16384,len(pcm),duration,max(map(len,parts)),9728,88)+b''.join(struct.pack('<II80s',a,b,v.encode()[:79])for a,b,v in subs)+struct.pack('<%dI'%frames,*sizes)+pcm+b''.join(parts));tmp.replace(out/(name+'.vcv'))
    record=dict(name=name,duration_ms=duration,frames=frames,camera_cuts_ms=[t for t,_,_ in shots],subtitle_pages=len(subs),source_audio='FIST.mp3' if concert else 'Glight.mp3',objects=list(props),actors=[a.spec['model'] for _,a in actors],adaptation=('SCM success path with stock chat poses and no lip sync.' if concert else 'SCM success path with searchlight rotation fitted to music.'))
    (ROOT/f'reports/script-movie-{name}.json').write_text(json.dumps(record,indent=2)+'\n');print(record)
if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('game',type=Path);p.add_argument('name',choices=('gspot','fist'));p.add_argument('--output',type=Path,default=ROOT/'sdcard/revc/cutscenes');a=p.parse_args();stage(a.game,a.name,a.output)
