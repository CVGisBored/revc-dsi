#!/usr/bin/env python3
"""Convert a bounded static sector to DS fixed-point triangles; inspect the PE."""
import argparse
import csv
import hashlib
import json
import math
from pathlib import Path
from collections import Counter
from assets import Archive, FormatError, dff, instances, place, resolve, timed_models, visible_at


def executable_report(path):
    import pefile
    pe = pefile.PE(str(path))
    return {
        'file': path.name, 'sha256': hashlib.sha256(path.read_bytes()).hexdigest(),
        'machine': hex(pe.FILE_HEADER.Machine), 'image_base': hex(pe.OPTIONAL_HEADER.ImageBase),
        'entry_rva': hex(pe.OPTIONAL_HEADER.AddressOfEntryPoint),
        'image_size': pe.OPTIONAL_HEADER.SizeOfImage,
        'sections': [{'name': s.Name.rstrip(b'\0').decode('ascii'),
                      'rva': hex(s.VirtualAddress), 'virtual_bytes': s.Misc_VirtualSize,
                      'file_bytes': s.SizeOfRawData} for s in pe.sections],
        'imports': {d.dll.decode('ascii'): [i.name.decode('ascii') if i.name else f'ordinal:{i.ordinal}'
                                           for i in d.imports]
                    for d in getattr(pe, 'DIRECTORY_ENTRY_IMPORT', [])},
        'parser_warnings': pe.get_warnings(),
        'source_recovery': 'None. Static PE inventory only; no decompilation or unpacking.',
    }


def face_color(points, model_id):
    u = [points[1][i] - points[0][i] for i in range(3)]
    v = [points[2][i] - points[0][i] for i in range(3)]
    n = (u[1]*v[2]-u[2]*v[1], u[2]*v[0]-u[0]*v[2], u[0]*v[1]-u[1]*v[0])
    length = math.sqrt(sum(a*a for a in n))
    if length < 1e-5:
        return None
    light = .5 + .5 * abs(sum(a*b for a, b in zip(n, (.3, .4, .866))) / length)
    base = (23 + model_id % 5, 20 + model_id % 7, 16 + model_id % 9)
    r, g, b = (min(31, round(c * light)) for c in base)
    return r | g << 5 | b << 10


