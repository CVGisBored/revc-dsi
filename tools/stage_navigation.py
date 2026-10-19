#!/usr/bin/env python3
"""Import Vice City's radar tiles and the opening mission's authored objectives.
This is a checked mission adaptation, not an SCM compiler or cutscene engine.
"""
import argparse, hashlib, json, re, struct, textwrap
from pathlib import Path
import numpy as np
from PIL import Image
from assets import Archive, resolve
from materials import txd
from stage_scripts import inspect

ROOT=Path(__file__).resolve().parents[1]

def gxt_table(data, name):
    if data[:4]!=b'TABL':raise ValueError('Expected VC GXT tables')
    size,=struct.unpack_from('<I',data,4)
    tables={key.split(b'\0')[0].decode():off for key,off in struct.iter_unpack('<8sI',data[8:8+size])}
    p=tables[name]
    if data[p:p+4]!=b'TKEY':p+=8
    if data[p:p+4]!=b'TKEY':raise ValueError('Missing GXT keys')
    size,=struct.unpack_from('<I',data,p+4);keys=data[p+8:p+8+size];p+=8+size
    if data[p:p+4]!=b'TDAT':raise ValueError('Missing GXT text')
    size,=struct.unpack_from('<I',data,p+4);text=data[p+8:p+8+size];result={}
    for off,key in struct.iter_unpack('<I8s',keys):
        end=off
        while end+2<=len(text) and text[end:end+2]!=b'\0\0':end+=2
        result[key.split(b'\0')[0].decode()]=re.sub(r'~[^~]*~','',text[off:end].decode('utf-16le')).strip()
    return result

def stage(game):
    scm=resolve(game,'data/main.scm').read_bytes();info=inspect(scm)
    # These offsets identify the retail Steam script inspected in this port.
    # Fail on another version instead of silently importing different coordinates.
    if info['sha256']!='0fb3fd45b6a6dc21870cb48d9abe983510284b2b09b0c3d0eeac51d5dff338c7':
        raise ValueError('Opening mission extraction needs review for this SCM version')
    initial=scm[:info['main_bytes']];party=scm[info['offsets'][3]:info['offsets'][4]]
    def point(body,addresses):
        values=[];sources=[]
        for addr in addresses:
            pattern=struct.pack('<HBH',5,2,addr)+b'\x06'
            positions=[m.start() for m in re.finditer(re.escape(pattern),body)]
            if len(positions)!=1:raise ValueError(f'Ambiguous mission coordinate {addr}')
            sources.append(positions[0]);values.append(struct.unpack_from('<f',body,positions[0]+6)[0])
        return values,sources
    points={};provenance={}
    for name,body,addresses in [('Hotel',initial,(1812,1816,1820)),('Office',initial,(1824,1828,1832)),
                               ('Clothes',party,(396,400,404)),('Yacht',party,(8452,8456,8460)),('Club',party,(8440,8444,8448))]:
        points[name],provenance[name]=point(body,addresses)
    intro=scm[info['offsets'][1]:info['offsets'][2]]
    if intro[14139:14145]!=bytes.fromhex('a100020c0006'):raise ValueError('Changed intro spawn command')
    points['Spawn']=list(struct.unpack_from('<f',intro,14145))+list(struct.unpack_from('<f',intro,14150))+list(struct.unpack_from('<f',intro,14155))
    if party[10096:10102]!=bytes.fromhex('9a00040504')+b'o':raise ValueError('Changed Mercedes creation command')
    points['Passenger']=[struct.unpack_from('<f',party,p)[0] for p in (10103,10108,10113)]
    gxt=resolve(game,'TEXT/american.gxt').read_bytes();main=gxt_table(gxt,'MAIN');hotel=gxt_table(gxt,'HOTEL');law=gxt_table(gxt,'LAWYER1')
    phone=[]
    for p in range(info['offsets'][2],info['offsets'][3]-10):
        if scm[p:p+2]==b'\xbc\x00':
            key=scm[p+2:p+10].split(b'\0')[0].decode('ascii',errors='ignore')
            if key.startswith('INTB_') and key in hotel:
                lines=textwrap.wrap(hotel[key],32,break_long_words=True)
                phone.extend('\n'.join(lines[i:i+2]) for i in range(0,len(lines),2))
    if not phone:raise ValueError('Missing original phone dialogue')
    def literal(s):return json.dumps(s,ensure_ascii=True)
    header=['// Generated from the owned retail SCM/GXT by tools/stage_navigation.py.','#pragma once','#include "gameplay.h"','namespace slice { namespace missionData {']
    for name,(x,y,z) in points.items():header.append(f'constexpr Point {name} = {{{x-230:.9f}f,{z:.9f}f,{-y-1250:.9f}f}};')
    header.append('const char* const Phone[] = {'+','.join(map(literal,phone))+'};')
    header.append('constexpr unsigned PhonePages = sizeof(Phone)/sizeof(Phone[0]);')
    for name,key in [('ClothesText','LAW1_1'),('YachtText','LAW1_2'),('ClubText','LAW1_3')]:
        text=main.get(key,law.get(key));header.append(f'const char* const {name} = {literal(text)};')
    header+=['} }'];(ROOT/'generated').mkdir(exist_ok=True);(ROOT/'generated/campaign_data.h').write_text('\n'.join(header)+'\n')
    archive=Archive(game);atlas=Image.new('RGB',(256,256))
    for i in range(64):
        textures=txd(archive.read(f'radar{i:02}.txd'))
        image=textures[f'radar{i:02}'].convert('RGB').resize((32,32),Image.Resampling.LANCZOS)
        atlas.paste(image,((i%8)*32,(i//8)*32))
    indexed=atlas.quantize(colors=240,method=Image.Quantize.MEDIANCUT)
    colors=np.asarray(indexed.getpalette(),dtype=np.uint16).reshape(-1,3)[:240]
    palette=(colors[:,0]>>3)|((colors[:,1]>>3)<<5)|((colors[:,2]>>3)<<10)
    pixels=np.asarray(indexed,dtype=np.uint8)+16
    out=ROOT/'sdcard/revc/world/citymap.vcm';out.write_bytes(b'VRM1'+palette.astype('<u2').tobytes()+pixels.tobytes())
    atlas.save(ROOT/'reports/citymap.png')
    report={'scm_sha256':info['sha256'],'adaptation':'Opening hotel arrival, original An Old Friend dialogue, The Party objectives; cutscenes/audio/full SCM services not ported',
            'points_gta_xyz':points,'coordinate_command_offsets':provenance,'phone_pages':len(phone),'reward':100,
            'radar':'64 original radar00..63 TXDs; north up, original world bounds -2000..2000', 'map_bytes':out.stat().st_size}
    (ROOT/'reports/navigation.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report))

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('game',type=Path);stage(p.parse_args().game)
