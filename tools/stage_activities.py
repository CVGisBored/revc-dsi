"""Stage outdoor property pickups, service dispatches and retail Sparrow courses."""
from scm_cutscenes import commands

PROPERTY_KEYS = ('PRNT_NO','CAR_NO','PORN_NO','ICE_NO','TAXI_NO','BANK_NO','BOAT_NO','STRP_NO',
                 'NBMN_L','LNKV_L','HYCO_L','OCHE_L','WASH_L','VCPT_L','SKUM_L')
ASSET_PRICES = (2540,2500,2520,2436,2480,2556,2416,2456)
NAMES = ('Print Works','Sunshine Autos','Film Studio','Cherry Popper','Kaufman Cabs','Malibu Club',
         'Boatyard','Pole Position','El Swanko Casa','Links View','Hyman Condo','Ocean Heights',
         '1102 Washington Street','3321 Vice Point','Skumole Shack')

def position(x,y,z):
    return (x-230,z,-y-1250)

def extend(data,info,records,allrows,road,road_vehicle):
    main=list(commands(data,None,info['entry']))
    setup=allrows[0]['rows']
    values={v[0]:v[1] for _,n,v in setup+main if n in ('SET_VAR_FLOAT','SET_VAR_INT') and isinstance(v[1],(int,float))}
    pickups={v[-2]:v for _,n,v in main if n in ('CREATE_FORSALE_PROPERTY_PICKUP','CREATE_LOCKED_PROPERTY_PICKUP')}
    def update(i,title,type_,raw_points,price=0,reward=0):
        route=[]
        for raw in raw_points:
            point,delta=(road if type_ in (3,7) else road_vehicle)(raw)
            route.append(dict(source=raw,point=point,projection_metres=delta))
        records[i].update(title=title,kind='activity',action=type_,targets=price,reward=reward,route=route,start=route[0]['point'])
    for k,key in enumerate(PROPERTY_KEYS):
        v=pickups[key]
        xyz=position(*(values[c] for c in v[:3]))
        price=values[('global',ASSET_PRICES[k])] if k<8 else values[v[3]]
        update(36+k,NAMES[k]+' purchase',3,[xyz],price=price)
    fleet=[v for _,n,v in setup+main if n=='CREATE_CAR_GENERATOR']
    def spawn(model):
        v=next(v for v in fleet if v[4]==model)
        return position(*(values.get(c,c) for c in v[:3]))
    # The original services pick targets dynamically. Shape short dispatches
    # around original station/factory locations using driveable street segments.
    for i,kind,origin,offsets,pay in (
        (77,4,spawn(137),[(75,40),(-35,120),(-110,35)],1200),
        (78,5,spawn(156),[(-65,45),(-95,-55),(20,-120)],1500),
        (83,6,records[39]['route'][0]['source'],[(75,30),(120,-50),(35,-100),(-60,-80),(-80,25)],750)):
        x,y,z=origin
        update(i,records[i]['title'],kind,[(x+dx,y,z+dz) for dx,dz in offsets],reward=pay)
        records[i]['start']=road_vehicle(origin)[0]
    pads=[position(*(values[c] for c in v[:3])) for v in fleet if v[4]==199]
    assert len(pads)==4
    courses=[]
    for k,area in enumerate(('Downtown','Ocean Beach','Vice Point','Little Haiti')):
        i=84+k; coords=[(v[0][1],v[1]) for _,n,v in allrows[i]['rows']
                         if n=='SET_VAR_FLOAT' and 30028<=v[0][1]<30500]
        assert [a for a,_ in coords]==list(range(30028,30028+4*len(coords),4)) and len(coords)%3==0
        gates=[position(*(v for _,v in coords[j:j+3])) for j in range(0,len(coords),3)]
        update(i,area+' Chopper Checkpoint',7,[pads[k]],reward=100)
        records[i]['flight_gates']=gates
        courses.append(gates)
    return courses

def aircraft(game,path):
    import numpy as np
    from assets import Archive,one,chunks,transform
    from car_mesh import frame_data,palette,material_color
    from materials import detailed_geometry,txd
    from simplify_mesh import simplify
    import struct
    archive=Archive(game);clump,frames=frame_data(archive.read('sparrow.dff'))
    geometries=[g for k,_,g in chunks(one(clump,0x1a)[1]) if k==15]
    textures=txd(archive.read('sparrow.txd'));paint=palette(game,'sparrow')
    parts={}
    for k,_,atomic in chunks(clump):
        if k!=20:continue
        frame,geo,_,_=struct.unpack('<4I',one(atomic,1)[1]);name,matrix=frames[frame]
        if name not in ('chassis_vlo','toprotor','rearrotor'):continue
        v,f,uv,_,m=detailed_geometry(geometries[geo]);indices=np.asarray([t[:3] for t in f])
        v=np.asarray([transform(matrix,p) for p in v]);ids=range(len(f))
        if name=='chassis_vlo':v,indices,_,ids,_=simplify(v,indices,240,preserve_features=True)
        parts[name]=[(material_color(m[f[i][3]],uv,f[i][:3],textures,paint),[(x,z,-y) for x,y,z in v[tri]]) for tri,i in zip(indices,ids)]
    base=min(y for _,pts in parts['chassis_vlo'] for _,y,_ in pts)
    lines=['#pragma once','#include "sector.h"','namespace sparrow {']
    for name,records in parts.items():
        lines.append('static const RevcTriangle '+name+'[]={')
        for color,pts in records:
            verts=','.join('{%d,%d,%d}'%(round(x*64),round((y-base)*64),round(z*64)) for x,y,z in pts)
            lines.append('{%d,{%s}},'%(color,verts))
        lines.append('};')
    path.write_text('\n'.join(lines+['}'])+'\n')

def service_fleet(game,root):
    from assets import Archive
    from car_mesh import convert_car
    from actor_texture import SharedVehicleAtlas
    archive=Archive(game);atlas=SharedVehicleAtlas(width=128)
    out=root/'sdcard/revc/world'
    names=('firetruk','police','mrwhoop')
    for name in names:convert_car(archive,name,out/f'fleet-{name}.vcm',640,256,True,atlas,True)
    atlas.freeze()
    specs=['#pragma once','#include "fleet_data.h"','namespace slice { namespace fleetData {','static const Spec serviceModels[]={']
    for name in names:
        car=convert_car(archive,name,out/f'fleet-{name}.vcm',640,256,True,atlas)
        specs.append('{{%sf,%sf,%sf},%sf,%sf},'%tuple(float(v) for v in (*car['seat'],car['wheel_radius'],car['wheelbase'])))
    atlas.save(out/'fleet-service.atx')
    (root/'generated/service_fleet_data.h').write_text('\n'.join(specs+['};','} }'])+'\n')
