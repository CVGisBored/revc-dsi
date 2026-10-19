"""Read-only Vice City PC asset readers. No executable code is run or unpacked."""
from dataclasses import dataclass
from pathlib import Path
import math
import struct


class FormatError(ValueError):
    pass


def resolve(root, name):
    """Resolve Windows paths case-insensitively without leaving the game directory."""
    result = Path(root)
    for part in name.replace('\\', '/').split('/'):
        if not part or part == '.':
            continue
        if part == '..':
            raise FormatError('Parent path in manifest')
        matches = [p for p in result.iterdir() if p.name.casefold() == part.casefold()]
        if len(matches) != 1:
            raise FormatError(f'Missing or ambiguous path: {name}')
        result = matches[0]
        if not result.resolve().is_relative_to(Path(root).resolve()):
            raise FormatError('Path escapes game directory')
    return result


def lines(path):
    for number, line in enumerate(path.read_text(encoding='cp1252').splitlines(), 1):
        text = line.split('#', 1)[0].strip()
        if text:
            yield number, text


def manifest(root, kind):
    for _, text in lines(resolve(root, 'data/gta_vc.dat')):
        parts = text.split(None, 1)
        if len(parts) == 2 and parts[0].upper() == kind:
            yield resolve(root, parts[1])


@dataclass
class Instance:
    model_id: int
    name: str
    position: tuple
    scale: tuple
    rotation: tuple
    source: str


def instances(root):
    result = []
    for path in manifest(root, 'IPL'):
        section = ''
        for number, text in lines(path):
            if ',' not in text:
                section = text.lower()
                continue
            if section != 'inst':
                continue
            fields = [v.strip() for v in text.split(',')]
            if len(fields) != 13:
                raise FormatError(f'{path}:{number}: expected 13 VC instance fields')
            values = tuple(float(v) for v in fields[3:])
            if not all(math.isfinite(v) for v in values):
                raise FormatError(f'{path}:{number}: non-finite transform')
            result.append(Instance(int(fields[0]), fields[1].lower(), values[:3],
                                   values[3:6], values[6:], str(path.relative_to(root))))
    return result


def timed_models(root):
    result = {}
    for path in manifest(root, 'IDE'):
        section = ''
        for number, text in lines(path):
            if ',' not in text:
                section = text.lower()
            elif section == 'tobj':
                fields = [v.strip() for v in text.split(',')]
                start, end = int(fields[-2]), int(fields[-1])
                if not (0 <= start <= 24 and 0 <= end <= 24):
                    raise FormatError(f'{path}:{number}: invalid model hours')
                result[int(fields[0])] = (start, end)
    return result


def visible_at(hours, hour):
    start, end = hours
    return start <= hour < end if start <= end else hour >= start or hour < end


@dataclass
class Entry:
    name: str
    offset: int
    size: int


class Archive:
    """IMG v1: external DIR with 32-byte records, offsets in 2048-byte sectors."""
    def __init__(self, root):
        self.path = resolve(root, 'models/gta3.img')
        data = resolve(root, 'models/gta3.dir').read_bytes()
        if len(data) % 32:
            raise FormatError('Truncated IMG directory')
        length = self.path.stat().st_size
        self.entries = {}
        self.records = []
        self.duplicates = []
        for pos in range(0, len(data), 32):
            sector, count, raw = struct.unpack_from('<II24s', data, pos)
            name = raw.split(b'\0', 1)[0].decode('ascii').lower()
            entry = Entry(name, sector * 2048, count * 2048)
            if not name or '/' in name or '\\' in name or count == 0:
                raise FormatError('Invalid IMG entry name or size')
            if entry.offset + entry.size > length:
                raise FormatError(f'Invalid IMG bounds: {name}')
            self.records.append(entry)
            if name in self.entries:
                self.duplicates.append(name)
            else:
                self.entries[name] = entry

    def read(self, name):
        entry = self.entries[name.lower()]
        with self.path.open('rb') as file:
            file.seek(entry.offset)
            data = file.read(entry.size)
        if len(data) != entry.size:
            raise FormatError('Truncated IMG member')
        return data


def chunks(data, padded=False):
    pos = 0
    while pos < len(data):
        if padded and not any(data[pos:]):
            return
        if pos + 12 > len(data):
            raise FormatError('Truncated RenderWare chunk header')
        kind, length, version = struct.unpack_from('<III', data, pos)
        end = pos + 12 + length
        if end > len(data):
            raise FormatError('RenderWare chunk exceeds parent')
        yield kind, version, data[pos + 12:end]
        pos = end


def one(data, kind):
    found = [(v, d) for k, v, d in chunks(data) if k == kind]
    if len(found) != 1:
        raise FormatError(f'Expected one chunk {kind:#x}, found {len(found)}')
    return found[0]


def rw_version(v):
    return (((v >> 14) & 0x3ff00) + 0x30000) | ((v >> 16) & 0x3f) if v & 0xffff0000 else v << 8


