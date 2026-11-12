#!/usr/bin/env python3
"""Index all retail script slots and prepare explicit early route adaptations.
This extracts constants; it does not interpret control flow or claim SCM parity.
"""
import argparse,hashlib,json,math,struct
from pathlib import Path
from scm_cutscenes import commands,scenes
from stage_scripts import inspect
from stage_navigation import gxt_table
ROOT=Path(__file__).resolve().parents[1]
# Names for activities without a REGISTER_MISSION_PASSED title key.
FALLBACK={0:'World initialization',1:'In the Beginning...',2:'An Old Friend',36:'Malibu purchase',37:'Sunshine Autos purchase',38:'Film Studio purchase',39:'Cherry Popper purchase',40:'Kaufman Cabs purchase',41:'Print Works purchase',42:'Boatyard purchase',43:'Pole Position purchase',44:'Hyman Condo purchase',45:'Links View purchase',46:'El Swanko purchase',47:'Ocean Heights purchase',48:'1102 Washington purchase',49:'3321 Vice Point purchase',50:'Skumole Shack purchase',66:'Shooting range',75:'Taxi Driver',76:'Paramedic',77:'Firefighter',78:'Vigilante',79:'Hotring',80:'Bloodring',81:'Dirtring',82:'Street races',83:'Distribution',84:'Checkpoint activity 1',85:'Checkpoint activity 2',86:'Checkpoint activity 3',87:'Checkpoint activity 4',88:'Trial by Dirt',89:'Test Track',90:'PCJ Playground',91:'Cone Crazy',92:'Pizza Boy',93:'RC Raider Pickup',94:'RC Bandit Race',95:'RC Baron Race',96:'Checkpoint Charlie'}
# Existing, individually authored adaptations. IDs refer to original SCM slots.
ADAPTED={1:0,2:0,3:1,4:2,5:3,6:4,7:5,8:6,18:7}
FIGHTS={9:3,11:4,13:3,16:4,17:3,20:3,24:4,32:3,35:3,52:4,54:4,55:3,57:4,58:3,60:3,62:3,67:1,69:4,70:2,71:4}
VEHICLES={25:3,51:3,59:3,64:1,68:1,73:3,74:3}
ORDER=[9,10,11,12,13,14,15,17,16,19,20,31,32,33,21,22,23,24,25,26,27,28,29,30,34,35,53,54,55,56,57,58,59,60,61,62,63,64,65,67,68,69,70,71,72,73,74,51,52]
def parsed(data,index):
 rows=[];error=None
 try:
  for row in commands(data,index):rows.append(row)
 except (ValueError,KeyError,IndexError,struct.error) as e:error=str(e)
 return rows,error

