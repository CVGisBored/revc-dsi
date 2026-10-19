#!/usr/bin/env python3
"""Bake original ANPK keyframes and DFF skin weights for the ARM9 renderer."""
import argparse
import json
import struct
from pathlib import Path
import numpy as np
from assets import Archive, chunks, one, geometry

CLIPS = ('idle_stance', 'walk_player', 'run_player', 'colt45_fire', 'colt45_reload')
PLAYER_CLIPS = CLIPS + ('sprint_civi','walk_start','run_stop','run_stopr','jump_launch','jump_glide','jump_land','fightpunch')

def ifp(data):
    # IFP container sizes exclude their headers; every payload is four-byte padded.
    def parts(body):
        pos = 0
        while pos < len(body):
            tag, size = struct.unpack_from('<4sI', body, pos)
            end = pos + 8 + size
            if end > len(body): raise ValueError('Truncated IFP')
            yield tag, body[pos+8:end]
            pos = (end+3)&~3
    tag, body = next(parts(data))
    if tag != b'ANPK': raise ValueError('Expected ANPK')
    entries = iter(parts(body)); tag, info = next(entries)
    count, = struct.unpack_from('<I', info)
    result = {}
    for _ in range(count):
        tag, name = next(entries)
        if tag != b'NAME': raise ValueError('Missing animation name')
        tag, dgan = next(entries)
        if tag != b'DGAN': raise ValueError('Missing animation hierarchy')
        sequences = iter(parts(dgan)); tag, info = next(sequences)
        n, = struct.unpack_from('<I', info); nodes = {}
        for _ in range(n):
            tag, cpan = next(sequences)
            nodespec = iter(parts(cpan)); tag, anim = next(nodespec)
            frames, = struct.unpack_from('<I', anim, 28)
            if not frames: continue
            bone = struct.unpack_from('<i', anim, 40)[0] if len(anim)==44 else -1
            if bone<0:bone=anim[:28].split(b'\0')[0].decode().lower()
            tag, keys = next(nodespec)
            width = {b'KR00':5, b'KRT0':8, b'KRTS':11}[tag]
            values = np.frombuffer(keys, '<f4').reshape(frames, width).copy()
            if not np.isfinite(values).all() or (np.diff(values[:,-1])<0).any(): raise ValueError('Invalid keys')
            values[:,:3] *= -1  # CAnimManager::LoadAnimFile inverts the stored quaternion.
            nodes[bone] = values
        result[name.split(b'\0')[0].decode().lower()] = nodes
    return result

def matrix(values):
    m = np.eye(4); m[:3,:3] = np.asarray(values[:9]).reshape(3,3).T; m[:3,3] = values[9:12]
    return m

def skin(data):
    clump = list(chunks(data, padded=True))[0][2]
    _, frames = one(clump, 0xe); _, fs = one(frames, 1)
    count, = struct.unpack_from('<I', fs)
    local=[]; parents=[]; tags=[]; names=[]; order={}
    extensions=[p for k,_,p in chunks(frames) if k==3]
    for i in range(count):
        v=struct.unpack_from('<12fiI',fs,4+i*56);local.append(matrix(v));parents.append(v[12])
        plugins={k:p for k,_,p in chunks(extensions[i])}
        _, bone, n = struct.unpack_from('<iii',plugins[0x11e]); tags.append(bone)
        names.append(plugins.get(0x253f2fe,b'').split(b'\0')[0].decode().lower())
        if n:
            for j in range(n):
                tag,index,_=struct.unpack_from('<iii',plugins[0x11e],20+12*j);order[index]=tag
    _, glist=one(clump,0x1a); geos=[p for k,_,p in chunks(glist) if k==0xf]
    if len(geos)!=1: raise ValueError('Expected one skinned geometry')
    vertices,triangles=geometry(geos[0]); vertices=np.asarray(vertices);nv=len(vertices)
    _,extension=one(geos[0],3);_,raw=one(extension,0x116)
    nb,used,_,_=raw[:4]
    indices=np.frombuffer(raw,'u1',nv*4,4+used).reshape(nv,4)
    weights=np.frombuffer(raw,'<f4',nv*4,4+used+nv*4).reshape(nv,4)
    if not np.allclose(weights.sum(axis=1),1,atol=1e-4) or (indices>=nb).any(): raise ValueError('Invalid skin weights')
    pos=4+used+nv*20; inverse=[]
    for _ in range(nb):
        if not used: pos+=4  # Old skin plugin stores 0xdeaddead before each matrix.
        v=struct.unpack_from('<16f',raw,pos);pos+=64
        inverse.append(matrix(v[:3]+v[4:7]+v[8:11]+v[12:15]))
    boneframes=[tags.index(order[i]) for i in range(nb)]
    globals=[]
    for i,m in enumerate(local): globals.append(globals[parents[i]]@m if parents[i]>=0 else m)
    error=max(np.abs(globals[f]@inv-np.eye(4)).max() for f,inv in zip(boneframes,inverse))
    if error>1e-3:
        # Some mission actors carry a common object-space root offset. Their
        # inverse skin matrices are relative to the geometry, not that frame.
        bind=[globals[f]@inv for f,inv in zip(boneframes,inverse)]
        common=bind[0]
        if parents[0]!=-1 or not all(np.allclose(m,common,atol=1e-3) for m in bind):
            raise ValueError(f'Bind pose mismatch: {error}')
        # Keep that common basis: removing its rotation also rotates animated
        # mission actors onto their sides. Check the residual between bones.
        error=max(np.abs(m-common).max() for m in bind)
    return vertices,triangles,local,parents,tags,names,boneframes,np.asarray(inverse),indices,weights,float(error)

