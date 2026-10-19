#!/usr/bin/env python3
"""Import original vehicle, guard and weapon meshes within the handheld budget."""
import argparse
import json
from pathlib import Path
import struct
from assets import Archive,dff

def convert(archive,name,output,budget,grid,kind):
    if kind=='car':
        from car_mesh import convert_car
        return convert_car(archive,name,output,budget)
    groups=dff(archive.read(name+'.dff'))
    all_vertices=[p for v,_ in groups for p in v]
    vertical=1 if kind=='guard' else 2
    base=min(p[vertical] for p in all_vertices)
    faces={}
    for vertices,triangles in groups:
        points=[]
        for p in vertices:
            if kind=='guard': q=(p[0],p[1]-base,p[2])
            elif kind=='car': q=(p[0],p[2]-base,-p[1])
            else: q=(-p[1],p[2]-base,-p[0])
            points.append(tuple(round(v/grid)*grid for v in q))
        for tri in triangles:
            p=[points[i] for i in tri]
            u=[p[1][k]-p[0][k] for k in range(3)];v=[p[2][k]-p[0][k] for k in range(3)]
            n=(u[1]*v[2]-u[2]*v[1],u[2]*v[0]-u[0]*v[2],u[0]*v[1]-u[1]*v[0])
            area=sum(x*x for x in n)
            if area<1e-10: continue
            key=tuple(round(v*64) for q in p for v in q)
            if kind=='car': color=7|15<<5|26<<10
            elif kind=='guard': color=(24|18<<5|13<<10) if sum(q[1] for q in p)/3>1.4 else (5|7<<5|12<<10)
            else: color=12|12<<5|13<<10
            faces[key]=(area,struct.pack('<H9h',color,*key))
    records=[r for _,r in sorted(faces.values(),reverse=True)[:budget]]
    output.write_bytes(struct.pack('<4sI',b'VCM1',len(records))+b''.join(records))
    return {'source':name+'.dff','triangles':len(records),'file':output.name,
            'limitations':'Untextured static simplified mesh; no original animation'}

if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('game',type=Path)
    args=parser.parse_args();workspace=Path(__file__).resolve().parents[1]
    root=workspace/'sdcard/revc/world';archive=Archive(args.game)
    report=[convert(archive,'sentinel',root/'vehicle.vcm',600,.3,'car'),
            convert(archive,'swat',root/'guard.vcm',150,.14,'guard'),
            convert(archive,'colt45',root/'pistol.vcm',60,.02,'weapon')]
    (workspace/'reports/gameplay.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps(report))
