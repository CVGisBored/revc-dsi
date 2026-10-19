#!/usr/bin/env python3
"""Review/import the first four Ken missions from the owned retail SCM/GXT."""
import argparse,hashlib,json,re,struct,subprocess,textwrap
from pathlib import Path
from scm_cutscenes import commands
from stage_navigation import gxt_table
ROOT=Path(__file__).resolve().parents[1]
def main(game):
 data=(game/'data/main.scm').read_bytes();digest=hashlib.sha256(data).hexdigest()
 if digest!='0fb3fd45b6a6dc21870cb48d9abe983510284b2b09b0c3d0eeac51d5dff338c7':raise ValueError('Unreviewed SCM')
 lists={};stops={}
 for index in range(3,7):
  result=[]
  try:
   for c in commands(data,index):result.append(c)
  except (ValueError,KeyError) as error:stops[index]=str(error)
  lists[index]=result
 def at(index,address,name):
  result=next(commands(data,index,address))
  if result[1]!=name:raise ValueError((index,address,result))
  return result[2]
 def point(values):
  if len(values)!=3 or not all(isinstance(v,(int,float)) for v in values):raise ValueError(values)
  x,y,z=values;return (x-230,z,-y-1250)
 points={'Malibu':point(at(4,1510,'LOCATE_PLAYER_ON_FOOT_3D')[1:4]),'Chef':point([at(4,a,'SET_VAR_FLOAT')[1] for a in (203,213,223)]),'Ammo':point(at(4,11210,'ADD_BLIP_FOR_COORD')[:3]),'Lance':point(at(4,6791,'CREATE_CHAR')[2:5]),'RiotClothes':point([at(6,a,'SET_VAR_FLOAT')[1] for a in (91,101,111)]),'Depot':point(at(6,2437,'LOCATE_PLAYER_ANY_MEANS_3D')[1:4])}
 arrays={'Thugs':[point(at(4,a,'CREATE_CHAR')[2:5]) for a in (7017,7096,7175)],'JurorCars':[point(at(5,a,'CREATE_CAR')[1:4]) for a in (2631,2745)],'Jurors':[point(at(5,a,'CREATE_CHAR')[2:5]) for a in (2531,7424)],'Workers':[point(at(6,a,'CREATE_CHAR')[2:5]) for a in (2564,2619,2674,2729)],'Vans':[point(at(6,a,'CREATE_CAR')[1:4]) for a in (3920,3953,3986)]}
 out=['// Generated from reviewed commands in the supplied retail main.scm.','#pragma once','#include "gameplay.h"','namespace slice { namespace kenData {']
 f=lambda p:'{'+','.join('%.9ff'%v for v in p)+'}'
 for name,p in points.items():out.append('constexpr Point '+name+' = '+f(p)+';')
 for name,values in arrays.items():out.append('constexpr Point '+name+'[] = {'+','.join(map(f,values))+'};')
 gxt=(game/'TEXT/american.gxt').read_bytes();briefings={}
 for index in range(3,7):
  table=gxt_table(gxt,'LAWYER'+str(index-2));pages=[]
  for a,n,v in lists[index]:
   if n=='PRINT_NOW' and re.fullmatch('LAW'+str(index-2)+'_[A-Z]',v[0]):
    lines=textwrap.wrap(table[v[0]],32);pages+=['\n'.join(lines[k:k+2]) for k in range(0,len(lines),2)]
  if not pages:raise ValueError('Missing mission briefing')
  briefings[index-2]=pages;out.append('const char* const Brief%d[] = {'%(index-2)+','.join(json.dumps(v,ensure_ascii=True) for v in pages)+'};')
 out+=['struct Brief {const char* const* pages;unsigned count;};','static const Brief Briefings[] = {'+','.join('{Brief%d,sizeof(Brief%d)/sizeof(Brief%d[0])}'%(i,i,i) for i in range(1,5))+'};','} }']
 (ROOT/'generated/ken_missions_data.h').write_text('\n'.join(out)+'\n')
 report={'scm_sha256':digest,'missions':['The Party','Back Alley Brawl','Jury Fury','Riot'],'rewards':[100,200,400,1000],'points':points,'arrays':arrays,'briefing_pages':{k:len(v) for k,v in briefings.items()},'parser_prefix_stops':stops,'note':'Only the individually reviewed commands above are imported; the full VM remains unsupported. New introductions are original GXT text briefings, not converted cutscenes.'}
 (ROOT/'reports/ken-missions-source.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report))
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('game',type=Path);main(p.parse_args().game)
