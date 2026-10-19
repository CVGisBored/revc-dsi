#include "vehicle_animation.h"
#include "vehicle_handling.h"
#include "animation.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>
#include <string>
using namespace slice;
int main(int argc,char** argv) {
    assert(argc==2);
    for(const char* name : {"vehicle","traffic"}) {
        std::string base=std::string(argv[1])+"/world/"+name;
        FILE* f=std::fopen((base+".vcm").c_str(),"rb");assert(f);
        char magic[4];uint32_t n=0;assert(std::fread(magic,4,1,f)==1 && !std::memcmp(magic,"VCM1",4));
        assert(std::fread(&n,4,1,f)==1 && n<=640);
        std::vector<RevcTriangle> source(n);assert(std::fread(source.data(),sizeof(RevcTriangle),n,f)==n);std::fclose(f);
        VehicleAnimation animation;assert(animation.open((base+".vpa").c_str(),n));
        auto neutral=animation.pose(source.data(),n,0,0,0,-1);
        assert(!std::memcmp(neutral,source.data(),n*sizeof(RevcTriangle)));
        auto posed=animation.pose(source.data(),n,1.1f,.3f,1.25f,-1);
        unsigned changed=0;
        for(unsigned i=0;i<n;++i) {
            assert(posed[i].color==source[i].color);
            if(std::memcmp(posed[i].vertex,source[i].vertex,sizeof(posed[i].vertex)))++changed;
            // Rigid parts preserve each triangle's edge lengths within v16 rounding.
            for(int edge=0;edge<3;++edge) {
                double a=0,b=0;
                for(int k=0;k<3;++k) {
                    double d=source[i].vertex[edge][k]-source[i].vertex[(edge+1)%3][k];a+=d*d;
                    d=posed[i].vertex[edge][k]-posed[i].vertex[(edge+1)%3][k];b+=d*d;
                }
                assert(std::fabs(std::sqrt(a)-std::sqrt(b))<2);
            }
        }
        assert(changed>=96 && changed<n);
        f=std::fopen((base+".vpa").c_str(),"rb");assert(f);assert(!std::fseek(f,50,SEEK_SET));
        std::vector<unsigned char> part(n);assert(std::fread(part.data(),1,n,f)==n);std::fclose(f);
        auto transfer=animation.pose(source.data(),n,0,0,0,-1,{.06f,0,.08f});
        unsigned bodyChanged=0;
        for(unsigned i=0;i<n;++i) {
            bool changed=std::memcmp(transfer[i].vertex,source[i].vertex,sizeof(transfer[i].vertex))!=0;
            if(part[i]>=1 && part[i]<=4)assert(!changed);
            else if(changed)++bodyChanged;
        }
        assert(bodyChanged>200);
        assert(!animation.open((base+".vpa").c_str(),n-1));
    }
    Animation driver;assert(driver.open((std::string(argv[1])+"/world/driver.vca").c_str(),120));
    auto end=driver.meshAt(Animation::Walk,39);
    std::vector<RevcTriangle> finalEntry(end,end+driver.count());
    assert(!std::memcmp(finalEntry.data(),driver.poseMesh(Animation::Walk,1,Animation::Walk,1,0,false),driver.count()*sizeof(RevcTriangle)));
    SentinelHandling handling;int gear=1;
    // Independent source first-gear calculation including 0x2 engine boost.
    assert(std::fabs(handling.acceleration(0,1,gear)-24.f*.4f*.5f*1.6f/3600)<1e-8f);
    float v=0;int highest=1;
    for(int i=0;i<60*60;++i) {
        v=handling.drag(v+handling.acceleration(v,1,gear));
        highest=std::max(highest,gear);assert(std::isfinite(v) && v>=0 && v<handling.maximum*50/60+.01f);
    }
    assert(highest==5 && v>.5f);
    const float forward=v;gear=0;v=0;
    for(int i=0;i<600;++i)v=handling.drag(v+handling.acceleration(v,-1,gear));
    assert(gear==0 && v<-.12f && v>=-.17f && std::fabs(v)<forward);
    assert(handling.drag(forward)<forward);
    std::printf("Rigid wheels/doors, unchanged neutral meshes, five source gear ratios, boost, drag and reverse passed; cruise %.1f km/h\n",forward*216);
}
