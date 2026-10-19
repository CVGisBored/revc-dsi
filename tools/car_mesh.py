"""Intact high-detail vehicle parts, original wheel models, and stock colors."""
from pathlib import Path
import struct
from assets import chunks,one,geometry,compose,transform,resolve,lines
from materials import detailed_dff,detailed_geometry,txd
import numpy as np

def frame_data(data):
    clump=list(chunks(data,padded=True))[0][2];_,flist=one(clump,0xe);_,body=one(flist,1)
    count,=struct.unpack_from('<I',body);extensions=[p for k,_,p in chunks(flist) if k==3];result=[]
    for i in range(count):
        v=struct.unpack_from('<12fiI',body,4+i*56);matrix=compose(result[v[12]][1],v[:12]) if v[12]>=0 else v[:12]
        plugins={k:p for k,_,p in chunks(extensions[i])};name=plugins.get(0x253f2fe,b'').split(b'\0')[0].decode().lower()
        result.append((name,matrix))
    return clump,result

def palette(game,name):
    section='';colors=[];paint=None
    for _,row in lines(resolve(game,'data/carcols.dat')):
        if row.lower() in ('col','car','end'):section=row.lower();continue
        fields=[v.strip() for v in row.split(',')]
        if section=='col':colors.append(tuple(int(v) for v in fields[:3]))
        elif section=='car' and fields[0].lower()==name:paint=(colors[int(fields[1])],colors[int(fields[2])])
    return paint or ((56,120,208),(5,5,5))

def material_color(mat,uv,face,textures,paint):
    color,name=mat;rgb=np.array(color[:3],dtype=float)
    if tuple(color[:3])==(60,255,0):rgb=np.array(paint[0],dtype=float)
    elif tuple(color[:3])==(255,0,175):rgb=np.array(paint[1],dtype=float)
    image=textures.get(name)
    if image is not None:
        image=image.convert('RGB');coords=np.mean([uv[i] for i in face],axis=0)
        pixel=np.array(image.getpixel((int(coords[0]*image.width)%image.width,int(coords[1]*image.height)%image.height)))
        rgb=rgb*pixel/255
    elif color[3]<255:rgb*=.35 # Glass without a raster retains a dark reflective tint.
    r,g,b=[min(31,max(0,round(v/255*31))) for v in rgb]
    return r|g<<5|b<<10

