#!/usr/bin/env python3
"""Stage the INTRO script's ride and Rosenberg departure with original speech.
This reconstructs the scripted (non-IFP-cutscene) segment using its authored
positions, cameras, original normal character animations and car model.
"""
import argparse,math,struct,json,subprocess,textwrap
from pathlib import Path
import numpy as np
from PIL import Image
from assets import resolve
from cutscene_render import Renderer,Actor
from scm_cutscenes import commands
from stage_animation import ifp
from stage_navigation import gxt_table
from compact_assets import lz10,unpack
ROOT=Path(__file__).resolve().parents[1]

class StagedActor(Actor):
    def __init__(self,renderer,model,nodes,placement,cliptime=lambda t:t):
        super().__init__(renderer.archive,{'model':model,'animation':'script','handle':('actor',model)}, {'script':nodes},renderer)
        self.placement=placement;self.cliptime=cliptime
    def update(self,time):super().update(self.cliptime(time))
    def draw(self,renderer,time,parent=None):super().draw(renderer,self.cliptime(time),self.placement(time))

def transform(xyz,angle=0):
    c,s=math.cos(math.radians(angle)),math.sin(math.radians(angle));m=np.eye(4);m[:2,:2]=[[c,-s],[s,c]];m[:3,3]=xyz;return m

