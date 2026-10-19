#!/usr/bin/env python3
"""Check packaged NDS headers and executable bounds, without running the ROM."""
import struct
import argparse
from pathlib import Path


def crc16(data):
    result = 0xffff
    for byte in data:
        result ^= byte
        for _ in range(8):
            result = (result >> 1) ^ (0xa001 if result & 1 else 0)
    return result


def check_assets(data, root):
    fnt_offset, fnt_size, fat_offset, fat_size = struct.unpack_from('<IIII', data, 0x40)
    if (fnt_size < 8 or fat_size % 8 or not fat_size or
            fnt_offset < 0x200 or fat_offset < 0x200 or
            fnt_offset + fnt_size > len(data) or fat_offset + fat_size > len(data)):
        raise ValueError('Invalid NitroFS table bounds')
    fnt = data[fnt_offset:fnt_offset + fnt_size]
    directories = struct.unpack_from('<H', fnt, 6)[0]
    if not directories or directories * 8 > len(fnt):
        raise ValueError('Invalid NitroFS directory table')
    found, visited = {}, set()

    def walk(directory, prefix):
        index = directory - 0xf000
        if index < 0 or index >= directories or directory in visited:
            raise ValueError('Invalid or repeated NitroFS directory')
        visited.add(directory)
        cursor, file_id, _ = struct.unpack_from('<IHH', fnt, index * 8)
        if cursor < directories * 8:
            raise ValueError('Invalid NitroFS name table offset')
        while True:
            if cursor >= len(fnt):
                raise ValueError('Truncated NitroFS name table')
            tag = fnt[cursor]
            cursor += 1
            if tag == 0:
                break
            length = tag & 0x7f
            if not length or cursor + length > len(fnt):
                raise ValueError('Invalid NitroFS name')
            name = fnt[cursor:cursor + length].decode('ascii')
            cursor += length
            if name in ('.', '..') or '/' in name or '\\' in name:
                raise ValueError('Unsafe NitroFS name')
            path = prefix + name
            if tag & 0x80:
                if cursor + 2 > len(fnt):
                    raise ValueError('Truncated NitroFS directory entry')
                child = struct.unpack_from('<H', fnt, cursor)[0]
                cursor += 2
                walk(child, path + '/')
            else:
                if (file_id + 1) * 8 > fat_size or path in found:
                    raise ValueError('Invalid NitroFS file entry')
                start, end = struct.unpack_from('<II', data, fat_offset + file_id * 8)
                if start < 0x200 or end < start or end > len(data):
                    raise ValueError('Invalid NitroFS file bounds')
                found[path] = data[start:end]
                file_id += 1

    walk(0xf000, '')
    expected = {p.relative_to(root).as_posix(): p.read_bytes()
                for p in root.rglob('*') if p.is_file()}
    if not expected or found.keys() != expected.keys():
        raise ValueError('NitroFS file list differs from staged assets')
    for name, content in expected.items():
        if found[name] != content:
            raise ValueError(f'Embedded asset differs: {name}')
    print(f'NitroFS: {len(found)} files, {sum(map(len, found.values()))} asset bytes; exact match')


def check(path, assets=None):
    data = path.read_bytes()
    if len(data) < 0x1000:
        raise ValueError('ROM too small')
    if crc16(data[:0x15e]) != struct.unpack_from('<H', data, 0x15e)[0]:
        raise ValueError('Invalid DS header CRC')
    for cpu, start in (('ARM9', 0x20), ('ARM7', 0x30)):
        offset, entry, address, size = struct.unpack_from('<IIII', data, start)
        if not size or offset < 0x200 or offset + size > len(data):
            raise ValueError(f'Invalid {cpu} file bounds')
        if not address <= (entry & ~1) < address + size:
            raise ValueError(f'Invalid {cpu} entry point')
        print(f'{cpu}: {size} bytes, entry {entry:#x}')
    if data[0x12] not in (0, 2, 3):
        raise ValueError('Invalid DS/DSi device type')
    print(f'Header CRC valid; device type {data[0x12]}; ROM {len(data)} bytes')
    if assets is not None:
        check_assets(data, assets)
    print('This structural check does not establish runtime or DSi hardware compatibility.')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('rom', type=Path)
    parser.add_argument('--assets', type=Path, help='Verify embedded NitroFS files against this directory')
    args = parser.parse_args()
    check(args.rom, args.assets)
