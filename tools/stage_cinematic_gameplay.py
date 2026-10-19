#!/usr/bin/env python3
"""Bake cinematic actors into the existing fixed-budget live renderer formats."""
import argparse,json
from pathlib import Path
from assets import Archive,resolve
from materials import txd,detailed_dff
from stage_animation import bake,ifp
from actor_texture import SharedVehicleAtlas,PedestrianAtlas
from car_mesh import convert_car

ROOT=Path(__file__).resolve().parents[1]
CAR_CLIPS=('car_sit','car_getin_lhs','car_getin_rhs','car_getout_lhs','car_getout_rhs')

def stage(game,out):
    out.mkdir(parents=True,exist_ok=True);archive=Archive(game)
    animations=ifp(resolve(game,'anim/ped.ifp').read_bytes());animations.update(ifp(archive.read('colt45.ifp')))
    report={'actors':[],'vehicles':[]}
    for i in range(1,4):
        suffix='' if i==1 else str(i);name='player'+suffix;source='csplay'+suffix
        for filename,budget,clips in [(name,520,None),('driver'+suffix,120,CAR_CLIPS)]:
            print('Baking',filename,'from',source,flush=True)
            report['actors'].append(bake(archive,animations,name,budget,out/(filename+'.vca'),clips,
                source_name=source,texture_size=(256,128),protect_face=True))
    pedestrians=PedestrianAtlas()
    specs=[('male01','male01','male01',80),('hfyst','hfyst','hfyst',80),
           ('igmerc2','csmerc','mercedes',120),('chef','chef','chef',80),
           ('igbuddy','csbuddy','igbuddy',80),('spandxa','spandxa','spandxa',80)]
    for name,source,filename,budget in specs:
        print('Baking textured actor',filename,'from',source,flush=True)
        textures=txd(archive.read(source+'.txd'));g=detailed_dff(archive.read(source+'.dff'))[0]
        names={g[4][m][1] for *_,m in g[1]}
        if len(names)!=1:raise ValueError((source,'requires more than one raster'))
        slot=pedestrians.add(textures[next(iter(names))])
        report['actors'].append(bake(archive,animations,name,budget,out/(filename+'.vca'),
            source_name=source,texture_atlas=pedestrians,texture_slot=slot,protect_face=True))
        if name=='male01':
            report['actors'].append(bake(archive,animations,name,80,out/'fleet-driver.vca',CAR_CLIPS,
                texture_atlas=pedestrians,texture_slot=slot,protect_face=True))
    pedestrians.save(out/'pedestrians.atx')
    atlas=SharedVehicleAtlas();names=('sentinel','taxi','admiral','spand')
    for name in names:convert_car(archive,name,out/f'fleet-{name}.vcm',640,256,True,atlas,True)
    atlas.freeze();print('Vehicle atlas',atlas.report,flush=True)
    for name in names:
        print('Baking shared-atlas car',name,flush=True)
        report['vehicles'].append(convert_car(archive,name,out/f'fleet-{name}.vcm',640,256,True,atlas))
    atlas.save(out/'fleet-shared.atx');report['vehicle_atlas']=atlas.report
    report['texture_vram']={'player':32768,'fleet_shared':32768,'pedestrians':16384,'world_limit':147456,
        'total':229376,'remaining':32768,'previous_total':229376}
    report['limits']={'player_faces':520,'driver_faces':120,'civilian_faces':80,'mercedes_faces':120,'close_vehicle_faces':640,
        'world_renderer':'unchanged','source_animations':'original ped.ifp; extra cinematic face bones stay in their neutral bind pose'}
    (out/'cinematic-gameplay-assets.json').write_text(json.dumps(report,indent=2)+'\n')
    print('Complete:',out,flush=True)

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('game',type=Path)
    p.add_argument('--output',type=Path,default=ROOT/'sdcard/revc/world');a=p.parse_args();stage(a.game,a.output)
