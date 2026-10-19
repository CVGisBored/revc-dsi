#!/usr/bin/env python3
"""Prepare bounded original mission actors and original vehicle distant meshes."""
import argparse,struct,json
from pathlib import Path
from assets import Archive,chunks,one
from materials import detailed_dff,txd
from car_mesh import frame_data,material_color,palette
from stage_animation import bake,ifp
ROOT=Path(__file__).resolve().parents[1]
def main(game):
 archive=Archive(game);out=ROOT/'sdcard/revc/world';reports=[]
 for name,file in [('sentinel','vehicle-lod.vcm'),('taxi','traffic-lod.vcm'),('admiral','admiral.vcm'),('spand','spand.vcm')]:
  data=archive.read(name+'.dff');clump,frames=frame_data(data);exclude=tuple(label for label,m in frames if '_vlo' not in label)
  textures={}
  for key in ('generic',name):
   try:textures.update(txd(archive.read(key+'.txd')))
   except KeyError:pass
  paint=palette(game,name);records=[];points=[]
  for vertices,faces,uv,_,materials in detailed_dff(data,exclude):
   for a,b,c,m in faces:
    p=[(vertices[i][0],vertices[i][2],-vertices[i][1]) for i in (a,b,c)];points+=p;records.append((material_color(materials[m],uv,(a,b,c),textures,paint),p))
  if not records or len(records)>160:raise ValueError((name,len(records)))
  floor=min(p[1] for p in points)
  blob=[struct.pack('<H9h',color,*[round((v-(floor if k==1 else 0))*64) for p in tri for k,v in enumerate(p)]) for color,tri in records]
  (out/file).write_bytes(struct.pack('<4sI',b'VCM1',len(blob))+b''.join(blob));reports.append({'model':name,'faces':len(blob),'file':file})
 animations=ifp((game/'anim/ped.ifp').read_bytes());animations.update(ifp(archive.read('colt45.ifp')))
 for name in ('chef','igbuddy','spandxa'):
  reports.append(bake(archive,animations,name,80,out/(name+'.vca')))
 (ROOT/'reports/mission-assets.json').write_text(json.dumps(reports,indent=2)+'\n');print(json.dumps(reports))
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('game',type=Path);main(p.parse_args().game)
