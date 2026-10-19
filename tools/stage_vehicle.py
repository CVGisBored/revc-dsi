#!/usr/bin/env python3
"""Stage articulated source cars and the small seated/entry/exit Tommy mesh."""
import argparse,json
from pathlib import Path
from assets import Archive,resolve
from car_mesh import convert_car
from stage_animation import bake,ifp
ROOT=Path(__file__).resolve().parents[1]
def stage(game):
    archive=Archive(game);out=ROOT/'sdcard/revc/world'
    report={
        'sentinel':convert_car(archive,'sentinel',out/'vehicle.vcm',600),
        'taxi':convert_car(archive,'taxi',out/'traffic.vcm',420),
        'driver':bake(archive,ifp(resolve(game,'anim/ped.ifp').read_bytes()),'player',120,out/'driver.vca',
            ('car_sit','car_getin_lhs','car_getin_rhs','car_getout_lhs','car_getout_rhs'))}
    (ROOT/'reports/vehicle-animation.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps(report),flush=True)
if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('game',type=Path);stage(p.parse_args().game)