def stage(game,preview=False,fps=10,motion=False,output_dir=None):
    scm=resolve(game,'data/main.scm').read_bytes();cmd=list(commands(scm,1));at={p:(name,args) for p,name,args in cmd}
    def vec(p,name,start=0):
        n,a=at[p]
        if n!=name:raise ValueError('Changed INTRO departure command')
        return np.asarray(a[start:start+3],float)
    origin=np.array([119.,-826.,9.44]);route=[vec(12124,'CREATE_CAR',1),vec(12426,'CAR_GOTO_COORDINATES',1),vec(12740,'CAR_GOTO_COORDINATES',1),vec(12833,'CAR_GOTO_COORDINATES',1)]
    # All camera/actor coordinates come from the non-cutscene portion of INTRO.
    eyes=[vec(12349,'SET_FIXED_CAMERA_POSITION'),vec(13099,'SET_FIXED_CAMERA_POSITION'),vec(13981,'SET_FIXED_CAMERA_POSITION')]
    targets=[vec(12381,'POINT_CAMERA_AT_POINT'),vec(13131,'POINT_CAMERA_AT_POINT'),vec(14013,'POINT_CAMERA_AT_POINT')]
    ken_path=[vec(12644,'SET_CAR_COORDINATES',1),np.array([125.809,-815.263,9.444]),np.array([116.269,-825.438,9.444])]
    player=vec(14139,'SET_CHAR_COORDINATES',1)
    audio=[];subtitles=[];cursor=0
    text=gxt_table(resolve(game,'TEXT/american.gxt').read_bytes(),'INTRO')
    for i in range(1,5):
        pcm=subprocess.check_output(['ffmpeg','-v','error','-i',str(resolve(game,f'Audio/intro{i}.wav')),'-f','s8','-ac','1','-ar','16384','-'])
        lines=textwrap.wrap(text[f'INTRO{i}'],32);pages=['\n'.join(lines[j:j+2]) for j in range(0,len(lines),2)];duration=len(pcm)/16384
        for j,page in enumerate(pages):subtitles.append((round((cursor+duration*j/len(pages))*1000),round((cursor+duration*(j+1)/len(pages))*1000),page))
        audio.append(pcm);cursor+=duration
        gap=1. if i==1 else .9 if i==2 else .4 if i==3 else 0
        audio.append(b'\0'*round(gap*16384));cursor+=gap
    dialogue=cursor;duration=dialogue+9;pcm=b''.join(audio).ljust(math.ceil(duration*16384),b'\0')
    renderer=Renderer(game);spec={'name':'departure','origin':origin};renderer.background(spec)
    ped=ifp(resolve(game,'anim/ped.ifp').read_bytes())
    def nodes(clip):
        result={k:v.copy() for k,v in ped[clip].items()}
        root=result.get(0,result.get('root'))
        if root is not None and root.shape[1]>5:root[:,4:6]=0
        return result
    idle=nodes('idle_stance');walk=nodes('walk_civi');sit=nodes('car_sitp');driver=nodes('car_sit')
    distances=[np.linalg.norm(route[i+1][:2]-route[i][:2]) for i in range(3)];length=sum(distances);drive_duration=3.5
    def car(t):
        along=min(1,t/drive_duration)*length
        for i,d in enumerate(distances):
            if along<=d or i==2:
                u=min(1,along/d);p=route[i]*(1-u)+route[i+1]*u;delta=route[i+1]-route[i];angle=math.degrees(math.atan2(-delta[0],delta[1]));break
            along-=d
        # The car model's origin is above its wheel contacts, matching the SCM car Z.
        p=p-origin;return transform(p-[0,0,.5],angle)
    def ken(t):
        if t<dialogue:return car(t)@transform([-.5,.1,.3])
        u=min(1,(t-dialogue)/8);p=ken_path[0]*(1-min(1,u*2))+ken_path[1]*min(1,u*2) if u<.5 else ken_path[1]*(2-2*u)+ken_path[2]*(2*u-1)
        return transform(p-origin,225)
    def tommy(t):return car(t)@transform([.5,.1,.3]) if t<dialogue else transform(player-origin,225.495)
    # Alternate seated/standing source clips at the authored exit; limbs remain ordinary game animations.
    class Passenger(StagedActor):
        def __init__(self,model,placement):super().__init__(renderer,model,sit,placement)
        def update(self,t):
            self.nodes=(walk if self.spec['model']=='igken' else idle) if t>=dialogue else (driver if self.spec['model']=='igken' else sit)
            self.pose_time=(t-dialogue)%1.1 if t>=dialogue and self.spec['model']=='igken' else t%2
            Actor.update(self,self.pose_time)
        def draw(self,r,t,parent=None):Actor.draw(self,r,self.pose_time,self.placement(t))
    actors=[StagedActor(renderer,'admiral',{},car),Passenger('igken',ken),Passenger('player',tommy)]
    # Intact road car parts use the original paint markers rather than displaying their debug colors.
    for actor in actors:
        if actor.spec['model']=='admiral':
            actor.groups=[g for g in actor.groups if not any(x in actor.names[g[0]] for x in ('_dam','_vlo'))]
            for _,_,batches in actor.groups:
                for _,_,rgba,_ in batches:
                    mask=(rgba[:,1]>.9)&(rgba[:,0]<.4)&(rgba[:,2]<.2);rgba[mask,:3]=.7
    def render(t):
        shot=0 if t<2 else 1 if t<dialogue else 2
        eye=eyes[shot]-origin;target=targets[shot]-origin
        # A constant key pair reuses the exact camera projection and texture pipeline.
        paths=[np.array([[0,70,70,70],[duration,70,70,70]]),np.array([[0,0,0,0],[duration,0,0,0]])]
        paths += [np.array([[0,*p,*p,*p],[duration,*p,*p,*p]]) for p in (eye,target)]
        return renderer.render(spec,paths,actors,t)
    for t in (3.,dialogue+3):render(t).save(ROOT/f'reports/cutscene-departure-{int(t):02}.png')
    if preview:return
    frames=math.ceil(duration*fps);parts=[];sizes=[];previous=None
    if motion:
        from cinematic_codec import palette_image,raw_frame,encode_motion,decode_motion,KEY
        palette_image_source=palette_image([render(duration*i/24) for i in range(24)])
    for f in range(frames):
        if motion:
            raw=raw_frame(render(f/fps),palette_image_source);packed,key,stable=encode_motion(raw,previous,f%30==0)
            if decode_motion(packed,previous,key)!=stable:raise ValueError('Departure motion differs')
            previous=stable;sizes.append(len(packed)|(KEY if key else 0))
        else:
            image=render(f/fps).quantize(colors=256,method=0);rgb=np.asarray(image.getpalette(),dtype=np.uint16).reshape(-1,3)
            palette=((rgb[:,0]>>3)|((rgb[:,1]>>3)<<5)|((rgb[:,2]>>3)<<10)|0x8000).astype('<u2').tobytes().ljust(512,b'\0')
            raw=palette+np.asarray(image,dtype=np.uint8).tobytes();packed=lz10(raw)
            if unpack(packed)!=raw:raise ValueError('Departure compression differs')
            sizes.append(len(packed))
        parts.append(packed)
        if f%100==0:print('departure',f,frames,flush=True)
    header=struct.pack('<4s4H7I',b'VCV3' if motion else b'VCV1',128 if motion else 256,72 if motion else 144,fps,len(subtitles),frames,16384,len(pcm),math.ceil(duration*1000),max(map(len,parts)),9728 if motion else 37376,88)
    output=(output_dir or ROOT/'sdcard/revc/cutscenes')/'departure.vcv';output.parent.mkdir(parents=True,exist_ok=True);output.write_bytes(header+b''.join(struct.pack('<II80s',a,b,t.encode()[:79]) for a,b,t in subtitles)+struct.pack('<'+str(frames)+'I',*sizes)+pcm+b''.join(parts))
    report={'name':'departure','bytes':output.stat().st_size,'duration':duration,'frames':frames,'fps':fps,'source':'INTRO authored cameras, road route, actor placements and INTRO1..4 speech; ordinary ped animations approximate the scripted actions','limitations':'Scripted vehicle/actor motion approximated between original waypoints; source engine AI and facial animation not running'}
    ((output_dir/'departure.json') if output_dir else ROOT/'reports/departure.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report))
if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('game',type=Path);p.add_argument('--preview',action='store_true');p.add_argument('--fps',type=int,default=10);p.add_argument('--motion',action='store_true');p.add_argument('--output',type=Path);a=p.parse_args();stage(a.game,a.preview,a.fps,a.motion,a.output)
