#!/usr/bin/env python3
"""Extract the original player's forward root-motion curves, in game metres/seconds."""
import argparse,json
from pathlib import Path
from stage_animation import ifp
NAMES=('idle_stance','walk_player','run_player','colt45_fire','colt45_reload','sprint_civi','walk_start','run_stop','run_stopr','jump_launch','jump_glide','jump_land')
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('game',type=Path);a=p.parse_args()
 clips=ifp((a.game/'anim/ped.ifp').read_bytes());root=Path(__file__).resolve().parents[1]
 out=['// Generated from the supplied ped.ifp. Times are seconds; forward travel is GTA Y.','#pragma once','namespace slice { namespace playerData {','struct Key {float time,forward;};','struct Track {const Key* keys;unsigned count;float duration;};']
 report=[]
 for i,name in enumerate(NAMES):
  if name not in clips:out.append(f'static constexpr Key keys{i}[] = {{{{0.f,0.f}},{{1.f,0.f}}}};');report.append({'name':name,'duration':1.,'keys':2,'speed':0.});continue
  nodes=clips[name];keys=nodes.get(0,nodes.get('root'));duration=max(float(k[-1,-1]) for k in nodes.values())
  if keys is None or keys.shape[1]<=5:values=[(0.,0.),(duration,0.)]
  else:values=[(float(k[-1]),float(k[5])) for k in keys]
  literal=lambda f:format(f,'.9f')+'f'
  out.append(f'static constexpr Key keys{i}[] = {{'+','.join('{'+literal(t)+','+literal(v)+'}' for t,v in values)+'};')
  report.append({'name':name,'duration':duration,'keys':len(values),'speed':(values[-1][1]-values[0][1])/duration})
 out.append('static constexpr Track tracks[] = {')
 for i,row in enumerate(report):out.append('{keys%d,%d,%.9ff},'%(i,row['keys'],row['duration']))
 out+=['};','// CPlayerPed / CPed / CPhysical use 50 simulation units per second.','static constexpr float TimeScale=50.f,MoveAcceleration=.07f,HeadingDegrees=15.f;','static constexpr float Gravity=.008f,Mass=70.f,JumpImpulse=8.5f;','static constexpr float InitialStamina=150.f,StaminaFloor=-150.f,SprintBlendRate=2.f;','} }']
 (root/'generated/player_motion_data.h').write_text('\n'.join(out)+'\n')
 (root/'reports/player-motion-source.json').write_text(json.dumps({'clips':report,'sources':['src/peds/PlayerPed.cpp: SetRealMoveAnim, PlayerControlZelda, UseSprintEnergy, RestoreSprintEnergy','src/peds/Ped.cpp: CalculateNewVelocity, FinishLaunchCB','src/animation/FrameUpdate.cpp: root translation extraction','src/animation/RpAnimBlend.cpp: synchronized movement periods','src/core/Timer.h: GetTimeStepInSeconds','src/entities/Physical.h: GRAVITY'],'units':'Original world metres, seconds; forward Y only for player movement associations'},indent=2)+'\n')
 print(json.dumps(report))
if __name__=='__main__':main()
