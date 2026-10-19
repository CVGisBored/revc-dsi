#!/usr/bin/env python3
"""Import individually reviewed Cortez/Avery mission objectives, not the SCM VM."""
import json,re,struct,textwrap
from pathlib import Path
from scm_cutscenes import commands
from stage_navigation import gxt_table
from stage_scripts import inspect
ROOT=Path(__file__).resolve().parents[1]
def main(game):
 data=(game/'data/main.scm').read_bytes();info=inspect(data)
 assert info['sha256']=='0fb3fd45b6a6dc21870cb48d9abe983510284b2b09b0c3d0eeac51d5dff338c7'
 # Derive the reviewed command prefixes from the owner's local SCM. A clean
 # source checkout must not need a report containing extracted game dialogue.
 lists={};stops={}
 for index in (7,8,18):
  result=[]
  try:
   for command in commands(data,index):result.append(command)
  except (ValueError,KeyError) as error:stops[index]=str(error)
  lists[index]=result
 def at(i,a,n):
  _,name,args=next(commands(data,i,a));assert name==n;return args
 def point(p):
  assert len(p)==3 and all(isinstance(v,(int,float))for v in p);x,y,z=p;return '{%.9ff,%.9ff,%.9ff}'%(x-230,z,-y-1250)
 def globals_at(i,addrs):return [at(i,a,'SET_VAR_FLOAT')[1]for a in addrs]
 rewards=[at(7,8951,'ADD_SCORE')[1],at(8,9942,'ADD_SCORE')[1],at(18,9850,'ADD_SCORE')[1]]
 assert rewards==[250,500,500]
 out=['// Reviewed retail source objectives; bounded handheld adaptations.','#pragma once','#include "gameplay.h"','namespace slice { namespace extraData {']
 pts={'Gonzalez':at(7,3079,'CREATE_CHAR')[2:5],'Penthouse':at(7,2501,'ADD_BLIP_FOR_COORD')[:3], 'Courier':at(8,1613,'CREATE_CHAR')[2:5], 'GolfClothes':globals_at(18,(456,466,476)), 'GolfTarget':globals_at(18,(746,756,766)), 'Avery':at(18,994,'SET_PLAYER_COORDINATES')[1:4], 'Cortez':at(7,2303,'SET_PLAYER_COORDINATES')[1:4], 'RoofEntry':at(7,3358,'SET_PLAYER_COORDINATES')[1:4], 'GolfEntrance':at(18,3982,'ADD_BLIP_FOR_COORD')[:3]}
 for n,p in pts.items():out.append('constexpr Point '+n+' = '+point(p)+';')
 guards=[globals_at(18,v)for v in ((546,556,566),(586,596,606),(626,636,646))]
 out.append('constexpr Point GolfGuards[] = {'+','.join(map(point,guards))+'};')
 gxt=(game/'TEXT/american.gxt').read_bytes();pages={}
 for k,(idx,table,prefix)in enumerate(((7,'GENERA1','GEN1'),(8,'GENERA2','COL2'),(18,'SERG1','TEX1'))):
  texts=gxt_table(gxt,table);ps=[]
  for _,n,args in lists[idx]:
   if n=='PRINT_NOW' and re.fullmatch(prefix+'_[A-Z]',args[0]):
    lines=textwrap.wrap(texts[args[0]],32);ps+=['\n'.join(lines[p:p+2])for p in range(0,len(lines),2)]
  assert ps;pages[table]=len(ps);out.append('const char* const Brief%d[] = {'%k+','.join(json.dumps(v,ensure_ascii=True)for v in ps)+'};')
 out+=['struct Brief {const char* const* pages;unsigned count;};','static const Brief Briefings[] = {'+','.join('{Brief%d,sizeof(Brief%d)/sizeof(Brief%d[0])}'%(i,i,i)for i in range(3))+'};','} }']
 (ROOT/'generated/expanded_missions_data.h').write_text('\n'.join(out)+'\n')
 (ROOT/'reports/expanded-missions-source.json').write_text(json.dumps({'sha256':info['sha256'],'missions':['Treacherous Swine','Mall Shootout','Four Iron'],'points':pts,'guards':guards,'briefing_pages':pages,'rewards':rewards,'reward_commands':[[7,8951],[8,9942],[18,9850]],'adaptation':'Original targets, locations, dialogue; bounded combat, wanted cooldown and item delivery. Target pursuit, ranged enemy AI and golf-cart escape are not the full original SCM.'},indent=2)+'\n')
if __name__=='__main__':
 import sys;main(Path(sys.argv[1]))
