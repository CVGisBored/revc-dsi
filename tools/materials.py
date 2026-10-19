"""Original PC RenderWare materials, UVs and texture rasters for world conversion."""
from pathlib import Path
import struct
import numpy as np
from PIL import Image
from assets import chunks,one,geometry,rw_version,compose,transform,manifest,lines,FormatError,resolve

def model_textures(game):
    result={}
    for path in manifest(game,'IDE'):
        section=''
        for _,text in lines(path):
            if ',' not in text:section=text.lower();continue
            if section not in ('objs','tobj','anim','cars','peds','weap'):continue
            fields=[v.strip() for v in text.split(',')]
            result[int(fields[0])]=fields[2].lower()
    return result

def detailed_geometry(data):
    vertices,triangles=geometry(data)
    version,body=one(data,1);flags,nt,nv,_=struct.unpack_from('<4I',body)
    tex=(flags>>16)&255 or (2 if flags&0x80 else 1 if flags&4 else 0)
    pos=16+(12 if rw_version(version)<0x34000 else 0)
    prelight=[(255,255,255,255)]*nv
    if flags&8:
        prelight=list(struct.iter_unpack('<4B',body[pos:pos+nv*4]));pos+=nv*4
    uv=list(struct.iter_unpack('<2f',body[pos:pos+nv*8])) if tex else [(0.,0.)]*nv
    pos+=tex*nv*8
    faces=[(a,b,c,m) for b,a,m,c in struct.iter_unpack('<4H',body[pos:pos+nt*8])]
    _,mlist=one(data,8);_,spec=one(mlist,1)
    n,=struct.unpack_from('<I',spec);mapping=struct.unpack_from('<'+str(n)+'i',spec,4)
    raw=iter(p for k,_,p in chunks(mlist) if k==7);materials=[]
    for index in mapping:
        if index>=0:
            if index>=len(materials):raise FormatError('Invalid material reference')
            materials.append(materials[index]);continue
        mat=next(raw);_,s=one(mat,1);color=tuple(s[4:8]);name=''
        if struct.unpack_from('<I',s,12)[0]:
            _,texture=one(mat,6);names=[p for k,_,p in chunks(texture) if k==2]
            name=names[0].split(b'\0')[0].decode('ascii').lower()
        materials.append((color,name))
    if any(m>=len(materials) for *_,m in faces):raise FormatError('Face material outside list')
    # Some untextured meshes contain uninitialized UV slots. Preserve geometry,
    # and only remove texturing from faces that actually reference invalid UVs.
    bad={i for i,p in enumerate(uv) if not all(np.isfinite(p))}
    if bad:
        fallback={}
        fixed=[]
        for a,b,c,m in faces:
            if materials[m][1] and bad.intersection((a,b,c)):
                if m not in fallback:fallback[m]=len(materials);materials.append((materials[m][0],''))
                m=fallback[m]
            fixed.append((a,b,c,m))
        faces=fixed;uv=[(0.,0.) if i in bad else p for i,p in enumerate(uv)]
    return vertices,faces,uv,prelight,materials

def detailed_dff(data,skip_frames=()):
    clump=list(chunks(data,padded=True))[0][2];_,flist=one(clump,0xe);_,body=one(flist,1)
    count,=struct.unpack_from('<I',body);matrices=[]
    extensions=[p for k,_,p in chunks(flist) if k==3]
    names=[]
    for extension in extensions:
        plugins={k:p for k,_,p in chunks(extension)}
        names.append(plugins.get(0x253f2fe,b'').split(b'\0')[0].decode('ascii').lower())
    for i in range(count):
        v=struct.unpack_from('<12fiI',body,4+i*56);parent=v[12]
        if not -1<=parent<i:raise FormatError('Invalid frame parent')
        matrices.append(compose(matrices[parent],v[:12]) if parent>=0 else v[:12])
    _,glist=one(clump,0x1a);geos=[detailed_geometry(p) for k,_,p in chunks(glist) if k==0xf]
    result=[]
    for kind,_,atomic in chunks(clump):
        if kind!=0x14:continue
        _,spec=one(atomic,1);frame,geo,flags,_=struct.unpack('<4I',spec)
        if not flags&4:continue
        if frame<len(names) and any(term in names[frame] for term in skip_frames):continue
        vertices,*attributes=geos[geo]
        result.append(([transform(matrices[frame],p) for p in vertices],*attributes))
    return result

