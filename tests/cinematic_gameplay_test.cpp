#include "animation.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
using slice::Animation;
int main(int argc,char** argv){
    assert(argc==2);std::string root=std::string(argv[1])+"/world/";
    struct Actor {const char* name;int limit;bool driver;};
    Actor actors[]={{"player",520,false},{"player2",520,false},{"player3",520,false},
        {"driver",120,true},{"driver2",120,true},{"driver3",120,true},{"fleet-driver",80,true},
        {"male01",80,false},{"hfyst",80,false},{"mercedes",120,false},{"chef",80,false},{"igbuddy",80,false},{"spandxa",80,false}};
    unsigned samples=0;
    for(const auto& spec:actors){
        Animation a;assert(a.open((root+spec.name+".vca").c_str(),spec.limit));assert(a.uv() && a.count()<=spec.limit);
        const int clips=std::strncmp(spec.name,"player",6)==0 ? Animation::ClipCount : 5;
        for(int clip=0;clip<clips;++clip)for(unsigned tick=0;tick<180;++tick){
            const auto* mesh=a.meshAt(Animation::Clip(clip),tick);assert(mesh);
            for(int i=0;i<a.count();++i)for(const auto& v:mesh[i].vertex){
                // A named root applied twice previously moved female running
                // poses four metres away from their collision/scene position.
                assert(v[0]>-128 && v[0]<128 && v[2]>-128 && v[2]<128);
                assert(v[1]>-128 && v[1]<160);if(!spec.driver)assert(v[1]>=0);
            }
            ++samples;
        }
        if(clips==Animation::ClipCount){assert(a.speed(Animation::Run)>6 && a.speed(Animation::Run)<7);assert(a.speed(Animation::Sprint)>14 && a.speed(Animation::Sprint)<15);}
    }
    struct Page {const char* name;unsigned width,height;};
    Page pages[]={{"player",256,128},{"player2",256,128},{"player3",256,128},{"fleet-shared",256,128},{"pedestrians",128,128}};
    for(const auto& p:pages){
        FILE* f=std::fopen((root+p.name+".atx").c_str(),"rb");assert(f);char magic[4];unsigned short w,h;
        assert(std::fread(magic,4,1,f)==1 && !std::memcmp(magic,"ATX1",4));assert(std::fread(&w,2,1,f)==1 && std::fread(&h,2,1,f)==1);
        assert(w==p.width && h==p.height);assert(!std::fseek(f,0,SEEK_END) && std::ftell(f)==long(520+w*h));std::fclose(f);
    }
    std::printf("13 cinematic/gameplay actors: %u animation samples, bounded/grounded poses, original gait speeds, unchanged face ceilings and rectangular atlas sizes passed\n",samples);
}
