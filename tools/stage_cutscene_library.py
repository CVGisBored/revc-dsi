#!/usr/bin/env python3
"""Convert every discoverable authored cutscene; record unsupported sequences explicitly."""
import argparse,concurrent.futures,json,os,shutil,subprocess,sys
from pathlib import Path
from scm_cutscenes import scenes
ROOT=Path(__file__).resolve().parents[1]
def main(game,workers):
 data=(game/'data/main.scm').read_bytes();out=ROOT/'sdcard/revc/cutscenes';work=ROOT/'build/mission-cinematics';work.mkdir(parents=True,exist_ok=True)
 entries=[];jobs=[]
 for i in range(97):
  try:specs=scenes(data,i)
  except (ValueError,KeyError,IndexError) as e:entries.append({'mission':i,'status':'unsupported','reason':str(e)});continue
  for spec in specs:
   name=spec['name'];entry={'mission':i,'name':name}
   if (out/(name+'.vcv')).exists():entry['status']='embedded'
   elif 'origin' not in spec and name not in ('gspot','fist'):entry.update(status='unsupported',reason='Sequence has no authored 3D origin/object staging; requires a separate sequence adapter.')
   else:entry['status']='pending';jobs.append(entry)
   entries.append(entry)
 def convert(entry):
  name=entry['name'];dest=work/name;dest.mkdir(exist_ok=True);log=dest/'conversion.log'
  env=dict(os.environ,LP_NUM_THREADS='2',OPENBLAS_NUM_THREADS='1',OMP_NUM_THREADS='1')
  scripted=name in ('gspot','fist')
  args=['stage_script_movies.py',str(game),name] if scripted else ['stage_cutscenes.py',str(game),'--missions',str(entry['mission']),'--only',name,'--fps','30','--compact','--interframe','--motion']
  with log.open('w') as stream:
   run=subprocess.run([sys.executable,str(ROOT/'tools'/args[0]),*args[1:],'--output',str(dest)],stdout=stream,stderr=subprocess.STDOUT,env=env)
  if run.returncode==0 and (dest/(name+'.vcv')).exists():
   temporary=out/(name+'.vcv.tmp');shutil.copyfile(dest/(name+'.vcv'),temporary);temporary.replace(out/(name+'.vcv'));entry.update(status='embedded',conversion=json.loads((ROOT/'reports'/f'script-movie-{name}.json').read_text()) if scripted else json.loads((dest/'conversion.json').read_text())[0])
  else:entry.update(status='unsupported',reason=log.read_text()[-1800:])
  return entry
 def report():
  (ROOT/'reports/cutscene-library.json').write_text(json.dumps(entries,indent=2)+'\n')
 report();print(f'{len(jobs)} new scenes to convert',flush=True)
 with concurrent.futures.ThreadPoolExecutor(max_workers=workers) as pool:
  for future in concurrent.futures.as_completed([pool.submit(convert,j)for j in jobs]):
   entry=future.result();print(entry['name'],entry['status'],flush=True);report()
 print('Embedded:',sum(e['status']=='embedded'for e in entries),'Unsupported:',sum(e['status']=='unsupported'for e in entries),flush=True)
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('game',type=Path);p.add_argument('--workers',type=int,default=4);a=p.parse_args();main(a.game,max(1,min(8,a.workers)))
