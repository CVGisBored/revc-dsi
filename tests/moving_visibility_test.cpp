#include "world.h"
#include <cassert>
#include <algorithm>
#include <cmath>
#include <cstdio>
static slice::World world,reference;
static slice::Game support;
static bool drawn(slice::Point p,int count){
    for(int i=0;i<count;++i){const auto& t=world.visibleTriangle(i);float x=p.x*64,z=p.z*64;
        float ax=t.vertex[0][0],az=t.vertex[0][2],bx=t.vertex[1][0],bz=t.vertex[1][2],cx=t.vertex[2][0],cz=t.vertex[2][2];
        float det=(bx-ax)*(cz-az)-(bz-az)*(cx-ax);if(std::fabs(det)<1)continue;
        float u=((x-ax)*(cz-az)-(z-az)*(cx-ax))/det,v=((bx-ax)*(z-az)-(bz-az)*(x-ax))/det;
        if(u<-.002f || v<-.002f || u+v>1.002f)continue;
        float y=t.vertex[0][1]+u*(t.vertex[1][1]-t.vertex[0][1])+v*(t.vertex[2][1]-t.vertex[0][1]);
        if(std::fabs(y-p.y*64)<24)return true;
    }return false;
}
int main(int argc,char** argv){assert(argc==2);
    for(bool turnaround:{false,true})for(bool extended:{false,true})for(float step:{.6f,.98f,1.2f,2.4f,4.8f}){
        assert(world.open(argv[1]) && reference.open(argv[1]));
        slice::Point seed={-150,10,-350},shift;assert(world.update(seed,shift,true));
        if(extended)assert(world.enableExtendedCache());world.renderBudget(512);
#ifndef OLD_WORLD
        world.streamUnitLimit(2); // Simulate a card allowing only two work pieces/frame.
#endif
        unsigned samples=0,missing=0,bytes=0,maxBytes=0,frames=0;float lag=0;
        for(float travel=0;travel<(turnaround ? 192 : 96);travel+=step){
            auto origin=reference.anchor();float direction=turnaround && travel>=96 ? -1.f : 1.f;
            slice::Point global={seed.x,seed.y,seed.z+(direction>0 ? travel : 192-travel)};
            assert(reference.update({global.x-origin.x,global.y-origin.y,global.z-origin.z},shift,true));origin=reference.anchor();
            slice::Point p={global.x-origin.x,global.y-origin.y,global.z-origin.z};support.bindGeometry(reference.triangles(),reference.count());
            assert(support.placeActor(p,3));assert(support.startExploration(reference.triangles(),reference.count(),p));
            global={support.player.x+origin.x,support.player.y+origin.y,support.player.z+origin.z};
            origin=world.anchor();p={global.x-origin.x,global.y-origin.y,global.z-origin.z};auto lead=p;lead.z+=direction*std::min(step>1 ? 24.f : 6.f,step*36/(step>3 ? 8 : 4));
            assert(world.updateCollision(p,lead));assert(world.update(lead,shift,false,true,step>1));
            origin=world.anchor();p={global.x-origin.x,global.y-origin.y,global.z-origin.z};
#ifndef OLD_WORLD
            assert(std::fabs(p.x)<96 && std::fabs(p.z)<96);
#endif
            auto n=world.visible({p.x,p.y+1.8f,p.z-direction*4},0,-direction,std::sin(.22f),std::cos(.22f),&p);
            unsigned read=world.lastReadBytes();bytes+=read;maxBytes=std::max(maxBytes,read);++frames;lag=std::max(lag,world.visualLag(p));
            for(float ahead:{0.f,2.f,5.f}){auto q=support.player;q.z+=ahead*direction;if(!support.placeActor(q,.3f))continue;
                q={q.x+reference.anchor().x-origin.x,q.y+reference.anchor().y-origin.y,q.z+reference.anchor().z-origin.z};
                ++samples;if(!drawn(q,n))++missing;
            }
        }
        printf("%s %s at %.0f km/h, limited work/frame: %u/%u missing near-ground samples, %u frames, %u bytes, peak %u, scan lag %.1fm",
            turnaround ? "turnaround" : "forward",extended ? "DSi cache" : "DS cache",step*60/(step>3 ? 8 : 4)*3.6f,missing,samples,frames,bytes,maxBytes,lag);
#ifndef OLD_WORLD
        printf(", %u tile publishes (%u scenery), %u tile reuses",world.publishedPatches(),world.publishedSceneryPatches(),world.reusedPatches());
#endif
        puts("");fflush(stdout);
#ifndef OLD_WORLD
        assert(samples>50 && missing==0 && world.publishedPatches()>0 && world.publishedSceneryPatches()>0);
        const auto r=reference.anchor();const slice::Point stopped={support.player.x+r.x,support.player.y+r.y,support.player.z+r.z};
        unsigned settled=0;
        for(;settled<256;++settled){auto a=world.anchor();slice::Point p={stopped.x-a.x,stopped.y-a.y,stopped.z-a.z};
            assert(world.updateCollision(p,p) && world.update(p,shift,false,true));
            if(!world.streaming() && !world.lastReadBytes())break;
        }
        assert(settled<256); // No repeated reads/rebuilds after stopping.
#endif
    }
}
