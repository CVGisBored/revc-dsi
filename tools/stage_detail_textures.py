#!/usr/bin/env python3
"""128-pixel source rasters for a bounded close-world texture cache."""
import argparse,json,struct,hashlib
from pathlib import Path
import numpy as np
from PIL import Image
from assets import Archive
from materials import TextureRegistry
from compact_assets import lz10,unpack
ROOT=Path(__file__).resolve().parents[1]
def stage(game,limit=512):
 root=ROOT/'sdcard/revc/world';rows=json.loads((ROOT/'reports/materials.json').read_text())['textures'];count=len(rows)
 dtype=np.dtype([('color','<u2'),('texture','<u2'),('flags','<u2'),('coords','<i2',(9,)),('uv','<f4',(6,))]);assert dtype.itemsize==48
 records=np.memmap(root/'quality-source.bin',dtype=dtype,mode='r');freq=np.bincount(records['texture'][(records['flags']&3)==0],minlength=count+1)
 registry=TextureRegistry(game,Archive(game));offsets=[0]*count;unique={};parts=[];size=12+count*4;report=[]
 for row in sorted(rows,key=lambda r:int(freq[r['id']]),reverse=True):
  if not freq[row['id']] or max(row['size'])<128:continue
  image=registry.dictionary(row['dictionary']).get(row['name'])
  if image is None:
   image=next((registry.dictionary(n)[row['name']]for n in ('generic','particle') if row['name']in registry.dictionary(n)),None)
  if image is None:continue
  im=image.convert('RGBA').resize((128,128),Image.Resampling.LANCZOS);rgba=np.asarray(im);alpha=rgba[:,:,3]<128;transparent=bool(alpha.any());rgb=im.convert('RGB')
  # Flat rasters gain no detail at 128 pixels. Keep the normal page for those.
  if np.asarray(rgb).std()<3:continue
  quant=rgb.quantize(colors=255 if transparent else 256);indices=np.asarray(quant,dtype='u1');pal=np.asarray(quant.getpalette(),dtype='u2').reshape(-1,3)
  colors=np.zeros((256,3),dtype='u2')
  if transparent:indices=indices+1;indices[alpha]=0;colors[1:1+len(pal)]=pal
  else:colors[:len(pal)]=pal
  palette=((colors[:,0]>>3)|((colors[:,1]>>3)<<5)|((colors[:,2]>>3)<<10)).astype('<u2')
  raw=struct.pack('<I',int(transparent))+palette.tobytes()+indices.tobytes();key=hashlib.sha256(raw).digest()
  if key not in unique:
   if len(parts)>=limit:continue
   packed=lz10(raw);assert unpack(packed)==raw;unique[key]=size;part=struct.pack('<I',len(packed))+packed;parts.append(part);size+=len(part)
  offsets[row['id']-1]=unique[key];report.append({'id':row['id'],'dictionary':row['dictionary'],'texture':row['name'],'source_size':row['size'],'uses':int(freq[row['id']])})
  if len(report)%100==0:print('Detail rasters',len(report),'unique',len(parts),flush=True)
 dest=root/'detail.vct';dest.write_bytes(struct.pack('<4sII',b'VHD1',count,len(parts))+struct.pack('<%dI'%count,*offsets)+b''.join(parts))
 out={'file':str(dest),'bytes':dest.stat().st_size,'resolution':128,'unique':len(parts),'mapped_ids':len(report),'textures':report,'source':'Original TXD rasters, not enlarged 64-pixel cache images','runtime':'Near-world promotion only after camera and streaming settle; normal 64-pixel textures remain available during movement'}
 (ROOT/'reports/detail-textures.json').write_text(json.dumps(out,indent=2)+'\n');print('Detail texture pack:',dest.stat().st_size,'bytes',len(parts),'unique',len(report),'mapped IDs',flush=True)
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('game',type=Path);p.add_argument('--limit',type=int,default=512);a=p.parse_args();stage(a.game,a.limit)
