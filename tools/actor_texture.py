"""DS paletted actor textures and per-material vehicle atlases."""
import struct
from pathlib import Path
import numpy as np
from PIL import Image

def save_texture(image,path):
    image=image.convert('RGB');indexed=image.quantize(colors=256,method=Image.Quantize.MEDIANCUT)
    rgb=np.asarray(indexed.getpalette(),dtype=np.uint16).reshape(-1,3)
    palette=((rgb[:,0]>>3)|((rgb[:,1]>>3)<<5)|((rgb[:,2]>>3)<<10)).astype('<u2').tobytes().ljust(512,b'\0')
    Path(path).write_bytes(struct.pack('<4sHH',b'ATX1',image.width,image.height)+palette+np.asarray(indexed,dtype=np.uint8).tobytes())

def project_uv(original,uv,points):
    a,b,c=np.asarray(original);edge=np.column_stack((b-a,c-a));coeff=np.linalg.lstsq(edge,(np.asarray(points)-a).T,rcond=None)[0].T
    uv=np.asarray(uv);return uv[0]+coeff[:,0:1]*(uv[1]-uv[0])+coeff[:,1:2]*(uv[2]-uv[0])

class Atlas:
    def __init__(self):self.tiles=[];self.keys={}
    def add(self,mat,textures,paint):
        color,name=mat;rgb=np.asarray(color[:3],float)
        if tuple(color[:3])==(60,255,0):rgb=np.asarray(paint[0],float)
        elif tuple(color[:3])==(255,0,175):rgb=np.asarray(paint[1],float)
        key=(tuple(rgb),name,color[3])
        if key not in self.keys:
            image=textures.get(name)
            if image is None:pixels=np.ones((64,64,3))*255
            else:pixels=np.asarray(image.convert('RGB').resize((64,64),Image.Resampling.LANCZOS),float)
            pixels=pixels*rgb/255
            if color[3]<255:pixels*=.35
            self.keys[key]=len(self.tiles);self.tiles.append(Image.fromarray(np.clip(pixels,0,255).astype('u1')))
        return self.keys[key]
    def size(self):return 64 if len(self.tiles)<=16 else 32
    def uv(self,slot,uv):
        tile=self.size();columns=256//tile;xy=np.array([(slot%columns)*tile,(slot//columns)*tile])
        return (xy+1+np.clip(uv,0,1)*(tile-2))*16
    def save(self,path):
        tile=self.size();columns=256//tile
        if len(self.tiles)>columns*columns:raise ValueError('Vehicle atlas exceeds capacity')
        image=Image.new('RGB',(256,256))
        for i,picture in enumerate(self.tiles):image.paste(picture.resize((tile,tile),Image.Resampling.LANCZOS),((i%columns)*tile,(i//columns)*tile))
        save_texture(image,path)

class SharedVehicleAtlas:
    """One resident 256x128 atlas; identical rasters/solid paints share storage.

    Collect all materials before freeze(). UVs remain stable during conversion.
    Original rasters are resized and quantized once, instead of quantizing a
    mostly empty 256px per-car atlas and then shrinking/quantizing it again.
    """
    def __init__(self):self.images=[];self.keys={};self.rects=None;self.report={};self.limits=[]
    def add(self,mat,textures,paint):
        import hashlib
        color,name=mat;rgb=np.asarray(color[:3],float)
        if tuple(color[:3])==(60,255,0):rgb=np.asarray(paint[0],float)
        elif tuple(color[:3])==(255,0,175):rgb=np.asarray(paint[1],float)
        source=textures.get(name)
        pixels=np.asarray(source.convert('RGB'),float) if source is not None else np.ones((1,1,3))*255
        pixels=pixels*rgb/255
        if color[3]<255:pixels*=.35
        pixels=np.clip(pixels,0,255).astype('u1')
        # Untextured glass and constant-color paint need only one texel.
        if np.all(pixels==pixels[0,0]):pixels=pixels[:1,:1]
        key=(pixels.shape,hashlib.sha256(pixels.tobytes()).digest())
        if key not in self.keys:
            if self.rects is not None:raise ValueError('Uncollected vehicle material')
            self.keys[key]=len(self.images);self.images.append(Image.fromarray(pixels));self.limits.append(0)
        limit=32 if max(pixels.shape[:2])<=64 else 256
        self.limits[self.keys[key]]=max(limit,self.limits[self.keys[key]])
        return self.keys[key]
    def freeze(self):
        # MaxRects packing with a duplicated edge texel around every material.
        for large in (64,56,48,44,40,36,32):
            sizes=[tuple(max(1,round(d*min(1,large/max(im.size),limit/max(im.size)))) for d in im.size) for im,limit in zip(self.images,self.limits)]
            free=[(0,0,256,128)];rects={}
            for i in sorted(range(len(sizes)),key=lambda i:max(sizes[i]),reverse=True):
                w,h=(d+2 for d in sizes[i]);fits=[r for r in free if w<=r[2] and h<=r[3]]
                if not fits:break
                x,y,_,_=min(fits,key=lambda r:(r[1],r[0],r[2]*r[3]-w*h))
                rects[i]=(x+1,y+1,*sizes[i]);remaining=[]
                for fx,fy,fw,fh in free:
                    if x>=fx+fw or x+w<=fx or y>=fy+fh or y+h<=fy:remaining.append((fx,fy,fw,fh));continue
                    if x>fx:remaining.append((fx,fy,x-fx,fh))
                    if x+w<fx+fw:remaining.append((x+w,fy,fx+fw-x-w,fh))
                    if y>fy:remaining.append((fx,fy,fw,y-fy))
                    if y+h<fy+fh:remaining.append((fx,y+h,fw,fy+fh-y-h))
                free=[a for k,a in enumerate(remaining) if not any(j!=k and b[0]<=a[0] and b[1]<=a[1] and b[0]+b[2]>=a[0]+a[2] and b[1]+b[3]>=a[1]+a[3] and (a!=b or j<k) for j,b in enumerate(remaining))]
            if len(rects)==len(sizes):
                self.rects=rects;self.report={'size':[256,128],'unique_materials':len(sizes),'large_raster_edge':large,'content_pixels':sum(w*h for w,h in sizes)};return
        raise ValueError('Vehicle atlas exceeds 32 KiB allocation')
    def uv(self,slot,uv):
        if self.rects is None:raise ValueError('Freeze atlas before assigning UVs')
        x,y,w,h=self.rects[slot]
        return (np.array([x+.5,y+.5])+np.clip(uv,0,1)*np.array([w-1,h-1]))*16
    def save(self,path):
        if self.rects is None:raise ValueError('Freeze atlas before saving')
        image=Image.new('RGB',(256,128))
        for i,(x,y,w,h) in self.rects.items():
            tile=np.asarray(self.images[i].resize((w,h),Image.Resampling.LANCZOS))
            image.paste(Image.fromarray(np.pad(tile,((1,1),(1,1),(0,0)),mode='edge')),(x-1,y-1))
        save_texture(image,path)

class PedestrianAtlas:
    """Six 32x64 source-raster tiles in one 128x128 resident page."""
    def __init__(self):self.images=[]
    def add(self,image):
        if len(self.images)>=8:raise ValueError('Pedestrian atlas full')
        self.images.append(image.convert('RGB'));return len(self.images)-1
    def uv(self,slot,uv):
        return (np.array([(slot%4)*32+1.5,(slot//4)*64+1.5])+np.clip(uv,0,1)*np.array([29,61]))*16
    def save(self,path):
        image=Image.new('RGB',(128,128))
        for i,im in enumerate(self.images):
            tile=np.asarray(im.resize((30,62),Image.Resampling.LANCZOS))
            image.paste(Image.fromarray(np.pad(tile,((1,1),(1,1),(0,0)),mode='edge')),((i%4)*32,(i//4)*64))
        save_texture(image,path)
