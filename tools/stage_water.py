#!/usr/bin/env python3
"""Copy the original fine water mask and levels; reuse its imported texture."""
import argparse
import json
from pathlib import Path
import struct
from assets import resolve, FormatError

def stage(game, workspace):
    data=resolve(game,'data/waterpro.dat').read_bytes()
    if len(data)!=21444:raise FormatError('Unexpected original water data size')
    count=struct.unpack_from('<I',data)[0]
    if not 0<count<=48:raise FormatError('Invalid original water level count')
    mask=data[5060:]
    if any(value!=-128 and not 0<=value<count for value in struct.unpack('<16384b',mask)):
        raise FormatError('Invalid original water cell')
    textures=json.loads((workspace/'reports/materials.json').read_text())['textures']
    texture=next(x['id'] for x in textures if x['name']=='waterclear256')
    target=workspace/'sdcard/revc/world/water.vcw'
    target.write_bytes(struct.pack('<4sII',b'VWA1',count,texture)+data[4:196]+mask)
    result={'original_file':'data/WATERPRO.DAT','levels':count,'texture':texture,
            'water_cells':sum(v!=128 for v in mask),'cell_metres':32,
            'limitations':['Flat original water mask; waves, swimming and boats are not implemented']}
    (workspace/'reports/water.json').write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps(result))

if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('game',type=Path)
    stage(parser.parse_args().game,Path(__file__).resolve().parents[1])