def convert_car(archive,name,output,budget,texture_size=256,build_lod=False,shared_atlas=None,collect_only=False):
    game=archive.path.parent.parent;data=archive.read(name+'.dff');clump,frames=frame_data(data)
    paint=palette(game,name);textures={}
    for dictionary in ('generic',name):
        key=dictionary+'.txd'
        try:textures.update(txd(archive.read(key) if key in archive.entries else resolve(game,'models/'+key).read_bytes()))
        except (KeyError,ValueError,FileNotFoundError):pass
    from actor_texture import Atlas,project_uv
    atlas=shared_atlas or Atlas()
    body=[]
    # Keep the original atomic identity through simplification. Doors and wheels
    # must rotate about their authored dummy frames, never a guessed centroid.
    _,glist=one(clump,0x1a);geos=[p for k,_,p in chunks(glist) if k==0xf]
    parts=[('',(0,0,0))]
    for label in ('wheel_rb_dummy','wheel_lb_dummy','wheel_rf_dummy','wheel_lf_dummy','door_lf_dummy','door_rf_dummy'):
        matrix=next(m for n,m in frames if n==label)
        c=transform(matrix,(0,0,0));parts.append((label,(c[0],c[2],-c[1])))
    for kind,_,atomic in chunks(clump):
        if kind!=0x14:continue
        _,spec=one(atomic,1);frame,geo,flags,_=struct.unpack('<4I',spec)
        label,matrix=frames[frame]
        if not flags&4 or any(s in label for s in ('_vlo','_dam','extra2')):continue
        vertices,faces,uv,prelight,materials=detailed_geometry(geos[geo])
        vertices=[transform(matrix,p) for p in vertices]
        part=5 if label.startswith('door_lf_') else 6 if label.startswith('door_rf_') else 0
        for a,b,c,mat in faces:
            points=[vertices[i] for i in (a,b,c)];body.append((points,material_color(materials[mat],uv,(a,b,c),textures,paint),np.asarray([uv[i] for i in (a,b,c)]),atlas.add(materials[mat],textures,paint),part))
    # The IDE wheel model and scale are the same values used by reVC.
    wheel_id=254;scale=.7
    for path in (game/'data').glob('*.ide'):
        for _,row in lines(path):
            fields=[v.strip() for v in row.split(',')]
            if len(fields)>=13 and fields[1].lower()==name and fields[3].lower()=='car':wheel_id=int(fields[11]);scale=float(fields[12])
    wheel_names={250:'wheel_sport',251:'wheel_saloon',252:'wheel_lightvan',253:'wheel_classic',254:'wheel_alloy',255:'wheel_lighttruck',256:'wheel_smallcar'}
    wheel_data=resolve(game,'models/generic/wheels.dff').read_bytes();wc,wf=frame_data(wheel_data)
    _,glist=one(wc,0x1a);geos=[p for k,_,p in chunks(glist) if k==0xf];wheel=[]
    wheel_textures=txd(resolve(game,'models/generic/wheels.txd').read_bytes())
    for kind,_,atomic in chunks(wc):
        if kind!=0x14:continue
        _,spec=one(atomic,1);frame,geo,flags,_=struct.unpack('<4I',spec)
        if wf[frame][0]!=wheel_names.get(wheel_id,'wheel_alloy')+'_l0':continue
        vertices,faces,uv,prelight,materials=detailed_geometry(geos[geo])
        for a,b,c,mat in faces:wheel.append(([vertices[i] for i in (a,b,c)],material_color(materials[mat],uv,(a,b,c),wheel_textures,paint),np.asarray([uv[i] for i in (a,b,c)]),atlas.add(materials[mat],wheel_textures,paint)))
    from simplify_mesh import simplify
    if collect_only:
        # Collect the distant model's materials too; its UVs share this page.
        if build_lod:
            _,bodylist=one(clump,0x1a);bodygeos=[p for k,_,p in chunks(bodylist) if k==0xf]
            for kind,_,atomic in chunks(clump):
                if kind!=0x14:continue
                _,spec=one(atomic,1);frame,geo,flags,_=struct.unpack('<4I',spec)
                if not flags&4 or '_vlo' not in frames[frame][0]:continue
                _,faces,_,_,materials=detailed_geometry(bodygeos[geo])
                for *_,mat in faces:atlas.add(materials[mat],textures,paint)
        return
    def selected(faces,limit):
        # Keep complete connected surfaces instead of dropping small triangles.
        vertices=[q for record in faces for q in record[0]]
        indices=np.arange(len(vertices)).reshape(-1,3)
        labels=np.repeat(np.asarray([r[4] if len(r)>4 else 0 for r in faces]),3)
        reduced,tris,_,originals,stats=simplify(vertices,indices,limit,attributes=labels[:,None],preserve_features=True)
        result=[]
        for tri,original in zip(tris,originals):
            points=reduced[tri]
            q=[(p[0],p[2],-p[1]) for p in points]
            tex=project_uv(faces[original][0],faces[original][2],points)
            result.append((faces[original][4] if len(faces[original])>4 else 0,faces[original][1],q,tex,faces[original][3]))
        return result
    per_wheel=min(24,budget//10);wheel=selected(wheel,per_wheel);body=selected(body,budget-4*len(wheel));records=[(color,points,uv,slot,part) for part,color,points,uv,slot in body]
    for label,matrix in frames:
        if not (label.startswith('wheel_') and label.endswith('_dummy')):continue
        center=transform(matrix,(0,0,0));side=-1 if center[0]<0 else 1
        for _,color,points,uv,slot in wheel:
            placed=[(center[0]+side*p[0]*scale,center[2]+p[1]*scale,-center[1]+p[2]*scale) for p in points]
            records.append((color,placed,uv,slot,next(i for i,(n,_) in enumerate(parts) if n==label)))
    base=min(p[1] for _,points,_,_,_ in records for p in points)
    low=[]
    if build_lod:
        for kind,_,atomic in chunks(clump):
            if kind!=0x14:continue
            _,spec=one(atomic,1);frame,geo,flags,_=struct.unpack('<4I',spec)
            label,matrix=frames[frame]
            if not flags&4 or '_vlo' not in label:continue
            # Read the body geometry list again (the wheel list above is separate).
            _,bodylist=one(clump,0x1a);bodygeos=[p for k,_,p in chunks(bodylist) if k==0xf]
            vertices,faces,uv,_,materials=detailed_geometry(bodygeos[geo])
            vertices=[transform(matrix,p) for p in vertices]
            for a,b,c,mat in faces:
                pts=[(vertices[i][0],vertices[i][2],-vertices[i][1]) for i in (a,b,c)]
                part=0
                for i,(_,center) in enumerate(parts[1:5],1):
                    if all(abs(p[0]-center[0])<.3 and (p[1]-center[1])**2+(p[2]-center[2])**2<(.7*scale)**2 for p in pts):part=i;break
                low.append((material_color(materials[mat],uv,(a,b,c),textures,paint),pts,np.asarray([uv[i] for i in (a,b,c)]),atlas.add(materials[mat],textures,paint),part))
        if not low or len(low)>160:raise ValueError('Missing or oversized original vehicle LOD')
    payload=[];uv_payload=[]
    for color,points,uv,slot,part in records:
        coords=[round((v-(base if k==1 else 0))*64) for p in points for k,v in enumerate(p)]
        payload.append(struct.pack('<H9h',color,*coords))
        uv_payload.append(struct.pack('<6h',*np.rint(atlas.uv(slot,uv)*texture_size/256).astype(int).flatten()))
    if len(payload)>budget:raise ValueError('Vehicle exceeds polygon allocation')
    output.write_bytes(struct.pack('<4sI',b'VCM1',len(payload))+b''.join(payload))
    if not shared_atlas:atlas.save(output.with_suffix('.atx'))
    if texture_size!=256 and not shared_atlas:
        from PIL import Image
        from actor_texture import save_texture
        raw=output.with_suffix('.atx').read_bytes();pal=np.frombuffer(raw[8:520],'<u2');idx=np.frombuffer(raw[520:],'u1').reshape(256,256)
        rgb=np.stack(((pal&31)*255//31,((pal>>5)&31)*255//31,((pal>>10)&31)*255//31),axis=1).astype('u1')
        save_texture(Image.fromarray(rgb[idx]).resize((texture_size,texture_size),Image.Resampling.LANCZOS),output.with_suffix('.atx'))
    output.with_suffix('.vcu').write_bytes(struct.pack('<4sI',b'VCU1',len(payload))+b''.join(uv_payload))
    pivots=[round((v-(base if k==1 else 0))*64) for _,p in parts for k,v in enumerate(p)]
    output.with_suffix('.vpa').write_bytes(struct.pack('<4sHH',b'VPA1',len(payload),len(parts))+struct.pack('<21h',*pivots)+bytes(r[4] for r in records))
    if build_lod:
        payload_low=[];uv_low=[]
        for color,pts,uv,slot,part in low:
            coords=[round((v-(base if k==1 else 0))*64) for p in pts for k,v in enumerate(p)]
            payload_low.append(struct.pack('<H9h',color,*coords));uv_low.append(struct.pack('<6h',*np.rint(atlas.uv(slot,uv)*texture_size/256).astype(int).flatten()))
        dest=output.with_name(output.stem+'-low')
        dest.with_suffix('.vcm').write_bytes(struct.pack('<4sI',b'VCM1',len(low))+b''.join(payload_low))
        dest.with_suffix('.vcu').write_bytes(struct.pack('<4sI',b'VCU1',len(low))+b''.join(uv_low))
        dest.with_suffix('.vpa').write_bytes(struct.pack('<4sHH',b'VPA1',len(low),7)+struct.pack('<21h',*pivots)+bytes(r[4] for r in low))
    seat=transform(next(m for n,m in frames if n=='ped_frontseat'),(0,0,0))
    centers=dict(parts);front=centers['wheel_lf_dummy'];rear=centers['wheel_lb_dummy']
    return {'source':name+'.dff','file':output.name,'triangles':len(payload),'wheel_triangles':len(wheel)*4,'paint':paint,
            'excluded_parts':['distant vehicle LOD','damaged alternatives','extra2'],
            'simplification':'Connected edge collapse; surviving surface coverage retained',
            'parts':{n:sum(r[4]==i for r in records) for i,(n,_) in enumerate(parts)},
            'seat':[-abs(seat[0]),seat[2]-base,-seat[1]],'wheel_radius':min(p[1]-base for _,p in parts[1:5]),'wheelbase':abs(front[2]-rear[2]),'low_triangles':len(low),'texture_size':texture_size,
            'limitations':f'Bounded mesh with original articulated wheel and front-door pivots; source raster textures in a {texture_size}-pixel atlas'}
