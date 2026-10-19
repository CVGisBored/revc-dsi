#!/usr/bin/env python3
"""Compare spatial COL reads with an unindexed view of identical source faces."""
from pathlib import Path
import subprocess
import tempfile

workspace = Path(__file__).resolve().parents[1]
root = workspace / 'sdcard/revc'
with tempfile.TemporaryDirectory(prefix='revc-linear-') as directory:
    reference = Path(directory)
    world = reference / 'world'
    world.mkdir()
    for source in (root / 'world').iterdir():
        if not source.is_file():
            continue
        target = world / source.name
        if source.name == 'collision.vcc':
            target.write_bytes(b'VCC1' + source.read_bytes()[4:])
        else:
            target.symlink_to(source)
    subprocess.run([str(workspace / 'build/revc-host/revc_spatial_streaming_test'),
                    str(root), str(reference)], check=True)
