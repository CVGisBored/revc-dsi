#include "view.h"
#include "projection.h"
#include "distant.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>
#include <unistd.h>
using namespace slice;
static RevcTriangle triangle(int z){RevcTriangle t{};int16_t v[3][3]={{-32,-32,int16_t(z)},{32,-32,int16_t(z)},{0,32,int16_t(z)}};std::memcpy(t.vertex,v,sizeof(v));return t;}
static std::vector<unsigned char> pack(const std::vector<unsigned char>& raw) {
    std::vector<unsigned char> bytes={0x10,uint8_t(raw.size()),uint8_t(raw.size()>>8),uint8_t(raw.size()>>16)};
    for(unsigned i=0;i<raw.size();i+=8){bytes.push_back(0);bytes.insert(bytes.end(),raw.begin()+i,raw.begin()+std::min<unsigned>(i+8,raw.size()));}return bytes;
}
static void append(std::vector<unsigned char>& target,const void* data,unsigned size){auto p=static_cast<const unsigned char*>(data);target.insert(target.end(),p,p+size);}
int main() {
    for(float metres:{.25f,1.f,5.f,16.f,41.6f,64.f,80.f})assert(std::fabs(Projection::distance(Projection::depth(metres))-metres)<.004f);
    ViewFrustum view({0,0,0},0,1,0,1);assert(view.triangle(triangle(-320)));assert(!view.triangle(triangle(320)));
    auto far=triangle(-70*64);far.vertex[0][2]=-90*64;assert(view.triangle(far));
    auto near=triangle(-320);near.vertex[0][2]=64;assert(view.triangle(near));
    auto upper=triangle(-8*64);for(auto& v:upper.vertex)v[1]+=5*64;
    assert(ViewFrustum({0,0,0},0,1,-.5f,.8660254f).triangle(upper));
    assert(!ViewFrustum({0,0,0},0,1,.5f,.8660254f).triangle(upper));
    assert(cameraVerticalInput(true,false)<0 && cameraVerticalInput(false,true)>0 && cameraVerticalInput(true,true)==0);
    auto pulled=cameraOutsideVehicle({0,1,0},{0,1,-6},{0,0,-5},0,1);
    assert(pulled.z>-2 && pulled.z<0 && std::fabs(pulled.x)<.0001f);
    auto clear=cameraOutsideVehicle({0,1,0},{0,1,-6},{8,0,-5},0,1);assert(clear.z==-6);
    auto rotated=cameraOutsideVehicle({0,1,0},{-6,1,0},{-5,0,0},1,0);assert(rotated.x>-2 && rotated.x<0);
    // Exercise the actual complete-object loader within 16m of the player.
    struct Index{int16_t anchor[3],low[3],high[3];uint16_t faces;uint32_t offset,size;};static_assert(sizeof(Index)==28,"index");
    Index entries[2]{};std::vector<unsigned char> blobs[2];unsigned maxPacked=0,maxRaw=0,offset=16+sizeof(entries);
    for(int object=0;object<2;++object) {
        std::vector<unsigned char> raw;int faces=object ? 2 : 4;
        for(int i=0;i<faces;++i){auto tri=triangle(-320-object*128);uint16_t texture=0;int16_t uv[6]={};append(raw,&tri.color,2);append(raw,&texture,2);append(raw,tri.vertex,18);append(raw,uv,12);}
        blobs[object]=pack(raw);maxPacked=std::max(maxPacked,unsigned(blobs[object].size()));maxRaw=std::max(maxRaw,unsigned(raw.size()));
        auto& e=entries[object];e.low[0]=e.low[1]=-32;e.high[0]=e.high[1]=32;e.low[2]=e.high[2]=-320-object*128;e.faces=faces;e.offset=offset;e.size=blobs[object].size();offset+=e.size;
    }
    char path[]="/tmp/revc-renderer-XXXXXX";int fd=mkstemp(path);assert(fd>=0);FILE* file=fdopen(fd,"wb");assert(file);
    unsigned count=2;assert(std::fwrite("VFO1",1,4,file)==4);std::fwrite(&count,4,1,file);std::fwrite(&maxPacked,4,1,file);std::fwrite(&maxRaw,4,1,file);std::fwrite(entries,sizeof(entries),1,file);
    for(const auto& blob:blobs)std::fwrite(blob.data(),1,blob.size(),file);std::fclose(file);
    DistantWorld world;assert(world.open(path) && world.update({0,0,0},{0,0,0}));
    assert(world.visible({0,0,0},0,1,0,1,{0,0,0},3)==2);assert(world.visibleObjectCount()==1 && world.visibleObjectTriangles(0)==2);
    assert(world.visible({0,0,0},0,1,0,1,{0,0,0},4)==4);assert(world.visibleObjectCount()==1 && world.visibleObjectTriangles(0)==4);
    assert(world.visible({0,0,0},0,1,0,1,{0,0,0},6)==6);assert(world.visibleObjectCount()==2);
    assert(world.visible({0,0,0},0,1,0,1,{0,0,0},6)==6);assert(world.visible({0,0,0},0,1,0,1,{0,0,0},0)==0);
    assert(world.visible({0,0,0},0,1,0,1,{0,0,0},6)==6);
    // A changed selection containing the same objects must perform no ROM IO.
    unsigned reused=0,reads=0;assert(world.update({9,0,0},{0,0,0},true));reused+=world.reusedObjects();reads+=world.readBytes();
    while(world.streaming()){assert(world.update({9,0,0},{0,0,0},true));reused+=world.reusedObjects();reads+=world.readBytes();}
    assert(reused==2 && reads==0);
    assert(world.update({0,0,0},{64,0,0},true,false) && world.readBytes()==0);
    assert(world.visible({-64,0,0},0,1,0,1,{-64,0,0},6)==6);std::remove(path);
    std::puts("Renderer: whole objects, vehicle camera collision, close coverage, clipping intersections, pitched visibility, camera input and cached budgets passed");
}
