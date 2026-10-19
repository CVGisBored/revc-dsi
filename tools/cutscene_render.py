"""Offline original-asset cutscene renderer, using Mesa EGL and full source geometry.
Original camera Beziers follow reVC Cam.cpp. The output is a DSi streaming cache.
"""
import os
os.environ.setdefault('PYOPENGL_PLATFORM','egl');os.environ.setdefault('EGL_PLATFORM','surfaceless')
import struct,math,re
from pathlib import Path
import numpy as np
from PIL import Image
from OpenGL import EGL,GL,GLU
from ctypes import c_int
from assets import chunks,one,place,instances,Archive
from materials import detailed_geometry,detailed_dff,model_textures,txd
from stage_animation import skin,pose,matrix,rotation

class Cuts:
    def __init__(self,game):
        self.path=game/'anim/cuts.img'
        self.entries={n.split(b'\0')[0].decode().lower():(off*2048,size*2048) for off,size,n in struct.iter_unpack('<II24s',(game/'anim/cuts.dir').read_bytes())}
    def read(self,name):
        off,size=self.entries[name]
        with self.path.open('rb') as f:f.seek(off);b=f.read(size)
        if len(b)!=size:raise ValueError('Truncated cutscene archive')
        return b

def cameras(data):
    result=[]
    for width,part in zip((4,4,10,10),data.split(b'\0')[0].decode().split(';')):
        nums=[float(v) for v in re.findall(r'[-+]?(?:\d*\.\d+|\d+)(?:[eE][-+]?\d+)?',part)]
        count=int(nums[0]);values=np.asarray(nums[1:1+count*width]).reshape(count,width)
        if len(nums)<1+count*width:raise ValueError('Invalid camera spline')
        result.append(values)
    return result

def spline(keys,time):
    # Incoming/outgoing control points surround each source key. <=75 ms pairs are camera cuts.
    hi=min(len(keys)-1,max(1,int(np.searchsorted(keys[:,0],time,side='right'))))
    while hi<len(keys)-1 and keys[hi,0]-keys[hi-1,0]<=.075:hi+=1
    a,b=keys[hi-1],keys[hi];u=np.clip((time-a[0])/max(1e-6,b[0]-a[0]),0,1);v=1-u
    n=(len(a)-1)//3
    return v*v*v*a[1:1+n]+3*u*v*v*a[1+2*n:1+3*n]+3*u*u*v*b[1+n:1+2*n]+u*u*u*b[1:1+n]

def frame_globals(local,parents,tags,names,nodes,time):
    result=[]
    for i,rest in enumerate(local):
        m=rest.copy();keys=nodes.get(tags[i],nodes.get(names[i]))
        if keys is not None:
            hi=min(len(keys)-1,int(np.searchsorted(keys[:,-1],time,side='right')));lo=max(0,hi-1)
            a,b=keys[lo],keys[hi];u=0 if b[-1]==a[-1] else np.clip((time-a[-1])/(b[-1]-a[-1]),0,1)
            q=a[:4].copy();r=b[:4].copy()
            if np.dot(q,r)<0:r=-r
            m[:3,:3]=rotation(q*(1-u)+r*u)
            if len(a)>5:m[:3,3]=a[4:7]*(1-u)+b[4:7]*u
        result.append(result[parents[i]]@m if parents[i]>=0 else m)
    return result

class Actor:
    def __init__(self,archive,spec,animations,renderer):
        self.spec=spec;data=archive.read(spec['model']+'.dff');self.nodes=animations[spec['animation']]
        self.skinned=None
        try:self.skinned=skin(data)
        except (ValueError,KeyError):pass
        clump=list(chunks(data,padded=True))[0][2];_,flist=one(clump,0xe);_,raw=one(flist,1);count,=struct.unpack_from('<I',raw)
        self.local=[];self.parents=[];self.tags=[];self.names=[]
        extensions=[p for k,_,p in chunks(flist) if k==3]
        for i in range(count):
            values=struct.unpack_from('<12fiI',raw,4+i*56);self.local.append(matrix(values));self.parents.append(values[12])
            plugins={k:p for k,_,p in chunks(extensions[i])};self.tags.append(struct.unpack_from('<i',plugins[0x11e],4)[0] if 0x11e in plugins else -999)
            self.names.append(plugins.get(0x253f2fe,b'').split(b'\0')[0].decode().lower())
        _,glist=one(clump,0x1a);geos=[detailed_geometry(p) for k,_,p in chunks(glist) if k==0xf]
        self.groups=[];textures=txd(archive.read(spec['model']+'.txd'))
        for k,_,body in chunks(clump):
            if k!=0x14:continue
            _,raw=one(body,1);frame,geo,flags,_=struct.unpack('<4I',raw)
            if not flags&4:continue
            v,faces,uv,light,mats=geos[geo]
            self.groups.append((frame,np.asarray(v),renderer.batches(faces,uv,light,mats,textures)))
        self.globals=[]
    def update(self,time):self.globals=frame_globals(self.local,self.parents,self.tags,self.names,self.nodes,time)
    def draw(self,renderer,time,parent=None):
        deform=pose(self.skinned,self.nodes,time) if self.skinned is not None else None
        for frame,vertices,batches in self.groups:
            vertices=deform if deform is not None else np.column_stack((vertices,np.ones(len(vertices))))@self.globals[frame].T
            if parent is not None:vertices=np.column_stack((vertices[:,:3],np.ones(len(vertices))))@parent.T
            points=vertices[:,:3].copy();points[:,2]+=1
            renderer.draw_batches(points,batches)