def geometry(data):
    version, body = one(data, 1)
    if len(body) < 16:
        raise FormatError('Truncated geometry')
    flags, nt, nv, nm = struct.unpack_from('<IIII', body)
    if flags & 0x01000000:
        raise FormatError('Native geometry unsupported')
    if nv > 65535 or nt > 100000 or not 1 <= nm <= 64:
        raise FormatError('Unsupported geometry counts')
    tex = (flags >> 16) & 255
    if not tex:
        tex = 2 if flags & 0x80 else (1 if flags & 4 else 0)
    pos = 16 + (12 if rw_version(version) < 0x34000 else 0)
    pos += (nv * 4 if flags & 8 else 0) + tex * nv * 8
    if pos + nt * 8 > len(body):
        raise FormatError('Truncated triangles')
    triangles = []
    for i in range(nt):
        b, a, material, c = struct.unpack_from('<HHHH', body, pos + i * 8)
        if max(a, b, c) >= nv:
            raise FormatError('Triangle index out of range')
        triangles.append((a, b, c))
    pos += nt * 8
    vertices = None
    for morph in range(nm):
        if pos + 24 > len(body):
            raise FormatError('Truncated morph target')
        has_v, has_n = struct.unpack_from('<II', body, pos + 16)
        pos += 24
        if has_v not in (0, 1) or has_n not in (0, 1):
            raise FormatError('Invalid morph target flags')
        if pos + nv * 12 * (has_v + has_n) > len(body):
            raise FormatError('Truncated vertex data')
        if morph == 0 and has_v:
            vertices = list(struct.iter_unpack('<fff', body[pos:pos + nv * 12]))
        pos += nv * 12 * (has_v + has_n)
    if vertices is None or not all(math.isfinite(v) for p in vertices for v in p):
        raise FormatError('Missing or non-finite vertices')
    return vertices, triangles


IDENTITY = (1., 0., 0., 0., 1., 0., 0., 0., 1., 0., 0., 0.)


def transform(matrix, point):
    return tuple(sum(matrix[k * 3 + axis] * point[k] for k in range(3)) + matrix[9 + axis]
                 for axis in range(3))


def compose(parent, child):
    columns = [tuple(sum(parent[k * 3 + axis] * child[col * 3 + k] for k in range(3))
                     for axis in range(3)) for col in range(3)]
    return tuple(v for c in columns for v in c) + transform(parent, child[9:12])


def dff(data):
    """Decode static PC clumps only; ignore materials, skinning, and extensions."""
    roots = list(chunks(data, padded=True))
    if len(roots) != 1 or roots[0][0] != 0x10:
        raise FormatError('Expected DFF clump')
    clump = roots[0][2]
    _, frames_chunk = one(clump, 0xe)
    _, frames = one(frames_chunk, 1)
    if len(frames) < 4:
        raise FormatError('Missing frame count')
    count, = struct.unpack_from('<I', frames)
    if len(frames) != 4 + count * 56:
        raise FormatError('Invalid frame list')
    matrices = []
    for i in range(count):
        values = struct.unpack_from('<12fiI', frames, 4 + i * 56)
        parent = values[12]
        if not -1 <= parent < i:
            raise FormatError('Unsupported frame hierarchy')
        matrices.append(compose(matrices[parent], values[:12]) if parent >= 0 else values[:12])
    _, glist = one(clump, 0x1a)
    _, gcount_body = one(glist, 1)
    if len(gcount_body) != 4:
        raise FormatError('Invalid geometry list count')
    gcount, = struct.unpack('<I', gcount_body)
    geometries = [geometry(d) for k, _, d in chunks(glist) if k == 0xf]
    if len(geometries) != gcount:
        raise FormatError('Geometry count mismatch')
    result = []
    for kind, _, atomic in chunks(clump):
        if kind != 0x14:
            continue
        _, body = one(atomic, 1)
        if len(body) != 16:
            raise FormatError('Unsupported atomic structure')
        frame, geo, flags, _ = struct.unpack('<IIII', body)
        if frame >= len(matrices) or geo >= len(geometries):
            raise FormatError('Invalid atomic reference')
        if not flags & 4:
            continue
        vertices, triangles = geometries[geo]
        result.append(([transform(matrices[frame], p) for p in vertices], triangles))
    return result


def place(instance, point):
    x, y, z, w = instance.rotation
    length = math.sqrt(x*x + y*y + z*z + w*w)
    if length < 1e-8:
        raise FormatError('Zero placement quaternion')
    # VC IPL stores the opposite vector sign to an active point rotation.
    x, y, z, w = -x/length, -y/length, -z/length, w/length
    px, py, pz = (a*b for a, b in zip(point, instance.scale))
    tx, ty, tz = 2*(y*pz-z*py), 2*(z*px-x*pz), 2*(x*py-y*px)
    rotated = (px+w*tx+y*tz-z*ty, py+w*ty+z*tx-x*tz, pz+w*tz+x*ty-y*tx)
    return tuple(a+b for a, b in zip(rotated, instance.position))
