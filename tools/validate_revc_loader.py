#!/usr/bin/env python3
"""Cross-check the upstream loader on staged and deliberately rotated real meshes."""
import argparse
import json
import math
from pathlib import Path
import struct
import subprocess
import tempfile
from assets import Archive, instances, dff, place, Instance
from prepare import face_color


def reference(path, obj, archive, center):
    result = []
    cx, cy = center
    for vertices, indices in dff(archive.read(obj.name+'.dff')):
        world = [place(obj, p) for p in vertices]
        for triangle in indices:
            points = [world[i] for i in triangle]
            if face_color(points, obj.model_id) is None:
                continue
            result.append(tuple(v for p in points for v in
                                (round((p[0]-cx)*64), round(p[2]*64), round(-(p[1]-cy)*64))))
    path.write_bytes(struct.pack('<I', len(result)) + b''.join(struct.pack('<9h', *p) for p in result))


def validate(game):
    workspace = Path(__file__).resolve().parents[1]
    binary = workspace / 'build/revc-host/revc_assets_test'
    subprocess.run([str(binary), str(workspace/'sdcard/revc'), str(workspace/'generated/sector-reference.bin')], check=True)
    report = json.loads((workspace/'reports/inspection.json').read_text())
    archive = Archive(game)
    sample = next(i for i in instances(game) if i.name == report['objects'][0]['name'])
    rotated = Instance(sample.model_id, sample.name, sample.position, (.5, 1.2, .7),
                       (0, 0, math.sqrt(.5), math.sqrt(.5)), sample.source)
    with tempfile.TemporaryDirectory(prefix='revc-transform-') as directory:
        root = Path(directory)
        (root/'models').mkdir()
        name = (sample.name+'.dff').encode('ascii')
        data = archive.read(sample.name+'.dff')
        (root/'models/sector.img').write_bytes(data)
        (root/'models/sector.dir').write_bytes(struct.pack('<II24s', 0, len(data)//2048, name))
        (root/'sector.vcs').write_bytes(struct.pack('<4sIff', b'VCS1', 1, *report['center']) +
                                       struct.pack('<24s10f', name, *rotated.position, *rotated.scale, *rotated.rotation))
        expected = root/'reference.bin'
        reference(expected, rotated, archive, report['center'])
        subprocess.run([str(binary), str(root), str(expected)], check=True)
    print('Staged-sector and rotated/scaled real-mesh validation passed')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('game', type=Path)
    validate(parser.parse_args().game.resolve())
