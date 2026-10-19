#include "gameplay.h"
#include <cmath>
#include <algorithm>
#include <limits>
#include <cstring>

namespace slice {
namespace {
constexpr float radius = .4f, stepHeight = .45f;
Point vertex(const RevcTriangle& t, int i) {
    return {t.vertex[i][0]/64.f, t.vertex[i][1]/64.f, t.vertex[i][2]/64.f};
}
float distance2(Point a, Point b) {
    return (a.x-b.x)*(a.x-b.x)+(a.z-b.z)*(a.z-b.z);
}
float segmentDistance2(Point p, Point a, Point b) {
    const float length = distance2(a,b);
    float u = length > 1e-8f ? ((p.x-a.x)*(b.x-a.x)+(p.z-a.z)*(b.z-a.z))/length : 0;
    u = std::max(0.f, std::min(1.f,u));
    return distance2(p, {a.x+u*(b.x-a.x), 0, a.z+u*(b.z-a.z)});
}
bool walkable(Point a, Point b, Point c) {
    const float ux=b.x-a.x, uy=b.y-a.y, uz=b.z-a.z;
    const float vx=c.x-a.x, vy=c.y-a.y, vz=c.z-a.z;
    const float nx=uy*vz-uz*vy, ny=uz*vx-ux*vz, nz=ux*vy-uy*vx;
    return ny*ny > 1e-8f && ny*ny >= nx*nx+nz*nz;
}
}

bool Game::ground(float x, float z, float low, float high, float& y) const {
    bool found=false;
    y=low;
    const float fx=x*64, fz=z*64;
    const int fxi=int(std::floor(fx)),fzi=int(std::floor(fz));
    const int cell=bucket(x,z);
    if(indexed && cell<0) return false;
    const int first=indexed ? starts[cell] : 0,last=indexed ? starts[cell+1] : meshCount;
    for (int item=first;item<last;++item) {
        const int i=indexed ? references[item] : item;
        const auto& t=mesh[i];
        if(fxi<std::min({t.vertex[0][0],t.vertex[1][0],t.vertex[2][0]}) ||
           fxi>std::max({t.vertex[0][0],t.vertex[1][0],t.vertex[2][0]}) ||
           fzi<std::min({t.vertex[0][2],t.vertex[1][2],t.vertex[2][2]}) ||
           fzi>std::max({t.vertex[0][2],t.vertex[1][2],t.vertex[2][2]})) continue;
        if(planesCached) {
            const auto& plane=floorPlanes[i];if(plane.inverseDet==0)continue;
            const auto& v=t.vertex;
            const float px=fx-v[2][0],pz=fz-v[2][2];
            const float u=((v[1][2]-v[2][2])*px+(v[2][0]-v[1][0])*pz)*plane.inverseDet;
            const float w=((v[2][2]-v[0][2])*px+(v[0][0]-v[2][0])*pz)*plane.inverseDet;
            if(u<-.0001f || w<-.0001f || u+w>1.0001f)continue;
            const float h=plane.x*x+plane.z*z+plane.height;
            if(h>=low && h<=high && (!found || h>y)){y=h;found=true;}
            continue;
        }
        Point a=vertex(mesh[i],0), b=vertex(mesh[i],1), c=vertex(mesh[i],2);
        if (!walkable(a,b,c)) continue;
        const float det=(b.z-c.z)*(a.x-c.x)+(c.x-b.x)*(a.z-c.z);
        const float u=((b.z-c.z)*(x-c.x)+(c.x-b.x)*(z-c.z))/det;
        const float v=((c.z-a.z)*(x-c.x)+(a.x-c.x)*(z-c.z))/det;
        if (u < -.0001f || v < -.0001f || u+v > 1.0001f) continue;
        const float h=u*a.y+v*b.y+(1-u-v)*c.y;
        if (h>=low && h<=high && (!found || h>y)) { y=h; found=true; }
    }
    return found;
}

bool Game::blocked(Point p,float footprint) const {
    const int fxi=int(std::floor(p.x*64)),fzi=int(std::floor(p.z*64)),r=int(std::ceil(footprint*64))+1;
    const int cell=bucket(p.x,p.z);
    if(indexed && cell<0) return true;
    const int first=indexed ? starts[cell] : 0,last=indexed ? starts[cell+1] : meshCount;
    for(int item=first;item<last;++item) {
        const int i=indexed ? references[item] : item;
        const auto& t=mesh[i];
        if(fxi<std::min({t.vertex[0][0],t.vertex[1][0],t.vertex[2][0]})-r ||
           fxi>std::max({t.vertex[0][0],t.vertex[1][0],t.vertex[2][0]})+r ||
           fzi<std::min({t.vertex[0][2],t.vertex[1][2],t.vertex[2][2]})-r ||
           fzi>std::max({t.vertex[0][2],t.vertex[1][2],t.vertex[2][2]})+r) continue;
        if(planesCached && floorPlanes[i].inverseDet!=0)continue;
        Point v[3]={vertex(mesh[i],0),vertex(mesh[i],1),vertex(mesh[i],2)};
        if (walkable(v[0],v[1],v[2])) continue;
        if (p.x < std::min({v[0].x,v[1].x,v[2].x})-footprint ||
            p.x > std::max({v[0].x,v[1].x,v[2].x})+footprint ||
            p.z < std::min({v[0].z,v[1].z,v[2].z})-footprint ||
            p.z > std::max({v[0].z,v[1].z,v[2].z})+footprint) continue;
        // Slice the triangle at foot, torso and head height, then test the
        // circular player footprint against the resulting horizontal segment.
        for (float height : {.25f,.85f,1.6f}) {
            const float y=p.y+height;
            Point hits[3]; int n=0;
            for (int j=0; j<3; ++j) {
                Point a=v[j],b=v[(j+1)%3];
                if ((a.y<=y && b.y>y) || (b.y<=y && a.y>y)) {
                    const float u=(y-a.y)/(b.y-a.y);
                    hits[n++]={a.x+u*(b.x-a.x),y,a.z+u*(b.z-a.z)};
                }
            }
            if (n==2 && segmentDistance2(p,hits[0],hits[1])<footprint*footprint) return true;
        }
    }
    return false;
}

bool Game::blockedPlayer(Point p) const {
    // CTempColModels::ms_colModelPed1: three .35 m spheres, expressed here
    // relative to supported feet (CPed's engine origin is FEET_OFFSET=1.04).
    constexpr float r=.35f;const int cell=bucket(p.x,p.z);if(indexed && cell<0)return true;
    const int first=indexed ? starts[cell] : 0,last=indexed ? starts[cell+1] : meshCount;
    auto sub=[](Point a,Point b){return Point{a.x-b.x,a.y-b.y,a.z-b.z};};
    auto dot=[](Point a,Point b){return a.x*b.x+a.y*b.y+a.z*b.z;};
    auto add=[](Point a,Point b,float scale){return Point{a.x+b.x*scale,a.y+b.y*scale,a.z+b.z*scale};};
    // Reject in fixed-point before converting vertices to soft-float. Most
    // faces in a grid cell cannot touch the player's three collision spheres.
    const int fx=int(std::floor(p.x*64)),fz=int(std::floor(p.z*64));
    const int lowY=int(std::floor((p.y+.44f)*64)),highY=int(std::ceil((p.y+1.94f)*64));
    for(int item=first;item<last;++item) {
        const auto& t=mesh[indexed ? references[item] : item];
        if(fx<std::min({t.vertex[0][0],t.vertex[1][0],t.vertex[2][0]})-24 ||
           fx>std::max({t.vertex[0][0],t.vertex[1][0],t.vertex[2][0]})+24 ||
           fz<std::min({t.vertex[0][2],t.vertex[1][2],t.vertex[2][2]})-24 ||
           fz>std::max({t.vertex[0][2],t.vertex[1][2],t.vertex[2][2]})+24 ||
           lowY>std::max({t.vertex[0][1],t.vertex[1][1],t.vertex[2][1]}) ||
           highY<std::min({t.vertex[0][1],t.vertex[1][1],t.vertex[2][1]}))continue;
        Point a=vertex(t,0),b=vertex(t,1),c=vertex(t,2);
        if(p.x<std::min({a.x,b.x,c.x})-r || p.x>std::max({a.x,b.x,c.x})+r ||
           p.z<std::min({a.z,b.z,c.z})-r || p.z>std::max({a.z,b.z,c.z})+r ||
           p.y+.44f>std::max({a.y,b.y,c.y}) || p.y+1.94f<std::min({a.y,b.y,c.y}))continue;
        Point ab=sub(b,a),ac=sub(c,a);
        for(float h : {.79f,1.19f,1.59f}) {
            Point center={p.x,p.y+h,p.z};
            if(center.y<std::min({a.y,b.y,c.y})-r || center.y>std::max({a.y,b.y,c.y})+r)continue;
            Point ap=sub(center,a),closest;
            float d1=dot(ab,ap),d2=dot(ac,ap);
            if(d1<=0 && d2<=0)closest=a;
            else {
                Point bp=sub(center,b);float d3=dot(ab,bp),d4=dot(ac,bp),vc=d1*d4-d3*d2;
                if(d3>=0 && d4<=d3)closest=b;
                else if(vc<=0 && d1>=0 && d3<=0)closest=add(a,ab,d1/(d1-d3));
                else {
                    Point cp=sub(center,c);float d5=dot(ab,cp),d6=dot(ac,cp),vb=d5*d2-d1*d6,va=d3*d6-d5*d4;
                    if(d6>=0 && d5<=d6)closest=c;
                    else if(vb<=0 && d2>=0 && d6<=0)closest=add(a,ac,d2/(d2-d6));
                    else if(va<=0 && d4-d3>=0 && d5-d6>=0)closest=add(b,sub(c,b),(d4-d3)/((d4-d3)+(d5-d6)));
                    else {
                        float sum=va+vb+vc;if(std::fabs(sum)<1e-12f)continue;
                        closest=add(add(a,ab,vb/sum),ac,vc/sum);
                    }
                }
            }
            Point delta=sub(center,closest);if(dot(delta,delta)<r*r-1e-6f)return true;
        }
    }
    return false;
}

bool Game::step(Point& p,float dx,float dz) const {
    Point next={p.x+dx,p.y,p.z+dz};
    float y;
    if (!ground(next.x,next.z,p.y-stepHeight,p.y+stepHeight,y)) return false;
    next.y=y;
    // Support the whole footprint, rather than just its center at ledges.
    for (Point offset : {Point{radius,0,0}, Point{-radius,0,0},
                         Point{0,0,radius},Point{0,0,-radius}}) {
        float support;
        if (!ground(next.x+offset.x,next.z+offset.z,y-stepHeight,y+stepHeight,support)) return false;
    }
    if (blocked(next)) return false;
    p=next;
    return true;
}
bool Game::placeActor(Point& p,float tolerance) const {
    float y;if(!ground(p.x,p.z,p.y-tolerance,p.y+std::min(tolerance,.45f),y))return false;
    Point candidate={p.x,y,p.z};if(!step(candidate,0,0))return false;
    p=candidate;return true;
}
bool Game::actorMove(Point& p,float dx,float dz) const {
    if(!std::isfinite(dx) || !std::isfinite(dz))return false;
    float length=std::sqrt(dx*dx+dz*dz);if(length>8.f)return false;
    const int steps=std::max(1,int(std::ceil(length/.18f)));bool moved=false;
    for(int i=0;i<steps;++i) {
        if(step(p,dx/steps,dz/steps))moved=true;
        else {moved=step(p,dx/steps,0)||moved;moved=step(p,0,dz/steps)||moved;}
    }
    return moved;
}

bool Game::move(float dx,float dz) {
    if (state!=Playing || !std::isfinite(dx) || !std::isfinite(dz)) return false;
    const float length=std::sqrt(dx*dx+dz*dz);
    if (length>8.f) return false;
    const int steps=std::max(1,static_cast<int>(std::ceil(length/.18f)));
    bool moved=false;
    for (int i=0;i<steps;++i) {
        if (step(player,dx/steps,dz/steps)) moved=true;
        else { // Slide along a blocking surface.
            moved=step(player,dx/steps,0) || moved;
            moved=step(player,0,dz/steps) || moved;
        }
    }
    return moved;
}

bool Game::movePlayer(float dx,float dz,float seconds,float impulse) {
    if(state!=Playing || !std::isfinite(dx) || !std::isfinite(dz) || !std::isfinite(seconds) || seconds<=0 || seconds>.05f)return false;
    if(impulse>0 && grounded){verticalSpeed=impulse;grounded=false;}
    bool moved=false;const int steps=std::max(1,int(std::ceil(std::sqrt(dx*dx+dz*dz)/.3f)));
    auto horizontal=[&](float x,float z) {
        Point next={player.x+x,player.y,player.z+z};float floor;
        bool supported=grounded && ground(next.x,next.z,player.y-stepHeight,player.y+stepHeight,floor);
        if(supported)next.y=floor;
        // Retain the streamed-world guard: never walk into absent collision data.
        if(!supported && !ground(next.x,next.z,player.y-64,player.y+.02f,floor))return false;
        if(blockedPlayer(next))return false;
        player=next;if(!supported)grounded=false;return true;
    };
    for(int i=0;i<steps;++i) {
        float x=dx/steps,z=dz/steps;
        if(horizontal(x,z))moved=true;
        else {moved=horizontal(x,0)||moved;moved=horizontal(0,z)||moved;}
    }
    if(!grounded) {
        // CPhysical gravity .008 and velocity units are based on 50 Hz.
        verticalSpeed-=.008f*50*50*seconds;
        const float speed2=(dx*dx+dz*dz)/(seconds*seconds)+verticalSpeed*verticalSpeed;
        verticalSpeed*=std::pow(1.f/(1.f+(.4f/70)*.5f*speed2/(50*50)),seconds*50);
        float nextY=player.y+verticalSpeed*seconds,floor;
        if(verticalSpeed<=0 && ground(player.x,player.z,nextY-.03f,player.y+.02f,floor)) {
            player.y=floor;verticalSpeed=0;grounded=true;
        } else {
            const int verticalSteps=std::max(1,int(std::ceil(std::fabs(nextY-player.y)/.08f)));
            const float dy=(nextY-player.y)/verticalSteps;
            for(int i=0;i<verticalSteps;++i) {
                Point next=player;next.y+=dy;
                if(blockedPlayer(next)){verticalSpeed=0;break;}
                // Horizontal triangles also include low ceilings.
                float ceiling;
                if(dy>0 && ground(player.x,player.z,player.y+1.65f,next.y+1.65f,ceiling)) {verticalSpeed=0;break;}
                player=next;
            }
        }
    }
    return moved;
}

bool Game::vehiclePose(Point& p,float sine,float cosine,Point* tilt) const {
    float y;
    if(!ground(p.x,p.z,p.y-.45f,p.y+.45f,y)) return false;
    Point center={p.x,y,p.z};
    if(blocked(center,.1f)) return false;
    float front=0,rear=0,right=0,left=0;
    for(float side : {-1.f,1.f}) for(float length : {-2.45f,2.45f}) {
        Point wheel={center.x+side*cosine+length*sine,y,center.z+side*sine-length*cosine};
        float support;
        if(!ground(wheel.x,wheel.z,y-1.5f,y+1.5f,support))return false;
        wheel.y=support;if(blocked(wheel,.1f))return false;
        if(length>0)front+=support;else rear+=support;
        if(side>0)right+=support;else left+=support;
    }
    if(tilt)*tilt={std::atan2((front-rear)*.5f,4.9f),0,std::atan2((right-left)*.5f,2.f)};
    p=center;return true;
}
bool Game::vehicleMove(float dx,float dz,float sine,float cosine,Point* tilt) {
    if(state!=Playing || !std::isfinite(dx) || !std::isfinite(dz)) return false;
    return vehicleActorMove(player,dx,dz,sine,cosine,tilt);
}
bool Game::vehicleActorMove(Point& p,float dx,float dz,float sine,float cosine,Point* tilt) const {
    if(!std::isfinite(dx) || !std::isfinite(dz))return false;
    float length=std::sqrt(dx*dx+dz*dz);
    if(length>8) return false;
    const int steps=std::max(1,static_cast<int>(std::ceil(length/.15f)));
    bool moved=false;
    for(int i=0;i<steps;++i) {
        Point next={p.x+dx/steps,p.y,p.z+dz/steps};
        if(!vehiclePose(next,sine,cosine,tilt)) break;
        p=next;moved=true;
    }
    return moved;
}
bool Game::lineClear(Point a,Point b) const {
    return traceFraction(a,b,false)==1.f;
}
Point Game::cameraPosition(Point focus,Point desired) const {
    const float hit=traceFraction(focus,desired,true);
    if(hit==1.f)return desired;
    const float dx=desired.x-focus.x,dy=desired.y-focus.y,dz=desired.z-focus.z;
    const float length=std::sqrt(dx*dx+dy*dy+dz*dz);
    const float fraction=std::max(0.f,hit-(length>0 ? .3f/length : 0.f));
    return {focus.x+dx*fraction,focus.y+dy*fraction,focus.z+dz*fraction};
}
float Game::traceFraction(Point a,Point b,bool nearest) const {
    float closest=1.f;
    const Point direction={b.x-a.x,b.y-a.y,b.z-a.z};
    const int low[3]={int(std::floor(std::min(a.x,b.x)*64)),int(std::floor(std::min(a.y,b.y)*64)),int(std::floor(std::min(a.z,b.z)*64))};
    const int high[3]={int(std::ceil(std::max(a.x,b.x)*64)),int(std::ceil(std::max(a.y,b.y)*64)),int(std::ceil(std::max(a.z,b.z)*64))};
    auto intersect=[&](int i) {
        const auto& t=mesh[i];bool overlaps=true;
        for(int k=0;k<3;++k) if(high[k]<std::min({t.vertex[0][k],t.vertex[1][k],t.vertex[2][k]}) ||
            low[k]>std::max({t.vertex[0][k],t.vertex[1][k],t.vertex[2][k]})) overlaps=false;
        if(!overlaps) return 1.f;
        const Point p=vertex(t,0),q=vertex(t,1),r=vertex(t,2);
        const Point u={q.x-p.x,q.y-p.y,q.z-p.z},v={r.x-p.x,r.y-p.y,r.z-p.z};
        const Point h={direction.y*v.z-direction.z*v.y,direction.z*v.x-direction.x*v.z,
                       direction.x*v.y-direction.y*v.x};
        const float det=u.x*h.x+u.y*h.y+u.z*h.z;
        if(std::fabs(det)<1e-6f) return 1.f;
        const float inv=1/det;
        const Point s={a.x-p.x,a.y-p.y,a.z-p.z};
        float w=(s.x*h.x+s.y*h.y+s.z*h.z)*inv;
        if(w<0 || w>1) return 1.f;
        const Point cross={s.y*u.z-s.z*u.y,s.z*u.x-s.x*u.z,s.x*u.y-s.y*u.x};
        float z=(direction.x*cross.x+direction.y*cross.y+direction.z*cross.z)*inv;
        float distance=(v.x*cross.x+v.y*cross.y+v.z*cross.z)*inv;
        if(z>=0 && w+z<=1 && distance>.001f && distance<.999f) {
            return distance;
        }
        return 1.f;
    };
    const int first=bucket(std::min(a.x,b.x),std::min(a.z,b.z));
    const int last=bucket(std::max(a.x,b.x),std::max(a.z,b.z));
    if(indexed && first>=0 && last>=0) {
        std::memset(traceVisited,0,(meshCount+7)/8);
        for(int z=first/GridSide;z<=last/GridSide;++z)for(int x=first%GridSide;x<=last%GridSide;++x) {
            const int cell=x+z*GridSide;
            for(uint32_t r=starts[cell];r<starts[cell+1];++r) {
                const unsigned i=references[r],bit=1u<<(i%8);
                if(traceVisited[i/8]&bit)continue;
                traceVisited[i/8]|=bit;
                const float distance=intersect(i);
                if(distance<1.f && !nearest)return distance;
                closest=std::min(closest,distance);
            }
        }
    } else for(int i=0;i<meshCount;++i) {
        const float distance=intersect(i);
        if(distance<1.f && !nearest)return distance;
        closest=std::min(closest,distance);
    }
    return closest;
}

int Game::bucket(float x,float z) const {
    const float scale=cellFixed==256 ? .25f : .125f;
    int ix=static_cast<int>(std::floor((x-gridOriginX/64.f)*scale)),iz=static_cast<int>(std::floor((z-gridOriginZ/64.f)*scale));
    return ix<0 || iz<0 || ix>=GridSide || iz>=GridSide ? -1 : ix+iz*GridSide;
}

void Game::bindGeometry(const RevcTriangle* triangles,int count) {
    mesh=triangles;meshCount=count;indexed=false;
    if(!mesh || count<=0 || count>65535) return;
    planesCached=count<=4096;
    if(planesCached)for(int i=0;i<count;++i) {
        auto& plane=floorPlanes[i];plane.inverseDet=0;
        const auto& v=mesh[i].vertex;
        int64_t ux=v[1][0]-v[0][0],uy=v[1][1]-v[0][1],uz=v[1][2]-v[0][2];
        int64_t vx=v[2][0]-v[0][0],vy=v[2][1]-v[0][1],vz=v[2][2]-v[0][2];
        int64_t nx=uy*vz-uz*vy,ny=uz*vx-ux*vz,nz=ux*vy-uy*vx;
        if(!ny || ny*ny<nx*nx+nz*nz)continue;
        // ARM9 has no FPU. Reject walls in integer space and reuse a single
        // reciprocal for the floor equation and barycentric support test.
        plane.inverseDet=-1.f/float(ny);
        plane.x=float(nx)*plane.inverseDet;plane.z=float(nz)*plane.inverseDet;
        plane.height=(v[0][1]-plane.x*v[0][0]-plane.z*v[0][2])/64.f;
    }
    // Four metre cells reduce repeated sphere and ground candidates while
    // moving. Dense regions retain the eight metre fallback within the same
    // fixed reference allocation.
    int minX=32767,minZ=32767,maxX=-32768,maxZ=-32768;
    for(int i=0;i<count;++i)for(const auto& v:mesh[i].vertex) {
        minX=std::min(minX,int(v[0]));maxX=std::max(maxX,int(v[0]));
        minZ=std::min(minZ,int(v[2]));maxZ=std::max(maxZ,int(v[2]));
    }
    for(cellFixed=256;cellFixed<=512;cellFixed*=2) {
    auto floorCell=[this](int value){int q=value/cellFixed;if(value%cellFixed<0)--q;return q*cellFixed;};
    // Collision can advance before the visual origin. Keep its broad phase
    // attached to the actual snapshot instead of an old fixed [-64,128] box.
    gridOriginX=floorCell(minX-26);gridOriginZ=floorCell(minZ-26);
    if((maxX+26-gridOriginX)/cellFixed>=GridSide || (maxZ+26-gridOriginZ)/cellFixed>=GridSide)continue;
    uint32_t counts[GridCells]={};
    auto bounds=[this](int i,int* range) {
        const auto& t=mesh[i];
        int x0=std::min({t.vertex[0][0],t.vertex[1][0],t.vertex[2][0]});
        int x1=std::max({t.vertex[0][0],t.vertex[1][0],t.vertex[2][0]});
        int z0=std::min({t.vertex[0][2],t.vertex[1][2],t.vertex[2][2]});
        int z1=std::max({t.vertex[0][2],t.vertex[1][2],t.vertex[2][2]});
        range[0]=std::max(0,(x0-gridOriginX-26)/cellFixed);range[1]=std::min(GridSide-1,(x1-gridOriginX+26)/cellFixed);
        range[2]=std::max(0,(z0-gridOriginZ-26)/cellFixed);range[3]=std::min(GridSide-1,(z1-gridOriginZ+26)/cellFixed);
        return true;
    };
    for(int i=0;i<count;++i) {
        int range[4];if(!bounds(i,range)) return;
        for(int z=range[2];z<=range[3];++z) for(int x=range[0];x<=range[1];++x) ++counts[x+z*GridSide];
    }
    starts[0]=0;
    for(int i=0;i<GridCells;++i) starts[i+1]=starts[i]+counts[i];
    if(starts[GridCells]>65536) continue;
    for(int i=0;i<GridCells;++i) counts[i]=starts[i];
    for(int i=0;i<count;++i) {
        int range[4];bounds(i,range);
        for(int z=range[2];z<=range[3];++z) for(int x=range[0];x<=range[1];++x)
            references[counts[x+z*GridSide]++]=i;
    }
    indexed=true;return;
    }
}

bool Game::init(const RevcTriangle* triangles,int count,const Point* preferred) {
    state=LoadingFailed; routeCount=0;
    bindGeometry(triangles,count);
    if (!mesh || meshCount<=0) return false;
    float best=std::numeric_limits<float>::max(); bool found=false;
    for (int i=0;i<meshCount;++i) {
        Point a=vertex(mesh[i],0),b=vertex(mesh[i],1),c=vertex(mesh[i],2);
        if (!walkable(a,b,c)) continue;
        Point p={(a.x+b.x+c.x)/3,(a.y+b.y+c.y)/3,(a.z+b.z+c.z)/3};
        const float dx=p.x-(preferred ? preferred->x : 0),dz=p.z-(preferred ? preferred->z : 0);
        const float dy=preferred ? p.y-preferred->y : 0;
        const float score=dx*dx+dz*dz+dy*dy;
        Point probe=p;
        if (score<best && step(probe,0,0)) { spawn=probe; best=score; found=true; }
    }
    if (!found) return false;
    // Flood a bounded grid through actual collision checks. Checkpoints use
    // one continuous reachable route, never an arbitrary roof across a gap.
    Point nodes[128]; int parent[128],gx[128],gz[128];
    nodes[0]=spawn; parent[0]=-1; gx[0]=gz[0]=0;
    int size=1,far=0,depth[128]={};
    const int directions[4][2]={{0,-1},{1,0},{0,1},{-1,0}};
    for (int head=0;head<size && size<128;++head) for (auto& d:directions) {
        int x=gx[head]+d[0],z=gz[head]+d[1]; bool seen=false;
        for(int k=0;k<size;++k) if(gx[k]==x && gz[k]==z) {seen=true;break;}
        if (seen || size==128) continue;
        Point p=nodes[head]; bool ok=true;
        for(int k=0;k<20;++k) if(!step(p,d[0]*.15f,d[1]*.15f)) {ok=false;break;}
        if(!ok) continue;
        nodes[size]=p; parent[size]=head; gx[size]=x; gz[size]=z; depth[size]=depth[head]+1;
        if(depth[size]>depth[far]) far=size;
        ++size;
    }
    for(int k=far;k>=0;k=parent[k]) route[routeCount++]=nodes[k];
    std::reverse(route,route+routeCount);
    if(routeCount<4) {routeCount=0;return false;}
    targets[0]=(routeCount-1)/3;
    targets[1]=2*(routeCount-1)/3;
    targets[2]=routeCount-1;
    restart();
    return true;
}

void Game::restart() {
    resetPlayerPhysics();
    if(routeCount<4 && exploring) {player=spawn;collected=0;state=Playing;return;}
    if(routeCount<4) {state=LoadingFailed;return;}
    player=spawn; collected=0; framesLeft=120*60;
    checkpoint=route[targets[0]]; state=Playing;
}
bool Game::startExploration(const RevcTriangle* triangles,int count,Point preferred) {
    resetPlayerPhysics();
    state=LoadingFailed;routeCount=0;
    bindGeometry(triangles,count);
    if(!mesh || count<=0) return false;
    bool safe=step(preferred,0,0);
    const Point seed=preferred;
    for(int ring=1;!safe && ring<=8;++ring) {
        for(int z=-ring;!safe && z<=ring;++z) for(int x=-ring;!safe && x<=ring;++x) {
            if(std::abs(x)!=ring && std::abs(z)!=ring) continue;
            Point candidate={seed.x+x*.5f,seed.y,seed.z+z*.5f};
            if(step(candidate,0,0)) {preferred=candidate;safe=true;}
        }
    }
    if(!safe) return false;
    spawn=player=preferred;exploring=true;collected=0;framesLeft=0;state=Playing;
    return true;
}
void Game::pause() {
    if(state==Playing) state=Paused;
    else if(state==Paused) state=Playing;
}
void Game::tick(int elapsedFrames) {
    if(state!=Playing || exploring) return;
    if(distance2(player,checkpoint)<1.f && std::fabs(player.y-checkpoint.y)<.6f) {
        if(++collected==3) {state=Won;return;}
        checkpoint=route[targets[collected]];
    }
    framesLeft-=std::max(0,std::min(120*60,elapsedFrames));
    if(framesLeft<=0) {framesLeft=0;state=TimedOut;}
}
void Game::rebase(Point shift) {
    auto apply=[shift](Point& p){p.x-=shift.x;p.y-=shift.y;p.z-=shift.z;};
    apply(player);apply(checkpoint);apply(spawn);
    for(int i=0;i<routeCount;++i) apply(route[i]);
}
void Game::explore(bool enabled) {
    exploring=enabled;
    if(enabled && (state==Won || state==TimedOut)) state=Playing;
}
}
