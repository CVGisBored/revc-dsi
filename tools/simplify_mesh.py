"""Topology-preserving edge collapse with animated-source correspondence.
Only existing mesh edges collapse. Surviving triangles retain their source
material; each new vertex remains a blend of original vertices for skinning.
"""
import heapq
from collections import defaultdict
import numpy as np

def simplify(vertices,faces,budget,attributes=None,preserve_features=False,importance=None):
    source=np.asarray(vertices,dtype=float);triangles=np.asarray(faces,dtype=int)
    if not len(triangles):raise ValueError('Empty mesh')
    # Weld exact seam duplicates, never nearby unrelated surfaces or limbs.
    attr=np.asarray(attributes) if attributes is not None else None
    priority=np.ones(len(source)) if importance is None else np.asarray(importance,dtype=float)
    if priority.shape!=(len(source),) or not np.isfinite(priority).all() or (priority<1).any():raise ValueError('Invalid vertex importance')
    keys={};remap=[];positions=[];influences=[];attrs=[];priorities=[]
    for i,p in enumerate(source):
        key=tuple(np.rint(p*100000).astype(int))
        if attr is not None:key+=tuple(np.rint(attr[i]*10000).astype(int))
        if key not in keys:
            keys[key]=len(positions);positions.append(p.copy());influences.append({i:1.});priorities.append(priority[i])
            if attr is not None:attrs.append(attr[i].copy())
        remap.append(keys[key])
    positions=np.asarray(positions);attrs=np.asarray(attrs) if attr is not None else None
    triangles=np.asarray(remap,dtype=int)[triangles]
    alive=np.array([len(set(f))==3 for f in triangles]);active=np.ones(len(positions),bool)
    adjacency=[set() for _ in positions];quadrics=np.zeros((len(positions),4,4));versions=np.zeros(len(positions),int)
    normals=np.zeros((len(triangles),3))
    for i,(a,b,c) in enumerate(triangles):
        if not alive[i]:continue
        n=np.cross(positions[b]-positions[a],positions[c]-positions[a]);area=np.linalg.norm(n)
        if area<1e-12:alive[i]=False;continue
        n/=area;normals[i]=n;plane=np.r_[n,-np.dot(n,positions[a])];q=np.outer(plane,plane)*area*np.mean([priorities[v] for v in (a,b,c)])
        for v in (a,b,c):adjacency[v].add(i);quadrics[v]+=q
    if preserve_features:
        # Protect panel borders and hard creases when reducing vehicle bodies.
        edge_faces=defaultdict(list)
        for i,face in enumerate(triangles):
            if alive[i]:
                for j in range(3):edge_faces[tuple(sorted((int(face[j]),int(face[(j+1)%3]))))].append(i)
        for (a,b),adjacent in edge_faces.items():
            edge=positions[b]-positions[a];length=np.linalg.norm(edge)
            if length<1e-10:continue
            if len(adjacent)==1:
                n=np.cross(edge/length,normals[adjacent[0]]);n/=max(np.linalg.norm(n),1e-10)
                plane=np.r_[n,-np.dot(n,positions[a])];q=np.outer(plane,plane)*length*10
                quadrics[a]+=q;quadrics[b]+=q
            elif len(adjacent)==2 and np.dot(normals[adjacent[0]],normals[adjacent[1]])<.7:
                for f in adjacent:
                    n=normals[f];plane=np.r_[n,-np.dot(n,positions[a])];q=np.outer(plane,plane)*length*4
                    quadrics[a]+=q;quadrics[b]+=q
    def neighbors(v):
        return {int(x) for f in adjacency[v] for x in triangles[f] if x!=v}
    def boundary(v):
        counts=defaultdict(int)
        for f in adjacency[v]:
            for x in triangles[f]:
                if x!=v:counts[int(x)]+=1
        return any(n==1 for n in counts.values())
    queue=[];serial=0
    def candidate(a,b):
        nonlocal serial
        if a>b:a,b=b,a
        if not active[a] or not active[b]:return
        shared=adjacency[a]&adjacency[b]
        if not 1<=len(shared)<=2:return
        ba,bb=boundary(a),boundary(b)
        if ba and bb and len(shared)!=1:return
        q=quadrics[a]+quadrics[b];p=np.r_[positions[a],1.];d=np.r_[positions[b]-positions[a],0.]
        denominator=float(d@q@d);t=np.clip(-float(d@q@p)/denominator,0,1) if denominator>1e-16 else .5
        if ba and not bb:t=0.
        elif bb and not ba:t=1.
        target=p+d*t;error=max(0.,float(target@q@target))
        # Avoid mixing differently moving joints before simpler regions are reduced.
        if attrs is not None:error+=.015*np.sum((attrs[a]-attrs[b])**2)*np.sum(d[:3]**2)
        error+=np.sum(d[:3]**2)*1e-7
        serial+=1;heapq.heappush(queue,(error,serial,a,b,int(versions[a]),int(versions[b]),float(t)))
    edges={tuple(sorted((int(f[j]),int(f[(j+1)%3])))) for i,f in enumerate(triangles) if alive[i] for j in range(3)}
    for a,b in sorted(edges):candidate(a,b)
    count=int(alive.sum());collapses=0
    while count>budget and queue:
        _,_,a,b,va,vb,t=heapq.heappop(queue)
        if not active[a] or not active[b] or va!=versions[a] or vb!=versions[b]:continue
        shared=adjacency[a]&adjacency[b]
        if not 1<=len(shared)<=2:continue
        # Link condition prevents pinching different surface sheets together.
        opposite={int(v) for f in shared for v in triangles[f] if v not in (a,b)}
        if neighbors(a)&neighbors(b)!=opposite:continue
        target=positions[a]*(1-t)+positions[b]*t;affected=adjacency[a]|adjacency[b];valid=True
        for f in affected-shared:
            ids=triangles[f];points=np.array([target if v in (a,b) else positions[v] for v in ids])
            n=np.cross(points[1]-points[0],points[2]-points[0]);length=np.linalg.norm(n)
            if length<1e-10 or np.dot(n/length,normals[f])<.2:valid=False;break
        if not valid:continue
        touched=neighbors(a)|neighbors(b)|{a,b}
        for f in affected:
            for v in triangles[f]:adjacency[v].discard(f)
        for f in shared:alive[f]=False;count-=1
        positions[a]=target;quadrics[a]+=quadrics[b]
        blended=defaultdict(float)
        for old,weight in influences[a].items():blended[old]+=weight*(1-t)
        for old,weight in influences[b].items():blended[old]+=weight*t
        influences[a]={i:w for i,w in blended.items() if w>1e-9}
        if attrs is not None:attrs[a]=attrs[a]*(1-t)+attrs[b]*t
        active[b]=False;adjacency[b].clear();collapses+=1
        for f in affected-shared:
            triangles[f][triangles[f]==b]=a
            pts=positions[triangles[f]];n=np.cross(pts[1]-pts[0],pts[2]-pts[0]);normals[f]=n/np.linalg.norm(n)
            for v in triangles[f]:adjacency[v].add(f)
        for v in touched:versions[v]+=1
        refresh={tuple(sorted((v,x))) for v in touched if active[v] for x in neighbors(v)}
        for u,v in sorted(refresh):candidate(u,v)
    if count>budget:raise ValueError(f'Cannot safely simplify mesh to {budget}: {count} triangles remain')
    ids=np.flatnonzero(alive);used=sorted({int(v) for f in triangles[ids] for v in f});mapping={v:i for i,v in enumerate(used)}
    result=np.array([[mapping[int(v)] for v in triangles[f]] for f in ids],dtype=int)
    weights=np.zeros((len(used),len(source)))
    for row,v in enumerate(used):
        for old,w in influences[v].items():weights[row,old]=w
    return positions[used],result,weights,ids,{'source_faces':len(faces),'faces':len(ids),'vertices':len(used),'collapses':collapses,'method':'edge collapse with topology/normal checks; no face selection'}
