#!/usr/bin/env python3
"""Validate and embed the untouched original Vice City main.scm mission bundle."""
import argparse
import hashlib
import json
import struct
from pathlib import Path

def inspect(data):
    def jump(pos):
        if data[pos:pos+3]!=b'\x02\x00\x01':raise ValueError('Invalid SCM header jump')
        return struct.unpack_from('<I',data,pos+3)[0]
    variables=jump(0);objects=jump(variables);start=jump(objects)
    main,largest,count,exclusive=struct.unpack_from('<IIHH',data,objects+8)
    offsets=struct.unpack_from('<'+str(count)+'I',data,objects+20)
    if not 8<=variables<objects<start<main<=offsets[0] or not 0<count<=128:raise ValueError('Invalid SCM sections')
    ends=offsets[1:]+(len(data),)
    if any(not main<=a<b<=len(data) or b-a>largest for a,b in zip(offsets,ends)):raise ValueError('Invalid mission offset/size')
    return {'sha256':hashlib.sha256(data).hexdigest(),'bytes':len(data),'globals_bytes':variables,
            'main_bytes':main,'largest_mission_bytes':largest,'missions':count,'exclusive':exclusive,
            'entry':start,'offsets':list(offsets),'status':'Original data staged; unsupported engine opcodes halt the diagnostic VM'}

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('game',type=Path);args=p.parse_args()
    root=Path(__file__).resolve().parents[1];data=(args.game/'data/main.scm').read_bytes();report=inspect(data)
    out=root/'sdcard/revc/scripts';out.mkdir(parents=True,exist_ok=True);(out/'main.scm').write_bytes(data)
    (root/'reports/scripts.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps({k:v for k,v in report.items() if k!='offsets'}))