def main(game):
 data=(game/'data/main.scm').read_bytes();info=inspect(data)
 if info['sha256']!='0fb3fd45b6a6dc21870cb48d9abe983510284b2b09b0c3d0eeac51d5dff338c7':raise ValueError('Unreviewed SCM version')
 gxt=(game/'TEXT/american.gxt').read_bytes();maintext=gxt_table(gxt,'MAIN');initial={}
 for _,n,v in parsed(data,0)[0]:
  if n in ('SET_VAR_FLOAT','SET_VAR_INT') and isinstance(v[1],(int,float)):initial[v[0]]=v[1]
 # Project early objectives onto original outdoor navigation segments. Boats,
 # aircraft and interiors need dedicated gameplay; keep these routes on land.
 paths=(ROOT/'sdcard/revc/world/paths.vcp').read_bytes();count=struct.unpack_from('<I',paths,4)[0]
 edges=list(dict.fromkeys(struct.iter_unpack('<I6f',paths[8+12*count:])))
 vehicle_edges=[e for e in edges if e[0]==1]
 def road(point,segments=edges):
  x,y,z=point;best=None;score=1e30
  for kind,ax,ay,az,bx,by,bz in segments:
   dx=bx-ax;dz=bz-az;length=dx*dx+dz*dz;t=max(0,min(1,((x-ax)*dx+(z-az)*dz)/length)) if length else 0
   q=(ax+t*dx,ay+t*(by-ay)+.1,az+t*dz);d=(x-q[0])**2+(z-q[2])**2
   if d<score:score=d;best=q
  return best,math.sqrt(score)
 records=[];movie_records=[];allrows=[]
 for index in range(info['missions']):
  rows,error=parsed(data,index);table=next((v[0]for _,n,v in rows if n=='LOAD_MISSION_TEXT'),None)
  texts=dict(maintext);texts.update(gxt_table(gxt,table)if table else {})
  key=next((v[0]for _,n,v in rows if n=='REGISTER_MISSION_PASSED' and isinstance(v[0],str)),None)
  name=maintext.get(key) if key else None
  if not name:
   names=[texts.get(v[0])for _,n,v in rows if n=='PRINT_BIG' and v and v[0] not in ('M_FAIL','M_PASS')]
   name=next((n for n in names if n and 'ASSET' not in n and 'ACQUIRED' not in n and 'PASSED' not in n),None)
  name=FALLBACK.get(index,name or table or f'Script {index:02d}')
  constants=dict(initial);points=[];fallback=[];reward=0;start=None
  def val(v):return constants.get(v)if isinstance(v,tuple)else v
  def coord(values):
   p=[val(v)for v in values]
   if len(p)!=3 or not all(isinstance(x,(int,float))and math.isfinite(x)for x in p):return None
   x,y,z=p
   if not -2000<x<2000 or not -2000<y<2000 or not -100<=z<350 or abs(x)+abs(y)<1:return None
   return (x-230,z,-y-1250)
  for address,n,v in rows:
   if n.startswith(('SET_VAR_','SET_LVAR_')) and len(v)==2 and isinstance(v[0],tuple):
    value=val(v[1]);constants.pop(v[0],None)
    if isinstance(value,(int,float)):constants[v[0]]=value
   if n=='ADD_SCORE' and isinstance(val(v[1]),int):reward=max(reward,val(v[1]))
   values=None
   if n=='SET_SHORTCUT_DROPOFF_POINT_FOR_MISSION':start=coord(v[:3])
   elif n=='ADD_BLIP_FOR_COORD':values=v[:3]
   elif n in ('LOCATE_PLAYER_ANY_MEANS_3D','LOCATE_PLAYER_ON_FOOT_3D','LOCATE_PLAYER_IN_CAR_3D','LOCATE_STOPPED_PLAYER_ANY_MEANS_3D','LOCATE_STOPPED_PLAYER_ON_FOOT_3D','LOCATE_STOPPED_PLAYER_IN_CAR_3D'):values=v[1:4]
   elif n=='CREATE_PICKUP':values=v[2:5]
   if values:
    p=coord(values)
    if p and all((p[0]-q['source'][0])**2+(p[2]-q['source'][2])**2>30**2 for q in points):points.append({'source':p,'address':address,'command':n})
   values=v[1:4] if n in ('CREATE_CAR','SET_PLAYER_COORDINATES') else v[2:5] if n=='CREATE_CHAR' else None
   if values:
    p=coord(values)
    if p and all((p[0]-q['source'][0])**2+(p[2]-q['source'][2])**2>30**2 for q in fallback):fallback.append({'source':p,'address':address,'command':n})
   # A runtime query/creation overwrites variables. Never reuse their initial
   # zero values as if they were authored coordinates.
   if n.startswith(('GET_','CREATE_')):
    for value in v:
     if isinstance(value,tuple):constants.pop(value,None)
  if not points:points=fallback
  # Limit one adaptation's resident route, not world rendering detail.
  if len(points)>8:points=[points[round(i*(len(points)-1)/7)]for i in range(8)]
  route=[]
  for p in points:
   safe,delta=road(p['source']);p.update(point=safe,projection_metres=delta)
   if all((safe[0]-q['point'][0])**2+(safe[2]-q['point'][2])**2>12**2 for q in route):route.append(p)
  movieids=[]
  try:specs=scenes(data,index)
  except (ValueError,KeyError,IndexError)as e:specs=[]
  for s in specs:
   movieids.append(len(movie_records));movie_records.append({'name':s['name'],'mission':index,'title':name,'available':(ROOT/'sdcard/revc/cutscenes'/f"{s['name']}.vcv").exists(),'has_origin':'origin'in s})
  kind='adapted' if index in ADAPTED else 'route' if route and index not in range(36,51) and index!=0 else 'catalog'
  record={'id':index,'title':name,'table':table,'kind':kind,'adapted':ADAPTED.get(index,-1),'route':route if kind=='route' else [],'start':road(start)[0]if start else route[0]['point']if route else [-93.543,9.44,-432.63],'reward':max(0,min(reward,100000)),'action':1 if index in FIGHTS else 2 if index in VEHICLES else 0,'targets':FIGHTS.get(index,VEHICLES.get(index,0)),'movies':movieids,'parse_stop':error}
  records.append(record);allrows.append({'id':index,'rows':rows,'error':error})
 from stage_activities import extend,aircraft,service_fleet
 courses=extend(data,info,records,allrows,road,lambda p:road(p,vehicle_edges))
 for movie in movie_records:movie['title']=records[movie['mission']]['title']
 aircraft(game,ROOT/'generated/sparrow_data.h')
 service_fleet(game,ROOT)
 # Generated content stays local; only this extractor belongs in source exports.
 def text(s):return json.dumps(s.encode('ascii','replace').decode())
 def point(p):return '{'+','.join(f'{v:.6f}f'for v in p)+'}'
 out=['// Generated from owned SCM/GXT. Early routes do not execute SCM logic.','#pragma once','namespace slice { namespace libraryData {']
 for i,course in enumerate(courses):
  out.append('static const Point Course%d[]={%s};'%(i,','.join(point(p) for p in course)))
 out.append('static const FlightCourse Courses[]={'+','.join('{Course%d,%d}'%(i,len(c)) for i,c in enumerate(courses))+'};')
 out.append('static const LibraryMovie Movies[] = {')
 for m in movie_records:out.append('{%s,%s,%d,%s},'%(text(m['name']),text(m['title']),m['mission'],'true'if m['available']else'false'))
 out.append('};');out.append('static const LibraryMission Missions[] = {')
 for r in records:
  route=r['route'];ms=r['movies'];out.append('{%s,%d,%d,%d,%s,%d,{%s},%d,{%s},%d,%d,%d},'%(text(r['title']),{'catalog':0,'adapted':1,'route':2,'activity':3}[r['kind']],r['adapted'],r['reward'],point(r['start']),len(route),','.join(point(q['point'])for q in route),len(ms),','.join(map(str,ms)),r['id'],r['action'],r['targets']))
 out.append('};');out.append('static const unsigned Order[] = {'+','.join(str(i)for i in ORDER if records[i]['kind']=='route')+'};');out.append('} }')
 (ROOT/'generated/mission_library_data.h').write_text('\n'.join(out)+'\n')
 report={'source_sha256':info['sha256'],'note':'All 97 SCM slots catalogued; early routes project source locations onto outdoor navigation. Service jobs use bounded dispatches. Property prices and flying courses come from SCM. Original branching and progression remain partial.','missions':records,'movies':movie_records,'story_order':[i for i in ORDER if records[i]['kind']=='route']}
 (ROOT/'reports/mission-library.json').write_text(json.dumps(report,indent=2)+'\n');(ROOT/'reports/all-missions-source-audit.json').write_text(json.dumps(allrows,indent=2)+'\n')
 from collections import Counter
 print(dict(Counter(r['kind']for r in records)),len(movie_records),'scene entries',sum(m['available']for m in movie_records),'embedded')
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('game',type=Path);main(p.parse_args().game)