def rotation(q):
    x,y,z,w=q/np.linalg.norm(q)
    return np.array([[1-2*(y*y+z*z),2*(x*y-z*w),2*(x*z+y*w)],
                     [2*(x*y+z*w),1-2*(x*x+z*z),2*(y*z-x*w)],
                     [2*(x*z-y*w),2*(y*z+x*w),1-2*(x*x+y*y)]])

def pose(model, nodes, time):
    vertices,_,local,parents,tags,names,frames,inverse,indices,weights,_=model
    globals=[]
    for i,rest in enumerate(local):
        # The untagged clump wrapper and the tagged root bone can both be
        # named "root". Named IFP tracks must animate only the bone, otherwise
        # e.g. woman_run applies its root translation/rotation twice.
        m=rest.copy(); keys=nodes.get(tags[i],nodes.get(names[i])) if tags[i]>=0 else None
        if keys is not None:
            hi=min(len(keys)-1,int(np.searchsorted(keys[:,-1],time,side='right')));lo=max(0,hi-1)
            a,b=keys[lo],keys[hi];mix=0 if b[-1]==a[-1] else np.clip((time-a[-1])/(b[-1]-a[-1]),0,1)
            q=a[:4].copy();r=b[:4].copy()
            if np.dot(q,r)<0:r=-r
            m[:3,:3]=rotation(q*(1-mix)+r*mix)
            if len(a)>5:m[:3,3]=a[4:7]*(1-mix)+b[4:7]*mix
        globals.append(globals[parents[i]]@m if parents[i]>=0 else m)
    matrices=np.asarray([globals[f]@inv for f,inv in zip(frames,inverse)])
    v=np.concatenate((vertices,np.ones((len(vertices),1))),axis=1)
    transformed=np.einsum('nvij,nj->nvi',matrices[indices],v)
    return (transformed[:,:,:3]*weights[:,:,None]).sum(axis=1)

