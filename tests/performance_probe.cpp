// Development-only ARM9 workload; timings are component costs, not game FPS.
#include <nds.h>
#include <cstdio>
#include <algorithm>
#include "world.h"
#include "citymap.h"
#include "rom_storage.h"
static slice::World world;
static slice::CityMap citymap;
static slice::Game physics;
static unsigned digest=2166136261u;
static void hash(const void* p,unsigned n){auto bytes=static_cast<const unsigned char*>(p);while(n--)digest=(digest^*bytes++)*16777619u;}
struct Cost {uint64_t total=0,peak=0;unsigned samples=0;void add(uint64_t start){uint64_t n=tickGetCount()-start;total+=n;peak=std::max(peak,n);++samples;}};
static void print(const char* label,const Cost& c){std::printf("%s avg:%5u max:%6u us\n",label,unsigned(c.total*1000000/TICK_FREQ/std::max(1u,c.samples)),unsigned(c.peak*1000000/TICK_FREQ));}
int main(int argc,char** argv){
    tickInit();if(isDSiMode())setCpuClock(true);
    videoSetMode(MODE_0_2D);videoSetModeSub(MODE_5_2D);vramSetBankA(VRAM_A_MAIN_BG);vramSetBankC(VRAM_C_SUB_BG);
    PrintConsole console;consoleInit(&console,0,BgType_Text4bpp,BgSize_T_256x256,31,0,true,true);lcdMainOnTop();
    std::printf("CPU workload - not game FPS\n");
    if(!slice::mountGameAssets(argc,argv) || !world.open("nitro:") || !citymap.open("nitro:/world/citymap.vcm")){
        std::printf("Load failed\n");while(pmMainLoop())swiWaitForVBlank();return 1;}
    for(bool extended:{false,true}){
        if(extended && !world.enableExtendedCache()){std::printf("Extended cache unavailable\n");break;}
        slice::Point seed{-150,10,-350},shift;auto origin=world.anchor();
        if(!world.update({seed.x-origin.x,seed.y-origin.y,seed.z-origin.z},shift,true))return 2;
        world.streamUnitLimit(1);world.renderBudget(700);unsigned previous=0,bytes=0;
        Cost collision,stream,view,bind,map;
        for(unsigned frame=0;frame<120;++frame){
            // One work unit/read per update makes both builds consume the same
            // work independent of timing; retained geometry and budgets match.
            const float travel=frame<60 ? frame*.8f : (119-frame)*.8f;
            origin=world.anchor();slice::Point p{seed.x-origin.x,seed.y-origin.y,seed.z+travel-origin.z},lead=p;
            lead.z+=frame<60 ? 6 : -6;
            auto start=tickGetCount();bool ok=world.updateCollision(p,lead,1);collision.add(start);
            start=tickGetCount();ok=ok && world.update(lead,shift,false,true,frame>=60);stream.add(start);
            if(!ok){std::printf("Stream failed %u\n",frame);return 3;}
            p.x-=shift.x;p.y-=shift.y;p.z-=shift.z;bytes+=world.lastReadBytes();
            if(previous!=world.collisionRevision()){start=tickGetCount();physics.bindGeometry(world.triangles(),world.count());bind.add(start);previous=world.collisionRevision();}
            start=tickGetCount();int n=world.visible({p.x,p.y+1.8f,p.z-4},0,-1,.2182296f,.9758974f,&p);view.add(start);
            hash(&n,sizeof(n));for(int i=0;i<n;++i){hash(&world.visibleTriangle(i),sizeof(RevcTriangle));const auto& m=world.visibleMaterial(i);hash(&m.texture,2);hash(m.uv,sizeof(m.uv));}
            if(frame%4==0){start=tickGetCount();citymap.draw({seed.x,seed.y,seed.z+travel},0,&seed,false);map.add(start);}
        }
        std::printf("%s cache / %u bytes\n",extended ? "Extended" : "DS",bytes);
        print("COL ",collision);print("LOAD",stream);print("VIEW",view);print("BIND",bind);print("MAP ",map);
        std::printf("Hash %08x\n",digest);
    }
    std::printf("Done. Same workload/visuals.\n");
    while(pmMainLoop())swiWaitForVBlank();return 0;
}
