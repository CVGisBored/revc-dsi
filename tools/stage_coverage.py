#!/usr/bin/env python3
"""Build complete original LOD objects, dissolving only exact coplanar interiors."""
import argparse,math,struct,json
from collections import defaultdict
from pathlib import Path
from assets import Archive,instances,place,timed_models,visible_at
from materials import detailed_dff,model_textures
from compact_assets import lz10,unpack
ROOT=Path(__file__).resolve().parents[1]
INDEX=struct.Struct('<3h6hHII')
RECORD=struct.Struct('<HH9h6h')
def cross(a,b):return (a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0])
def sub(a,b):return tuple(x-y for x,y in zip(a,b))
def plane(points):
 n=cross(sub(points[1],points[0]),sub(points[2],points[0]));g=math.gcd(*n)
 if not g:return None
 n=tuple(v//g for v in n)
 if next(v for v in n if v)<0:n=tuple(-v for v in n)
 return (*n,sum(a*b for a,b in zip(n,points[0])))
def triangulate(loop,normal):
 axis=max(range(3),key=lambda k:abs(normal[k]));axes=[k for k in range(3) if k!=axis]
 def xy(p):return tuple(p[k] for k in axes)
 def turn(a,b,c):return (b[0]-a[0])*(c[1]-a[1])-(b[1]-a[1])*(c[0]-a[0])
 loop=list(loop)
 while len(loop)>3:
  remove=next((i for i in range(len(loop)) if cross(sub(loop[i],loop[i-1]),sub(loop[(i+1)%len(loop)],loop[i]))==(0,0,0)),None)
  if remove is None:break
  loop.pop(remove)
 area=sum(xy(loop[i])[0]*xy(loop[(i+1)%len(loop)])[1]-xy(loop[(i+1)%len(loop)])[0]*xy(loop[i])[1] for i in range(len(loop)))
 if not area:return None
 sign=1 if area>0 else -1;out=[]
 while len(loop)>3:
  found=False
  for i,b in enumerate(loop):
   a,c=loop[i-1],loop[(i+1)%len(loop)];aa,bb,cc=map(xy,(a,b,c))
   if turn(aa,bb,cc)*sign<=0:continue
   if any(all(t*sign>=0 for t in (turn(aa,bb,xy(p)),turn(bb,cc,xy(p)),turn(cc,aa,xy(p)))) for p in loop if p not in (a,b,c)):continue
   out.append((a,b,c));loop.pop(i);found=True;break
  if not found:return None
 out.append(tuple(loop));return out

def dissolve(faces):
 """faces = (positions, UVs, RGB15, texture); return no arbitrary face omissions."""
 groups=defaultdict(list);output=[]
 for face in faces:
  key=plane(face[0])
  if key is not None:groups[(key,face[3])].append(face)
 for (p,texture),group in groups.items():
  edges=defaultdict(list)
  for i,(vertices,*_) in enumerate(group):
   for j in range(3):edges[tuple(sorted((vertices[j],vertices[(j+1)%3])))].append(i)
  neighbours=[set() for _ in group]
  for entries in edges.values():
   if len(entries)==2:a,b=entries;neighbours[a].add(b);neighbours[b].add(a)
  seen=set()
  for seed in range(len(group)):
   if seed in seen:continue
   component={seed};todo=[seed];seen.add(seed)
   while todo:
    for i in neighbours[todo.pop()]:
     if i not in seen:seen.add(i);component.add(i);todo.append(i)
   original=[group[i] for i in sorted(component)]
   if len(original)<3:output.extend(original);continue
   uv={};valid=True
   for vertices,coords,_,_ in original:
    for vertex,coord in zip(vertices,coords):
     if vertex in uv and any(abs(a-b)>1e-4 for a,b in zip(uv[vertex],coord)):valid=False
     uv[vertex]=coord
   boundary=defaultdict(list)
   for edge,entries in edges.items():
    if not component.intersection(entries):continue
    if len(entries)>2:valid=False
    if len(entries)==1:
     a,b=edge;boundary[a].append(b);boundary[b].append(a)
   if not valid or not boundary or any(len(v)!=2 for v in boundary.values()):output.extend(original);continue
   loop=[next(iter(boundary))];previous=None;current=loop[0]
   for _ in range(len(boundary)):
    following=next(v for v in boundary[current] if v!=previous)
    if following==loop[0]:break
    loop.append(following);previous,current=current,following
   if len(loop)!=len(boundary):output.extend(original);continue # Preserve holes and disconnected loops.
   triangles=triangulate(loop,p[:3])
   if triangles is None or len(triangles)>=len(original):output.extend(original);continue
   # Check the dissolve preserved exactly the covered area in its projection.
   axis=max(range(3),key=lambda k:abs(p[k]))
   before=sum(abs(cross(sub(v[1],v[0]),sub(v[2],v[0]))[axis]) for v,*_ in original)
   after=sum(abs(cross(sub(v[1],v[0]),sub(v[2],v[0]))[axis]) for v in triangles)
   if before!=after:output.extend(original);continue
   # Only dissolve when texture coordinates form a single affine map on this plane.
   first=original[0];a,b,c=first[0];u,v=sub(b,a),sub(c,a);axes=[k for k in range(3) if k!=axis];j,k=axes;det=u[j]*v[k]-u[k]*v[j]
   for vertex,coord in uv.items():
    delta=sub(vertex,a);s=(delta[j]*v[k]-delta[k]*v[j])/det;t=(u[j]*delta[k]-u[k]*delta[j])/det
    if any(abs(first[1][0][q]+s*(first[1][1][q]-first[1][0][q])+t*(first[1][2][q]-first[1][0][q])-coord[q])>1e-3 for q in range(2)):valid=False;break
   if not valid:output.extend(original);continue
   color=sum((round(sum((f[2]>>(5*q))&31 for f in original)/len(original))<<(5*q)) for q in range(3))
   output.extend((tri,tuple(uv[v] for v in tri),color,texture) for tri in triangles)
 return output

def build(game):
 archive=Archive(game);mapping=model_textures(game);timed=timed_models(game)
 ids={(r['dictionary'],r['name']):r['id'] for r in json.loads((ROOT/'reports/materials.json').read_text())['textures']}
 objects=[o for o in instances(game) if 'lod' in o.name and 'islandlod' not in o.name and (o.model_id not in timed or visible_at(timed[o.model_id],12))]
 blocks=[];index=[];before=after=0;largest=rawmax=0;geometry={}
 for number,obj in enumerate(objects):
  if obj.name not in geometry:geometry[obj.name]=detailed_dff(archive.read(obj.name+'.dff'))
  faces=[]
  for vertices,triangles,uv,prelight,materials in geometry[obj.name]:
   world=[place(obj,p) for p in vertices];world=[tuple(round(v*64) for v in (p[0]-230,p[2],-(p[1]+1250))) for p in world]
   for a,b,c,m in triangles:
    rgba,texture=materials[m];texture=ids.get((mapping.get(obj.model_id,'generic'),texture),ids.get(('generic',texture),0))
    color=sum(min(31,round(rgba[q]*sum(prelight[v][q] for v in (a,b,c))/3/255/255*31))<<(5*q) for q in range(3))
    faces.append((tuple(world[v] for v in (a,b,c)),tuple(uv[v] for v in (a,b,c)),color,texture))
  before+=len(faces);faces=dissolve(faces)
  if not faces:continue
  low=[min(v[k] for f in faces for v in f[0]) for k in range(3)];high=[max(v[k] for f in faces for v in f[0]) for k in range(3)]
  anchor=[(low[k]+high[k])//8192 for k in range(3)];base=[v*4096 for v in anchor]
  bounds=[v-base[k] for values in (low,high) for k,v in enumerate(values)]
  if any(not -32768<=v<=32767 for v in bounds):raise ValueError('Object exceeds local coordinate range: '+obj.name)
  payload=bytearray()
  for vertices,uv,color,texture in faces:
   phase=[math.floor(sum(v[k] for v in uv)/3) for k in range(2)];tex=[round((v[k]-phase[k])*1024) for v in uv for k in range(2)]
   if any(not -32768<=v<=32767 for v in tex):texture=0;tex=[0]*6
   xyz=[v[k]-base[k] for v in vertices for k in range(3)]
   payload.extend(RECORD.pack(color,texture,*xyz,*tex))
  blob=lz10(bytes(payload))
  assert unpack(blob)==payload
  blocks.append(blob);index.append((*anchor,*bounds,len(faces)));largest=max(largest,len(blob));rawmax=max(rawmax,len(payload));after+=len(faces)
  if number%200==0:print('Complete LODs',number,len(objects),before,after,flush=True)
 offset=16+len(index)*INDEX.size;entries=[]
 for entry,blob in zip(index,blocks):entries.append(INDEX.pack(*entry,offset,len(blob)));offset+=len(blob)
 out=ROOT/'sdcard/revc/world/distant.vcd';temporary=out.with_suffix('.new')
 temporary.write_bytes(struct.pack('<4sIII',b'VFO1',len(index),largest,rawmax)+b''.join(entries)+b''.join(blocks));temporary.replace(out)
 report={'format':'VFO1','objects':len(index),'original_faces':before,'faces':after,'bytes':offset,'index_stride':INDEX.size,'max_packed_object':largest,'max_raw_object':rawmax,'maximum_object_faces':max(e[-1] for e in index),'source':'Complete original individual LOD objects; no distance-based or tile-based triangle omissions','simplification':'Exact coplanar interior dissolve, boundary/area/UV checked; holes retained; prelight averaged across dissolved patches'}
 (ROOT/'reports/coverage.json').write_text(json.dumps(report,indent=2)+'\n');(ROOT/'reports/distant.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report),flush=True)
if __name__=='__main__':
 parser=argparse.ArgumentParser();parser.add_argument('game',type=Path);build(parser.parse_args().game)
