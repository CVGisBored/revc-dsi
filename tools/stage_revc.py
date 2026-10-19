#!/usr/bin/env python3
"""Stage the inspected static-sector assets for the reVC DSi ROM bootstrap."""
import argparse
import json
import struct
from pathlib import Path
from assets import Archive, instances, dff, place, FormatError
from prepare import face_color


def stage(game):
    workspace = Path(__file__).resolve().parents[1]
    report = json.loads((workspace / 'reports/inspection.json').read_text())
    game = game.resolve()
    if report['game_path'] != str(game):
        raise FormatError('Inspection report belongs to another game directory; run prepare.py first')
    archive = Archive(game)
    by_placement = {(i.name, i.position): i for i in instances(game)}
    objects = [by_placement[(o['name'], tuple(o['position']))] for o in report['objects']]
    if not 0 < len(objects) <= 64:
        raise FormatError('Staging supports 1..64 objects')
    output = workspace / 'sdcard/revc'
    (output / 'models').mkdir(parents=True, exist_ok=True)
    records, unique = [], {}
    with (output / 'models/sector.img').open('wb') as img:
        for obj in objects:
            name = obj.name + '.dff'
            encoded = name.encode('ascii')
            if len(encoded) >= 24:
                raise FormatError('DFF name exceeds directory record')
            if name in unique:
                continue
            data = archive.read(name)
            dff(data)  # Validate static geometry before passing it to the original loader.
            offset = img.tell() // 2048
            sectors = len(data) // 2048
            if not 0 < sectors <= 512:
                raise FormatError('DFF exceeds bootstrap resource limit')
            records.append(struct.pack('<II24s', offset, sectors, encoded))
            unique[name] = sectors
            img.write(data)
    (output / 'models/sector.dir').write_bytes(b''.join(records))
    # Independent reference positions for host validation of the upstream reader.
    reference = []
    cx, cy = report['center']
    for obj in objects:
        for vertices, indices in dff(archive.read(obj.name+'.dff')):
            world = [place(obj, p) for p in vertices]
            for triangle in indices:
                points = [world[i] for i in triangle]
                if face_color(points, obj.model_id) is None:
                    continue
                reference.append(tuple(v for p in points for v in
                                       (round((p[0]-cx)*64), round(p[2]*64), round(-(p[1]-cy)*64))))
    with (workspace / 'generated/sector-reference.bin').open('wb') as file:
        file.write(struct.pack('<I', len(reference)))
        for values in reference:
            file.write(struct.pack('<9h', *values))
    with (output / 'sector.vcs').open('wb') as file:
        file.write(struct.pack('<4sIff', b'VCS1', len(objects), *report['center']))
        for obj in objects:
            file.write(struct.pack('<24s10f', (obj.name+'.dff').encode('ascii'),
                                   *obj.position, *obj.scale, *obj.rotation))
    (output / 'README.txt').write_text(
        'Static sector assets from your local GTA Vice City installation.\n'
        'The build embeds this directory into the ROM as a read-only NitroFS filesystem.\n'
        'This is not the full game asset installation.\n')
    print(f'Staged {len(objects)} placements / {len(unique)} DFFs at {output}')
    print(f'IMG size: {(output / "models/sector.img").stat().st_size} bytes')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('game', type=Path)
    stage(parser.parse_args().game)