def bake(archive, animations, name, budget, output, selected_clips=None,
         source_name=None,texture_size=128,texture_atlas=None,texture_slot=0,protect_face=False):
    source_name=source_name or name
    texture_dimensions=(texture_size,texture_size) if isinstance(texture_size,int) else tuple(texture_size)
    model=skin(archive.read(source_name+'.dff'));vertices,triangles=model[:2]
    from materials import detailed_dff,txd
    from car_mesh import material_color
    attributes=detailed_dff(archive.read(source_name+'.dff'))[0];_,material_faces,uv,_,materials=attributes
    textures=txd(archive.read(source_name+'.txd'));face_colors={}
    for a,b,c,mat in material_faces:
        face_colors[a,b,c]=material_color(materials[mat],uv,(a,b,c),textures,((255,255,255),(255,255,255)))
    from simplify_mesh import simplify
    joint_weights=np.zeros((len(vertices),len(model[7])))
    for i in range(len(vertices)):
        for bone,weight in zip(model[8][i],model[9][i]):joint_weights[i,bone]+=weight
    importance=np.ones(len(vertices))
    if protect_face:
        # The cinematic rig has many facial bones. Protect its silhouette while
        # retaining the same total face cap and offline animation baking.
        tags=[model[4][frame] for frame in model[6]]
        head=[i for i,t in enumerate(tags) if t==5 or t>=5000]
        importance+=7*joint_weights[:,head].sum(axis=1)
    reduced,selected,correspondence,source_faces,simplification=simplify(vertices,triangles,budget,joint_weights,importance=importance)
    nt=len(selected);nv=len(reduced)
    from actor_texture import project_uv,save_texture
    face_records=[]
    for face,original in zip(selected,source_faces):
        source_face=triangles[original]
        tex=project_uv(vertices[list(source_face)],np.asarray(uv)[list(source_face)],reduced[face])
        tex=texture_atlas.uv(texture_slot,tex) if texture_atlas else tex*np.asarray(texture_dimensions)*16
        tex=np.clip(np.rint(tex),-32768,32767).astype(int)
        face_records.append(struct.pack('<4H6h',face_colors[tuple(source_face)],*face,*tex.flatten()))
    face_records=b''.join(face_records)
    if name in ('player','player2','player3'):
        names={materials[m][1] for *_,m in material_faces}
        if len(names)!=1 or next(iter(names)) not in textures:raise ValueError('Player must use one source raster')
        from PIL import Image
        save_texture(textures[next(iter(names))].resize(texture_dimensions,Image.Resampling.LANCZOS),output.with_suffix('.atx'))
    records=[];report=[];clips=list(selected_clips or (PLAYER_CLIPS if name in ('player','player2','player3') else CLIPS))
    if selected_clips is None and name not in ('player','player2','player3','hfyst','igmerc2'):clips[1:3]=['walk_civi','run_civi']
    elif selected_clips is None and name in ('hfyst','igmerc2'):clips[:3]=['woman_idlestance','woman_walksexy','woman_run']
    for clip in clips:
        looping=clip in ('idle_stance','walk_player','run_player','walk_civi','run_civi','woman_idlestance','woman_walksexy','woman_run','sprint_civi','car_sit')
        nodes=animations[clip];duration=max(float(k[-1,-1]) for k in nodes.values());nframes=16 if clip in clips[1:3] or clip=='sprint_civi' else 8
        if clip in CLIPS[3:5]:
            # Weapon clips are partial overlays: preserve the idle root/legs.
            base={bone:keys[:1].copy() for bone,keys in animations[clips[0]].items()}
            base.update(nodes);nodes=base
        # Clip root is in GTA's Z-up coordinates. Its displacement belongs to gameplay.
        root=pose(model,nodes,0)
        for frame in range(nframes):
            p=pose(model,nodes,duration*frame/(nframes if looping else nframes-1))
            # Remove the root's horizontal travel, which gameplay already integrates.
            keys=nodes.get(0,nodes.get('root'))
            if keys is not None and keys.shape[1]>5 and clip not in ('jump_launch','jump_glide'):
                time=duration*frame/(nframes if looping else nframes-1)
                for axis in (0,1):
                    travel=np.interp(time,keys[:,-1],keys[:,4+axis])-keys[0,4+axis]
                    p[:,axis]-=travel
            p=correspondence@p
            if selected_clips and clip.startswith('car_'):
                # Vehicle poses are relative to the authored seat/pelvis, rather
                # than separately grounding every pose and lifting the driver.
                time=duration*frame/(nframes if looping else nframes-1)
                if keys is not None and keys.shape[1]>6:
                    p[:,0]-=keys[0,4];p[:,1]-=keys[0,5]
                    p[:,2]-=np.interp(time,keys[:,-1],keys[:,6])
                q=np.column_stack((p[:,0],p[:,2],-p[:,1]))
            else:
                q=np.column_stack((p[:,0],p[:,2],-p[:,1]));q[:,1]-=root[:,2].min()
                q[:,1]-=min(0.,q[:,1].min())
            if not np.isfinite(q).all() or np.abs(q).max()>5:raise ValueError('Unbounded animation pose')
            records.append(np.rint(q*64).astype('<i2').tobytes())
        keys=nodes.get(0,nodes.get('root'));speed=float(np.linalg.norm(keys[-1,4:6]-keys[0,4:6])/duration) if keys is not None and keys.shape[1]>5 else 0.
        report.append({'clip':clip,'duration_ticks':max(1,round(duration*60)),'frames':nframes,'root_speed':speed})
    header=struct.pack('<4s4H',b'VCA6',nt,len(clips),nv,0)+b''.join(struct.pack('<HHf',r['frames'],r['duration_ticks'],r['root_speed']) for r in report)
    output.write_bytes(header+face_records+b''.join(records))
    return {'model':name,'source_model':source_name,'texture_size':texture_size,'face_protected':protect_face,'triangles':nt,'bytes':output.stat().st_size,'bind_error':model[-1],'clips':report,'vertices':nv,'simplification':simplification,
            'colors':('Shared original-raster atlas and per-face UVs' if texture_atlas else
                      'Original raster texture and per-face UVs' if name in ('player','player2','player3') else
                      'Sampled source colors')}

if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('game',type=Path);args=parser.parse_args()
    root=Path(__file__).resolve().parents[1];animations=ifp((args.game/'anim/ped.ifp').read_bytes());archive=Archive(args.game)
    animations.update(ifp(archive.read('colt45.ifp')))
    reports=[bake(archive,animations,'player',520,root/'sdcard/revc/world/player.vca'),
             bake(archive,animations,'swat',120,root/'sdcard/revc/world/guard.vca'),
             bake(archive,animations,'player2',520,root/'sdcard/revc/world/player2.vca'),
             bake(archive,animations,'player3',520,root/'sdcard/revc/world/player3.vca'),
             bake(archive,animations,'igmerc2',120,root/'sdcard/revc/world/mercedes.vca')]
    (root/'reports/animation.json').write_text(json.dumps(reports,indent=2)+'\n');print(json.dumps(reports))