class Renderer:
    def __init__(self,game):
        display=EGL.eglGetDisplay(EGL.EGL_DEFAULT_DISPLAY);a,b=c_int(),c_int();EGL.eglInitialize(display,a,b);EGL.eglBindAPI(EGL.EGL_OPENGL_API)
        attrs=(c_int*7)(EGL.EGL_SURFACE_TYPE,EGL.EGL_PBUFFER_BIT,EGL.EGL_RENDERABLE_TYPE,EGL.EGL_OPENGL_BIT,EGL.EGL_DEPTH_SIZE,24,EGL.EGL_NONE)
        configs=(EGL.EGLConfig*1)();n=c_int();EGL.eglChooseConfig(display,attrs,configs,1,n)
        size=(c_int*5)(EGL.EGL_WIDTH,256,EGL.EGL_HEIGHT,144,EGL.EGL_NONE)
        self.surface=EGL.eglCreatePbufferSurface(display,configs[0],size);self.context=EGL.eglCreateContext(display,configs[0],EGL.EGL_NO_CONTEXT,None)
        EGL.eglMakeCurrent(display,self.surface,self.surface,self.context);self.display=display
        GL.glViewport(0,0,256,144);GL.glEnable(GL.GL_DEPTH_TEST);GL.glDisable(GL.GL_CULL_FACE);GL.glEnable(GL.GL_ALPHA_TEST);GL.glAlphaFunc(GL.GL_GREATER,.35)
        GL.glEnableClientState(GL.GL_VERTEX_ARRAY);GL.glEnableClientState(GL.GL_COLOR_ARRAY);GL.glEnableClientState(GL.GL_TEXTURE_COORD_ARRAY)
        self.archive=Archive(game);self.texture_names=model_textures(game);self.placements=instances(game);self.static=[];self.textures=[];self.texcache={}
    def texture(self,image):
        key=hash(image.tobytes())
        if key in self.texcache:return self.texcache[key]
        handle=GL.glGenTextures(1);self.textures.append(handle);self.texcache[key]=handle;GL.glBindTexture(GL.GL_TEXTURE_2D,handle)
        image=image.convert('RGBA');GL.glTexImage2D(GL.GL_TEXTURE_2D,0,GL.GL_RGBA,image.width,image.height,0,GL.GL_RGBA,GL.GL_UNSIGNED_BYTE,np.asarray(image))
        for axis in (GL.GL_TEXTURE_WRAP_S,GL.GL_TEXTURE_WRAP_T):GL.glTexParameteri(GL.GL_TEXTURE_2D,axis,GL.GL_REPEAT)
        for axis in (GL.GL_TEXTURE_MIN_FILTER,GL.GL_TEXTURE_MAG_FILTER):GL.glTexParameteri(GL.GL_TEXTURE_2D,axis,GL.GL_LINEAR)
        return handle
    def batches(self,faces,uv,light,materials,textures):
        batches=[];uv=np.asarray(uv,dtype='f4');light=np.asarray(light,dtype='f4')/255
        # Degenerate source UVs are already removed by materials.detailed_geometry.
        for index,(color,name) in enumerate(materials):
            idx=np.asarray([(a,b,c) for a,b,c,m in faces if m==index],dtype='u4').reshape(-1)
            if not len(idx):continue
            pixels=textures.get(name);handle=self.texture(pixels) if pixels is not None else 0
            rgba=light*np.asarray(color,dtype='f4')/255
            # Interior prelights are preserved, but unlit actors have a modest source-like ambient fill.
            batches.append((idx,uv,np.ascontiguousarray(rgba,dtype='f4'),handle))
        return batches
    def draw_batches(self,vertices,batches):
        vertices=np.ascontiguousarray(vertices,dtype='f4');GL.glVertexPointer(3,GL.GL_FLOAT,0,vertices)
        for idx,uv,rgba,handle in batches:
            if handle:GL.glEnable(GL.GL_TEXTURE_2D);GL.glBindTexture(GL.GL_TEXTURE_2D,handle)
            else:GL.glDisable(GL.GL_TEXTURE_2D)
            GL.glTexCoordPointer(2,GL.GL_FLOAT,0,uv);GL.glColorPointer(4,GL.GL_FLOAT,0,rgba)
            GL.glDrawElements(GL.GL_TRIANGLES,len(idx),GL.GL_UNSIGNED_INT,idx)
    def background(self,spec):
        self.static=[];origin=np.asarray(spec['origin']);interior=spec['name'] in ('int_a','int_b')
        for obj in self.placements:
            if 'lod' in obj.name or obj.name.endswith('_nt'):continue
            if interior:
                if not obj.source.endswith('hotel.ipl') or obj.position[2]<17:continue
            elif np.linalg.norm(np.asarray(obj.position)-origin)>180:continue
            if not interior and obj.source.endswith('hotel.ipl'):continue
            try:
                textures=txd(self.archive.read(self.texture_names[obj.model_id]+'.txd'))
                for v,faces,uv,light,mats in detailed_dff(self.archive.read(obj.name+'.dff')):
                    points=np.asarray([place(obj,p) for p in v])-origin
                    self.static.append((points,self.batches(faces,uv,light,mats,textures)))
            except KeyError:continue
        if spec['name'] in ('law_1b','col_1','col_2'):
            # These yacht meshes are CREATE_OBJECT_NO_OFFSET objects in the
            # main SCM, so the IPL-only background pass cannot discover them.
            anchor=np.asarray((-375.4989929199219,-1322.31005859375,9.811240196228027))
            for model,texture in (('yt_main_body','yacht'),('yt_main_body2','yacht'),('yt_doors14','yacht'),('yt_tmp_boat','yacht'),('big_poly_tings','yachttings')):
                textures=txd(self.archive.read(texture+'.txd'))
                for v,faces,uv,light,mats in detailed_dff(self.archive.read(model+'.dff')):
                    points=np.asarray(v)+anchor-origin
                    self.static.append((points,self.batches(faces,uv,light,mats,textures)))
    def render(self,spec,paths,actors,time):
        eye=spline(paths[2],time);target=spline(paths[3],time);roll=math.radians(float(spline(paths[1],time)[0]))
        forward=target-eye;forward/=np.linalg.norm(forward);up=np.array([math.cos(roll+math.pi/2),0,math.sin(roll+math.pi/2)])
        left=np.cross(up,forward);up=np.cross(forward,left);up/=np.linalg.norm(up)
        fov=float(spline(paths[0],time)[0]);vfov=math.degrees(2*math.atan(math.tan(math.radians(fov/2))/(256/144)))
        GL.glClearColor(.045,.045,.065,1) if spec['name'] in ('int_a','int_b') else GL.glClearColor(.12,.12,.20,1)
        GL.glClear(GL.GL_COLOR_BUFFER_BIT|GL.GL_DEPTH_BUFFER_BIT);GL.glMatrixMode(GL.GL_PROJECTION);GL.glLoadIdentity();GLU.gluPerspective(vfov,256/144,.1,350)
        GL.glMatrixMode(GL.GL_MODELVIEW);GL.glLoadIdentity();GLU.gluLookAt(*eye,*target,*up)
        for points,batches in self.static:self.draw_batches(points,batches)
        for actor in actors:actor.update(time)
        byhandle={tuple(a.spec['handle']):a for a in actors}
        for actor in actors:
            parent=None
            if 'attach' in actor.spec:
                handle,component=actor.spec['attach'];other=byhandle[tuple(handle)]
                if component not in other.names:raise ValueError(f'Missing attached component {component}')
                parent=other.globals[other.names.index(component)]
            actor.draw(self,time,parent)
        GL.glFinish();raw=GL.glReadPixels(0,0,256,144,GL.GL_RGB,GL.GL_UNSIGNED_BYTE)
        return Image.frombytes('RGB',(256,144),raw).transpose(Image.Transpose.FLIP_TOP_BOTTOM)
