#!/usr/bin/env python3
"""Import the SUNNY midday palette used by the fixed noon world."""
import argparse
import json
from pathlib import Path
from assets import resolve, FormatError

def stage(game,workspace):
    rows=[line.split('//')[0].split() for line in resolve(game,'data/timecyc.dat').read_text().splitlines()]
    rows=[row for row in rows if row]
    noon=rows[12]
    if len(noon)!=52:raise FormatError('Unexpected timecycle columns')
    ambient=bytes(int(v) for v in noon[:3]);sky=bytes(int(v) for v in noon[18:21])
    water=bytes(int(v) for v in noon[-4:])
    (workspace/'sdcard/revc/world/lighting.vcl').write_bytes(b'VLI1'+ambient+sky+water)
    (workspace/'reports/lighting.json').write_text(json.dumps({'hour':12,'weather':'SUNNY',
        'ambient':list(ambient),'sky_bottom':list(sky),'water':list(water),
        'limitations':['Fixed midday palette; dynamic weather and directional lighting are not integrated']},indent=2)+'\n')

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('game',type=Path)
    stage(p.parse_args().game,Path(__file__).resolve().parents[1])
