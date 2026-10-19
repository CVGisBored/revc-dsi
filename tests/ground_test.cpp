#include "world.h"
#include "street.h"
#include "packed.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <vector>
#include <string>
#include <algorithm>
struct Index {int16_t x,y,z,pad;uint32_t offset,count;};
static slice::World world;static slice::Game game;
int main(int argc,char** argv) {
    assert(argc==2);std::string root=argv[1];
    FILE* index=std::fopen((root+"/world/terrain.vct").c_str(),"rb");assert(index);
    char magic[4];uint32_t count,compressedMax,rawMax;
    assert(std::fread(magic,1,4,index)==4 && !std::memcmp(magic,"VTR3",4));
    assert(std::fread(&count,4,1,index)==1 && std::fread(&compressedMax,4,1,index)==1 && std::fread(&rawMax,4,1,index)==1);
    std::vector<Index> blocks(count);assert(std::fread(blocks.data(),sizeof(Index),count,index)==count);std::fclose(index);
    FILE* source=std::fopen((root+"/world/terrain.bin").c_str(),"rb");assert(source);
    FILE* originalIndex=std::fopen((root+"/world/terrain-raw.vct").c_str(),"rb");assert(originalIndex);
    assert(std::fread(magic,1,4,originalIndex)==4 && !std::memcmp(magic,"VTR2",4));uint32_t originalCount;
    assert(std::fread(&originalCount,4,1,originalIndex)==1 && originalCount==count);
    std::vector<Index> originals(count);assert(std::fread(originals.data(),sizeof(Index),count,originalIndex)==count);std::fclose(originalIndex);
    FILE* original=std::fopen((root+"/world/terrain-raw.bin").c_str(),"rb");assert(original);
    std::vector<unsigned char> compressed(compressedMax),decoded(rawMax),reference(rawMax);uint64_t triangles=0;
    for(unsigned b=0;b<count;++b) {
        assert(blocks[b].x==originals[b].x && blocks[b].y==originals[b].y && blocks[b].z==originals[b].z);
        assert(!std::fseek(source,blocks[b].offset,SEEK_SET) && !std::fseek(original,originals[b].offset,SEEK_SET));
        uint32_t sizes[512];uint16_t faces[512];assert(std::fread(sizes,sizeof(sizes),1,source)==1 && std::fread(faces,sizeof(faces),1,original)==1);
        uint32_t bytes=2048;
        for(int tile=0;tile<512;++tile) {
            assert(sizes[tile]<=compressedMax && unsigned(faces[tile])*34<=rawMax);bytes+=sizes[tile];
            if(!faces[tile]){assert(!sizes[tile]);continue;}
            assert(std::fread(compressed.data(),1,sizes[tile],source)==sizes[tile]);
            size_t result=slice::unpackTerrain(compressed.data(),sizes[tile],decoded.data(),rawMax);
            assert(result==unsigned(faces[tile])*34 && std::fread(reference.data(),1,result,original)==result);
            assert(!std::memcmp(reference.data(),decoded.data(),result));triangles+=faces[tile];
        }
        assert(bytes==blocks[b].count);
    }
    std::fclose(source);std::fclose(original);
    unsigned char invalid[]={0x10,34,0,0,128,0,0},out[34];assert(!slice::unpackTerrain(invalid,sizeof(invalid),out,sizeof(out)));
    assert(!slice::unpackTerrain(invalid,3,out,sizeof(out)));
    assert(world.open(root.c_str()) && world.originalCollision());slice::Point shift,seed={12.95f,10.03125f,-.39f};
    assert(world.update(seed,shift,true));seed={seed.x-shift.x,seed.y-shift.y,seed.z-shift.z};
    assert(game.startExploration(world.triangles(),world.count(),seed));slice::Street job;assert(job.start(game));
    auto foot=game.player;int drawn=world.visible({foot.x,foot.y+2.3f,foot.z+2.8f},0,1,.427f,.904f,&foot);bool pavement=false;
    for(int i=0;i<drawn;++i) {
        const auto& t=world.visibleTriangle(i);float x[3],y[3],z[3];
        for(int v=0;v<3;++v){x[v]=t.vertex[v][0]/64.f;y[v]=t.vertex[v][1]/64.f;z[v]=t.vertex[v][2]/64.f;}
        float det=(z[1]-z[2])*(x[0]-x[2])+(x[2]-x[1])*(z[0]-z[2]);if(std::fabs(det)<1e-6f)continue;
        float u=((z[1]-z[2])*(foot.x-x[2])+(x[2]-x[1])*(foot.z-z[2]))/det;
        float v=((z[2]-z[0])*(foot.x-x[2])+(x[0]-x[2])*(foot.z-z[2]))/det;
        if(u<0 || v<0 || u+v>1)continue;float height=u*y[0]+v*y[1]+(1-u-v)*y[2];
        if(height>=foot.y-.05f && height<=foot.y+1.25f){assert(height<=foot.y+.05f);pavement=true;}
    }
    assert(pavement);int steps=0;
    for(int i=0;i<100;++i) {
        steps+=game.move(0,-.15f);assert(world.update(game.player,shift));game.rebase(shift);game.bindGeometry(world.triangles(),world.count());
        auto supported=game.player;assert(game.placeActor(supported,.03f) && std::fabs(supported.y-game.player.y)<.03f);
    }
    assert(steps>=16);
    std::printf("Terrain: %llu original faces decompress identically; pavement is at the player's feet; %d supported movement steps\n",
        static_cast<unsigned long long>(triangles),steps);
}
