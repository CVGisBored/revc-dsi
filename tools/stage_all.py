#!/usr/bin/env python3
"""Regenerate local game-derived build inputs in dependency order."""
import argparse
import hashlib
import os
from pathlib import Path
import shlex
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
SCM_SHA256 = '0fb3fd45b6a6dc21870cb48d9abe983510284b2b09b0c3d0eeac51d5dff338c7'


def steps(game):
    ordinary = [
        ('prepare', True), ('stage_revc', True), ('stage_world', True),
        ('stage_fast', False), ('stage_quality', True), ('stage_water', True),
        ('stage_lighting', True), ('stage_collision', True),
        ('stage_terrain', False), ('stage_backdrops', True),
        ('compact_assets', False), ('stage_gameplay', True),
        ('stage_animation', True), ('stage_player_motion', True),
        ('stage_population', True), ('stage_mission_assets', True),
        ('stage_scripts', True), ('stage_missions', True),
        ('stage_navigation', True), ('stage_expanded_missions', True),
        ('stage_vehicle', True), ('stage_fleet', True),
        ('stage_distant', True), ('stage_texture_averages', False),
        ('stage_detail_textures', True), ('stage_frontend', True),
    ]
    result = [(name, [sys.executable, 'tools/' + name + '.py'] + ([str(game)] if needs_game else []))
              for name, needs_game in ordinary]
    result += [
        ('compact_logos', [sys.executable, 'tools/compact_cinematics.py', '--only', 'logo', 'gtatitles']),
        ('stage_cutscenes', [sys.executable, 'tools/stage_cutscenes.py', str(game),
            '--missions', '1', '2', '3', '4', '5', '6', '7', '8', '18',
            '--fps', '30', '--compact', '--motion']),
        ('stage_departure', [sys.executable, 'tools/stage_departure.py', str(game), '--fps', '30', '--motion']),
        ('stage_cinematic_gameplay', [sys.executable, 'tools/stage_cinematic_gameplay.py', str(game)]),
    ]
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('game', type=Path)
    parser.add_argument('--plan', action='store_true', help='Print commands without reading game data or writing outputs')
    parser.add_argument('--from-step', help='Resume at a named step after its prerequisites have completed')
    parser.add_argument('--through-step', help='Stop after a named step')
    args = parser.parse_args()
    plan = steps(args.game.resolve())
    names = [name for name, _ in plan]
    for name in (args.from_step, args.through_step):
        if name and name not in names:
            parser.error('Unknown step: ' + name)
    start = names.index(args.from_step) if args.from_step else 0
    end = names.index(args.through_step) + 1 if args.through_step else len(plan)
    if start >= end:
        parser.error('The selected step range is empty')
    if args.plan:
        for name, command in plan[start:end]:
            print(name + ': ' + shlex.join(command))
        return
    scm = args.game / 'data/main.scm'
    if not scm.is_file() or hashlib.sha256(scm.read_bytes()).hexdigest() != SCM_SHA256:
        parser.error('A compatible PC main.scm is required; see BUILDING.md for the reviewed SHA-256')
    if not (ROOT / 'external/reVC/src/control/ScriptCommands.h').is_file():
        parser.error('Obtain and patch the pinned dependencies first; see BUILDING.md')
    for folder in ('generated', 'reports', 'sdcard/revc/world', 'sdcard/revc/cutscenes'):
        (ROOT / folder).mkdir(parents=True, exist_ok=True)
    env = os.environ.copy()
    env.setdefault('OPENBLAS_NUM_THREADS', '1')
    for name, command in plan[start:end]:
        print('\nStaging ' + name, flush=True)
        subprocess.run(command, cwd=ROOT, env=env, check=True)
    print('Selected conversion steps finished. Run make revc after all steps complete.')


if __name__ == '__main__':
    main()