def prepare(args):
    root = args.game.resolve()
    target = Path(__file__).resolve().parent.parent
    generated, reports = target / 'generated', target / 'reports'
    generated.mkdir(exist_ok=True)
    reports.mkdir(exist_ok=True)
    archive = Archive(root)
    objects = instances(root)
    timed = timed_models(root)
    cx, cy = args.center
    priority = {name.lower(): index for index, name in enumerate(args.priority)}
    selected = sorted((i for i in objects if not i.name.startswith('lod')
                       and (i.model_id not in timed or visible_at(timed[i.model_id], args.hour))
                       and math.hypot(i.position[0]-cx, i.position[1]-cy) <= args.radius),
                      key=lambda i: (priority.get(i.name, len(priority)),
                                     math.hypot(i.position[0]-cx, i.position[1]-cy), i.name))
    triangles, exported, skipped, cache = [], [], [], {}
    for instance in selected:
        try:
            name = instance.name + '.dff'
            if name not in cache:
                cache[name] = dff(archive.read(name))
            batch = []
            for vertices, indices in cache[name]:
                world = [place(instance, p) for p in vertices]
                for triangle in indices:
                    points = [world[i] for i in triangle]
                    color = face_color(points, instance.model_id)
                    if color is None:
                        continue
                    # GTA x,y,z -> DS x,z,-y; 64 world units per render unit.
                    coords = [(round((p[0]-cx)*64), round(p[2]*64), round(-(p[1]-cy)*64))
                              for p in points]
                    if any(not -32768 <= c <= 32767 for p in coords for c in p):
                        raise FormatError('Mesh exceeds signed v16 sector range')
                    batch.append((color, coords))
            if not batch:
                raise FormatError('No renderable static triangles')
            if len(triangles) + len(batch) > args.max_triangles:
                raise FormatError('Whole mesh exceeds remaining triangle budget')
            triangles.extend(batch)
            exported.append({'name': instance.name, 'position': instance.position, 'triangles': len(batch)})
        except (FormatError, KeyError) as error:
            skipped.append({'name': instance.name, 'reason': str(error)})
    if not triangles:
        raise FormatError('No geometry exported')
    with (generated / 'scene.hpp').open('w') as file:
        file.write('// Generated from local game assets. Do not redistribute.\n#pragma once\n')
        file.write('#include <cstdint>\nnamespace scene {\n')
        file.write('struct Triangle { uint16_t color; int16_t vertex[3][3]; };\n')
        file.write(f'constexpr int count = {len(triangles)};\n')
        file.write(f'constexpr int objects = {len(exported)};\n')
        file.write(f'constexpr float centerX = {cx:.6f}f, centerY = {cy:.6f}f;\n')
        file.write('static const Triangle triangles[count] = {\n')
        for color, coords in triangles:
            vertices = ','.join('{' + ','.join(str(v) for v in p) + '}' for p in coords)
            file.write('{' + str(color) + ',{' + vertices + '}},\n')
        file.write('};\n}\n')
    with (reports / 'archive.csv').open('w', newline='') as file:
        writer = csv.writer(file)
        writer.writerow(['name', 'offset_bytes', 'size_bytes'])
        writer.writerows((e.name, e.offset, e.size) for e in archive.records)
    with (reports / 'placements.csv').open('w', newline='') as file:
        writer = csv.writer(file)
        writer.writerow(['model_id', 'name', 'x', 'y', 'z', 'sx', 'sy', 'sz', 'qx', 'qy', 'qz', 'qw', 'source'])
        writer.writerows((i.model_id, i.name, *i.position, *i.scale, *i.rotation, i.source) for i in objects)
    report = {
        'game_path': str(root), 'img_bytes': archive.path.stat().st_size,
        'archive_members': len(archive.entries),
        'directory_records': len(archive.records), 'duplicate_names_first_record_used': archive.duplicates,
        'member_types': dict(Counter(Path(e.name).suffix for e in archive.entries.values())),
        'ipl_instances': len(objects), 'center': [cx, cy], 'radius': args.radius,
        'sample_hour': args.hour, 'priority_models': args.priority,
        'triangle_budget': args.max_triangles, 'triangles': len(triangles),
        'geometry_bytes': len(triangles) * 20, 'objects': exported, 'skipped': skipped,
        'limitations': ['Static, first morph target only', 'No materials or textures',
                        'No skinning or extension processing', 'Budget drops whole objects; scene is incomplete',
                        'IPL quaternion sign convention requires comparison against original game'],
        'executable': executable_report(resolve(root, 'gta-vc.exe')),
    }
    (reports / 'inspection.json').write_text(json.dumps(report, indent=2) + '\n')
    print(f'Archive: {len(archive.entries)} members; map: {len(objects)} placements')
    print(f'Sector: {len(exported)} objects, {len(triangles)} triangles, {len(triangles)*20} geometry bytes')
    print(f'Skipped: {len(skipped)} objects; see reports/inspection.json')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('game', type=Path)
    parser.add_argument('--center', type=float, nargs=2, default=(230., -1250.))
    parser.add_argument('--radius', type=float, default=220.)
    parser.add_argument('--max-triangles', type=int, default=1800)
    parser.add_argument('--hour', type=int, choices=range(24), default=12)
    parser.add_argument('--priority', nargs='*',
                        default=['oceanrda05_dy', 'deco_polgrnda16', 'deco_polgrnda17'],
                        help='Prefer these complete meshes before nearest other objects')
    args = parser.parse_args()
    if not 1 <= args.max_triangles <= 1800 or not 0 < args.radius <= 400:
        parser.error('Use 1..1800 triangles and a radius of 0..400 world units')
    prepare(args)