def txd(data):
    dictionary=list(chunks(data,padded=True))[0][2];result={}
    for kind,_,native in chunks(dictionary):
        if kind!=0x15:continue
        _,s=one(native,1);platform,filtering=struct.unpack_from('<II',s)
        if platform not in (8,9):raise FormatError('Unsupported native texture platform')
        name=s[8:40].split(b'\0')[0].decode('ascii').lower()
        fmt,extra,w,h,depth,levels,_,compression=struct.unpack_from('<IIHH4B',s,72)
        has_alpha=bool(extra) if platform==8 else bool(compression&1)
        if not 0<w<=4096 or not 0<h<=4096 or not levels:raise FormatError('Invalid texture size')
        if platform==9:compression={b'DXT1':1,b'DXT3':3,b'DXT5':5}.get(struct.pack('<I',extra),0) if compression&8 else 0
        pos=88;palette=None
        if fmt&0x6000:
            n=32 if fmt&0x4000 else 256
            palette=np.frombuffer(s,'u1',n*4,pos).reshape(n,4).copy();pos+=n*4
        size,=struct.unpack_from('<I',s,pos);raw=s[pos+4:pos+4+size]
        if len(raw)!=size:raise FormatError('Truncated raster')
        if compression:
            n={1:1,3:2,5:3}[compression]
            image=Image.frombytes('RGBA',(w,h),raw,'bcn',(n,'DXT'+str(compression)))
        elif palette is not None:
            if depth==4 and size==(w*h+1)//2:
                packed=np.frombuffer(raw,'u1');indices=np.column_stack((packed&15,packed>>4)).flatten()[:w*h]
            else:indices=np.frombuffer(raw,'u1',w*h)
            image=Image.fromarray(palette[indices].reshape(h,w,4))
        elif depth==32:image=Image.frombytes('RGBA',(w,h),raw,'raw','BGRA')
        elif depth==24:image=Image.frombytes('RGB',(w,h),raw,'raw','BGR').convert('RGBA')
        elif depth==16:
            values=np.frombuffer(raw,'<u2',w*h).reshape(h,w).astype(np.uint32);code=fmt&0xf00
            if code==0x200:  # RGB565
                rgba=np.stack(((values>>11)*255//31,((values>>5)&63)*255//63,(values&31)*255//31,np.full_like(values,255)),axis=-1)
            elif code in (0x100,0xa00):
                rgba=np.stack((((values>>10)&31)*255//31,((values>>5)&31)*255//31,(values&31)*255//31,
                               ((values>>15)*255 if code==0x100 else np.full_like(values,255))),axis=-1)
            elif code==0x300:
                rgba=np.stack((((values>>8)&15)*17,((values>>4)&15)*17,(values&15)*17,(values>>12)*17),axis=-1)
            else:raise FormatError(f'Unsupported raster format {fmt:x}')
            image=Image.fromarray(rgba.astype('u1'))
        else:raise FormatError(f'Unsupported raster depth {depth}')
        if not has_alpha:image.putalpha(255)
        result[name]=image
    return result

def handheld_texture(image):
    original=image.convert('RGBA').resize((64,64),Image.Resampling.LANCZOS)
    alpha=np.asarray(original)[:,:,3]<128;transparent=bool(alpha.any())
    rgb=original.convert('RGB');quant=rgb.quantize(colors=255 if transparent else 256)
    indices=np.asarray(quant).astype('u1');pal=np.array(quant.getpalette()[:768],dtype='u1').reshape(-1,3)
    pal=pal[:255 if transparent else 256]
    colors=np.zeros((256,3),dtype='u1')
    if transparent:
        indices=indices+1;indices[alpha]=0;colors[1:1+len(pal)]=pal
    else:colors[:len(pal)]=pal
    colors16=colors.astype(np.uint16)>>3
    palette=(colors16[:,0]|colors16[:,1]<<5|colors16[:,2]<<10).astype('<u2')
    error=float(np.abs(np.asarray(rgb).astype(int)-colors[indices].astype(int))[~alpha].mean()) if (~alpha).any() else 0
    return struct.pack('<I',int(transparent))+palette.tobytes()+indices.tobytes(),error

class TextureRegistry:
    def __init__(self,game,archive):
        self.game=game;self.archive=archive;self.dictionaries={};self.ids={};self.records=[];self.report=[];self.missing=set()
    def dictionary(self,name):
        if name not in self.dictionaries:
            key=name+'.txd'
            try:
                data=self.archive.read(key) if key in self.archive.entries else resolve(self.game,'models/'+key).read_bytes()
                self.dictionaries[name]=txd(data)
            except (KeyError,FormatError,ValueError) as error:
                self.dictionaries[name]={};self.missing.add((name,'dictionary: '+str(error)))
        return self.dictionaries[name]
    def get(self,dictionary,name):
        if not name:return 0
        key=(dictionary,name)
        if key in self.ids:return self.ids[key]
        image=self.dictionary(dictionary).get(name)
        if image is None:
            for common in ('generic','particle'):
                image=self.dictionary(common).get(name)
                if image is not None:break
        if image is None:
            self.missing.add(key);self.ids[key]=0;return 0
        record,error=handheld_texture(image);identity=len(self.records)+1
        if identity>=32768:raise FormatError('Too many textures')
        self.ids[key]=identity;self.records.append(record)
        self.report.append({'id':identity,'dictionary':dictionary,'name':name,'size':list(image.size),'mean_rgb_error':round(error,3)})
        return identity
    def write(self,path):path.write_bytes(struct.pack('<4sI',b'VTX1',len(self.records))+b''.join(self.records))
