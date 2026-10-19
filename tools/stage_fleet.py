#!/usr/bin/env python3
"""Stage four textured, articulated cars, shared source LODs and seated actors."""
import argparse
import json
from pathlib import Path
from assets import Archive, resolve
from car_mesh import convert_car
from stage_animation import bake, ifp

ROOT = Path(__file__).resolve().parents[1]
NAMES = ('sentinel', 'taxi', 'admiral', 'spand')
CLIPS = ('car_sit', 'car_getin_lhs', 'car_getin_rhs', 'car_getout_lhs', 'car_getout_rhs')


def stage(game):
    archive = Archive(game)
    out = ROOT / 'sdcard/revc/world'
    out.mkdir(parents=True, exist_ok=True)
    vehicles = [convert_car(archive, name, out / f'fleet-{name}.vcm', 640, 128, True) for name in NAMES]
    animations = ifp(resolve(game, 'anim/ped.ifp').read_bytes())
    drivers = [bake(archive, animations, name, budget, out / filename, CLIPS)
               for name, budget, filename in (('player', 120, 'driver.vca'), ('male01', 80, 'fleet-driver.vca'))]
    lines = ['#pragma once', '#include "gameplay.h"', 'namespace slice { namespace fleetData {',
             'struct Spec {Point seat;float radius,wheelbase;};', 'static const Spec models[]={']
    for car in vehicles:
        values = [f'{float(v):.9f}f' for v in (*car['seat'], car['wheel_radius'], car['wheelbase'])]
        lines.append('{{' + ','.join(values[:3]) + '},' + ','.join(values[3:]) + '},')
    lines.extend(['};', '} }'])
    (ROOT / 'generated/fleet_data.h').write_text('\n'.join(lines) + '\n')
    report = vehicles + drivers
    (ROOT / 'reports/fleet-assets.json').write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(report), flush=True)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('game', type=Path)
    stage(parser.parse_args().game)
