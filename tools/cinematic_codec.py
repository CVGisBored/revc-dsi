"""VCV2: independently seekable keyframes and lossless XOR/LZ10 video deltas."""
import struct
import numpy as np
from PIL import Image
from compact_assets import lz10,unpack
KEY=0x80000000

def palette_image(images):
    sheet=Image.new('RGB',(128,72*len(images)))
    for i,image in enumerate(images):sheet.paste(image.convert('RGB').resize((128,72),Image.Resampling.BOX),(0,72*i))
    return sheet.quantize(colors=256,method=Image.Quantize.MEDIANCUT)

def raw_frame(image,palette):
    im=image.convert('RGB').resize((128,72),Image.Resampling.BOX).quantize(palette=palette,dither=Image.Dither.NONE)
    rgb=np.asarray(palette.getpalette(),dtype=np.uint16).reshape(-1,3)
    colors=((rgb[:,0]>>3)|((rgb[:,1]>>3)<<5)|((rgb[:,2]>>3)<<10)|0x8000).astype('<u2')
    return colors.tobytes().ljust(512,b'\0')+im.tobytes()

def encode_frame(raw,previous,key):
    if key:return lz10(raw),True
    delta=bytes(np.bitwise_xor(np.frombuffer(raw,dtype='u1'),np.frombuffer(previous,dtype='u1')))
    return lz10(delta),False

def decode_frame(packed,previous,key):
    raw=unpack(packed)
    if key:return raw
    if previous is None or len(previous)!=len(raw):raise ValueError('Missing delta base')
    return bytes(np.bitwise_xor(np.frombuffer(raw,dtype='u1'),np.frombuffer(previous,dtype='u1')))

_similarity_cache={}
def encode_motion(raw,previous,key,tolerance=1):
    """4x4-pixel motion search on 8x8 blocks, with <=1 RGB555 unit hysteresis.

    All 30 source poses are rendered. This stabilizes tiny colour fluctuations;
    it does not duplicate or interpolate low-rate source frames.
    """
    if key:return lz10(raw),True,raw
    assert len(raw)==len(previous)==9728
    pal=raw[:512]
    cache_key=(pal,tolerance)
    if cache_key not in _similarity_cache:
        colors=np.frombuffer(pal,dtype='<u2');rgb=np.stack([(colors>>s)&31 for s in (0,5,10)],axis=1).astype('i2')
        _similarity_cache[cache_key]=np.max(np.abs(rgb[:,None,:]-rgb[None,:,:]),axis=2)<=tolerance
    similar=_similarity_cache[cache_key]
    cur=np.frombuffer(raw[512:],dtype='u1').reshape(72,128);old=np.frombuffer(previous[512:],dtype='u1').reshape(72,128)
    target=cur.reshape(9,8,16,8).transpose(0,2,1,3)
    pad=np.pad(old,4,mode='edge');score=np.full((9,16),65);predict=np.zeros_like(target);vectors=np.full((9,16),40,dtype='u1')
    offsets=sorted(((dx,dy) for dy in range(-4,5) for dx in range(-4,5)),key=lambda p:abs(p[0])+abs(p[1]))
    xx=np.arange(16)*8;yy=np.arange(9)*8
    for dx,dy in offsets:
        block=pad[4+dy:4+dy+72,4+dx:4+dx+128].reshape(9,8,16,8).transpose(0,2,1,3)
        cost=np.count_nonzero(~similar[block,target],axis=(2,3));valid=(xx[None,:]+dx>=0)&(xx[None,:]+dx+7<128)&(yy[:,None]+dy>=0)&(yy[:,None]+dy+7<72)
        better=valid&(cost<score)
        predict[better]=block[better];vectors[better]=(dy+4)*9+dx+4;score[better]=cost[better]
    stable=np.where(similar[predict,target],predict,target).astype('u1');residual=stable^predict
    pixels=stable.transpose(0,2,1,3).reshape(72,128).tobytes()
    delta=bytes(np.frombuffer(pal,dtype='u1')^np.frombuffer(previous[:512],dtype='u1'))+vectors.tobytes()+residual.transpose(0,2,1,3).reshape(72,128).tobytes()
    return lz10(delta),False,pal+pixels

def decode_motion(packed,previous,key):
    data=unpack(packed)
    if key:
        if len(data)!=9728:raise ValueError('Bad keyframe size')
        return data
    if len(data)!=9872 or previous is None:raise ValueError('Bad motion delta')
    old=np.frombuffer(previous[512:],dtype='u1').reshape(72,128);residual=np.frombuffer(data[656:],dtype='u1').reshape(72,128);result=np.empty_like(old)
    for i,code in enumerate(data[512:656]):
        if code>=81:raise ValueError('Invalid motion vector')
        x=i%16*8;y=i//16*8;dx=code%9-4;dy=code//9-4
        if x+dx<0 or y+dy<0 or x+dx+8>128 or y+dy+8>72:raise ValueError('Motion vector leaves frame')
        result[y:y+8,x:x+8]=old[y+dy:y+dy+8,x+dx:x+dx+8]^residual[y:y+8,x:x+8]
    palette=bytes(np.frombuffer(data[:512],dtype='u1')^np.frombuffer(previous[:512],dtype='u1'))
    return palette+result.tobytes()
