#!/usr/bin/env python3
"""Reject assets, binaries and likely credentials in a source-only export/index."""
import argparse
from pathlib import Path, PurePosixPath
import re
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
TOP_FILES = {'.gitignore', '.gitattributes', 'README.md', 'BUILDING.md',
             'THIRD_PARTY.md', 'Makefile', 'requirements.txt'}
SOURCE_DIRS = {'source', 'include', 'ports', 'cmake', 'tools', 'tests', 'licenses'}
EXTENSIONS = {'.c', '.cpp', '.h', '.hpp', '.py', '.cmake', '.patch', '.md', '.txt'}
LOCAL_DIRS = {'.git', '.venv', 'venv', '__pycache__', '.pytest_cache', '.mypy_cache',
              'external', 'generated', 'sdcard', 'reports', 'build', 'dist'}
SENSITIVE = [
    re.compile(rb'-----BEGIN (?:RSA |EC |OPENSSH |DSA )?PRIVATE KEY-----'),
    re.compile(rb'\bgh[pousr]_[A-Za-z0-9]{20,}\b'),
    re.compile(rb'\bgithub_pat_[A-Za-z0-9_]{30,}\b'),
    re.compile(rb'\b(?:AKIA|ASIA)[A-Z0-9]{16}\b'),
    re.compile(rb'https?://[^\s/@:]+:[^\s/@]+@'),
    re.compile(rb'/(?:home|Users)/[A-Za-z0-9_.-]+/'),
]


def path_problem(name):
    p = PurePosixPath(name)
    if p.is_absolute() or '..' in p.parts:
        return 'unsafe relative path'
    if name in TOP_FILES:
        return None
    if not p.parts or p.parts[0] not in SOURCE_DIRS:
        return 'outside source/documentation allowlist'
    if any(part.startswith('.') or part in LOCAL_DIRS for part in p.parts):
        return 'local/generated content inside source directory'
    if p.suffix not in EXTENSIONS and p.name != 'CMakeLists.txt':
        return 'not an allowed source/documentation file type'
    return None


def content_problem(data):
    if len(data) > 512 * 1024:
        return 'unusually large source file (review before allowing)'
    if b'\0' in data:
        return 'binary/NUL content'
    try:
        data.decode('utf-8')
    except UnicodeDecodeError:
        return 'not UTF-8 text'
    if any(pattern.search(data) for pattern in SENSITIVE):
        return 'possible credential or personal absolute path; inspect locally'
    return None


def git(*args):
    return subprocess.check_output(['git', '-C', str(ROOT), *args])


def audit(staged):
    errors = []
    count = total = 0
    if staged:
        if Path(git('rev-parse', '--show-toplevel').decode().strip()).resolve() != ROOT:
            raise ValueError('Initialize Git inside this export first; refusing to inspect a parent repository')
        entries = []
        for record in git('ls-files', '--stage', '-z').split(b'\0'):
            if not record:
                continue
            info, raw_name = record.split(b'\t', 1)
            mode, oid, stage = info.decode().split()
            name = raw_name.decode('utf-8')
            if mode not in ('100644', '100755') or stage != '0':
                errors.append((name, 'symlink, submodule or unresolved index entry'))
                continue
            entries.append((name, lambda oid=oid: git('cat-file', 'blob', oid)))
    else:
        entries = []
        def walk(folder):
            for p in sorted(folder.iterdir()):
                name = p.relative_to(ROOT).as_posix()
                if p.is_symlink():
                    errors.append((name, 'symlinks are excluded'))
                elif p.is_dir():
                    if p.name not in LOCAL_DIRS:
                        walk(p)
                else:
                    entries.append((name, p.read_bytes))
        walk(ROOT)
    for name, read in entries:
        problem = path_problem(name)
        if problem:
            errors.append((name, problem))
            continue
        data = read()
        problem = content_problem(data)
        if problem:
            errors.append((name, problem))
        count += 1
        total += len(data)
    if not count:
        errors.append(('.', 'no source files found'))
    for name, reason in errors:
        print(f'FAIL {name!r}: {reason}', file=sys.stderr)
    print(f'{count} text source/documentation files, {total:,} bytes; {len(errors)} issues')
    return 1 if errors else 0


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--staged', action='store_true', help='Check the complete Git index, not working files')
    args = parser.parse_args()
    try:
        sys.exit(audit(args.staged))
    except (OSError, ValueError, subprocess.CalledProcessError) as error:
        parser.exit(1, str(error) + '\n')
