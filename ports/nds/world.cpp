#include "world.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <new>
#include "packed.h"
#include "view.h"
#ifdef ARM9
#include <nds.h>
#endif

namespace slice {
namespace {
bool filePath(char* path,size_t size,const char* root,const char* leaf) {
    int result=std::snprintf(path,size,"%s/world/%s",root,leaf);
    return result>0 && static_cast<size_t>(result)<size;
}
float distance2(Point a,Point b) {
    return (a.x-b.x)*(a.x-b.x)+(a.y-b.y)*(a.y-b.y)+(a.z-b.z)*(a.z-b.z);
}
}

bool World::open(const char* root) {
    static_assert(sizeof(Index)==16,"World index ABI");
    static_assert(sizeof(RevcTriangle)==20,"Triangle ABI");
    close();openStage=1; char path[512];
    if(!root || !filePath(path,sizeof(path),root,"index.vct")) return false;
    FILE* file=std::fopen(path,"rb");
    rich=file!=nullptr;
    if(!file) {
        if(!filePath(path,sizeof(path),root,"index.vcf"))return false;
        file=std::fopen(path,"rb");if(!file)return false;
    }
    char magic[4]; uint32_t count,cell;float center[2];
    openStage=11;bool valid=std::fread(magic,1,4,file)==4 && (rich ? (!std::memcmp(magic,"VCF3",4) || !std::memcmp(magic,"VCF4",4)) : !std::memcmp(magic,"VCF2",4));
    if(valid){openStage=12;valid=std::fread(&count,4,1,file)==1 && count>0 && count<=MaxChunks;}
    if(valid){openStage=13;valid=std::fread(center,4,2,file)==2 && center[0]==230.f && center[1]==-1250.f;}
    if(valid){openStage=14;valid=std::fread(&cell,4,1,file)==1 && cell==64;}
    if(valid){openStage=15;index.reset(new(std::nothrow) Index[count]);valid=index && std::fread(index.get(),sizeof(Index),count,file)==count;}
    if(valid){openStage=16;valid=std::fgetc(file)==EOF;}
    std::fclose(file);if(!valid)return false;
    openStage=2;
    indexCount=count;
    if(!filePath(path,sizeof(path),root,rich ? "textured.bin" : "fast.bin")) {close();return false;}
    chunks=std::fopen(path,"rb"); if(!chunks) {close();return false;}
    if(std::fseek(chunks,0,SEEK_END)!=0) {close();return false;}
    const long length=std::ftell(chunks);
    uint64_t end=0;
    for(int i=0;i<indexCount;++i) {
        const auto& entry=index[i];
        if(entry.pad || entry.count<512 || entry.offset!=end) {close();return false;}
        end=static_cast<uint64_t>(entry.offset)+entry.count;
        if(length<0 || end>static_cast<uint64_t>(length)) {close();return false;}
        if(i>0 && !(index[i-1].x<entry.x ||
            (index[i-1].x==entry.x && index[i-1].y<entry.y) ||
            (index[i-1].x==entry.x && index[i-1].y==entry.y && index[i-1].z<entry.z))) {
            close();return false;
        }
    }
    if(end!=static_cast<uint64_t>(length)) {close();return false;}
    openStage=3;
    if(!filePath(path,sizeof(path),root,"player.vcp")) {close();return false;}
    file=std::fopen(path,"rb"); if(!file) {close();return false;}
    valid=std::fread(magic,1,4,file)==4 && !std::memcmp(magic,"VCP1",4) &&
        std::fread(&count,4,1,file)==1 && count>0 && count<=550 &&
        std::fread(ped,sizeof(RevcTriangle),count,file)==count && std::fgetc(file)==EOF;
    std::fclose(file);
    if(!valid) {close();return false;}
    pedCount=count;
    auto load=[&](const char* name,RevcTriangle* target,uint32_t maximum,int& output) {
        if(!filePath(path,sizeof(path),root,name)) return false;
        FILE* model=std::fopen(path,"rb");if(!model) return false;
        bool good=std::fread(magic,1,4,model)==4 && !std::memcmp(magic,"VCM1",4) &&
            std::fread(&count,4,1,model)==1 && count>0 && count<=maximum &&
            std::fread(target,sizeof(RevcTriangle),count,model)==count && std::fgetc(model)==EOF;
        std::fclose(model);if(good) output=count;return good;
    };
    openStage=4;
    if(!load("vehicle-lod.vcm",vehicleDistant,160,vehicleDistantCount) || !load("vehicle.vcm",vehicle,600,vehicleCountValue) || !load("guard.vcm",guard,150,guardCountValue) ||
       !load("pistol.vcm",weapon,60,weaponCountValue)) {close();return false;}
    openStage=5;
    filePath(path,sizeof(path),root,"vehicle.vcu");file=std::fopen(path,"rb");if(!file){close();return false;}
    valid=std::fread(magic,1,4,file)==4 && !std::memcmp(magic,"VCU1",4) && std::fread(&count,4,1,file)==1 && count==unsigned(vehicleCountValue) &&
        std::fread(vehicleCoordinates,sizeof(RevcUV),count,file)==count && std::fgetc(file)==EOF;
    std::fclose(file);if(!valid){close();return false;}
    openStage=6;
    filePath(path,sizeof(path),root,"backdrops.vcb");file=std::fopen(path,"rb");if(!file){close();return false;}
    valid=std::fread(magic,1,4,file)==4 && !std::memcmp(magic,"VBD2",4) && std::fread(&count,4,1,file)==1 && count<=32767 &&
        std::fread(&excludedGeometryCount,4,1,file)==1 && excludedGeometryCount<=65536;
    for(unsigned i=0;valid && i<count;++i){uint16_t id=0;valid=std::fread(&id,2,1,file)==1 && id>0 && id<32768;if(valid)excludedTextures[id/8]|=1u<<(id%8);}
    if(valid && excludedGeometryCount)excludedGeometry.reset(new(std::nothrow) uint32_t[excludedGeometryCount]);
    valid=valid && (!excludedGeometryCount || (excludedGeometry && std::fread(excludedGeometry.get(),4,excludedGeometryCount,file)==excludedGeometryCount));
    valid=valid && std::fgetc(file)==EOF;std::fclose(file);if(!valid){close();return false;}
    for(unsigned i=1;i<excludedGeometryCount;++i)if(excludedGeometry[i]<=excludedGeometry[i-1]){close();return false;}
    openStage=7;
    if(!filePath(path,sizeof(path),root,"collision.vcc")) {close();return false;}
    file=std::fopen(path,"rb");
    if(file) {
        valid=std::fread(magic,1,4,file)==4 && (!std::memcmp(magic,"VCC1",4) || !std::memcmp(magic,"VCC2",4)) &&
            std::fread(&count,4,1,file)==1 && count>0 && count<=MaxCollisionChunks;
        const bool blocked=valid && magic[3]=='2';
        if(valid)collisionIndex.reset(new(std::nothrow) Index[count]);
        valid=valid && collisionIndex && std::fread(collisionIndex.get(),sizeof(Index),count,file)==count && std::fgetc(file)==EOF;
        std::fclose(file);if(!valid) {close();return false;}collisionIndexCount=count;
        filePath(path,sizeof(path),root,"collision.bin");collisionFile=std::fopen(path,"rb");
        if(!collisionFile || std::fseek(collisionFile,0,SEEK_END)) {close();return false;}
        const long bytes=std::ftell(collisionFile);uint64_t position=0;
        for(int i=0;i<collisionIndexCount;++i) {
            const auto& entry=collisionIndex[i];
            if(entry.pad || entry.offset!=position || !entry.count) {close();return false;}
            position+=entry.count*sizeof(RevcTriangle);
            if(i && !(collisionIndex[i-1].x<entry.x ||
                (collisionIndex[i-1].x==entry.x && collisionIndex[i-1].y<entry.y) ||
                (collisionIndex[i-1].x==entry.x && collisionIndex[i-1].y==entry.y && collisionIndex[i-1].z<entry.z))) {close();return false;}
        }
        if(bytes<0 || position!=static_cast<uint64_t>(bytes)) {close();return false;}
        if(blocked) {
            collisionBlockStarts.reset(new(std::nothrow) uint32_t[count+1]);
            if(!collisionBlockStarts){close();return false;}
            collisionBlockStarts[0]=0;
            for(unsigned i=0;i<count;++i)collisionBlockStarts[i+1]=collisionBlockStarts[i]+(collisionIndex[i].count+127)/128;
            uint32_t blocks=0,blockSize=0;
            filePath(path,sizeof(path),root,"collision-bounds.vcb");file=std::fopen(path,"rb");
            if(!file){close();return false;}
            valid=std::fread(magic,1,4,file)==4 && !std::memcmp(magic,"VCB2",4) &&
                std::fread(&blocks,4,1,file)==1 && blocks==collisionBlockStarts[count] &&
                std::fread(&blockSize,4,1,file)==1 && blockSize==128;
            if(valid)collisionBounds.reset(new(std::nothrow) CollisionBounds[blocks]);
            valid=valid && collisionBounds && std::fread(collisionBounds.get(),sizeof(CollisionBounds),blocks,file)==blocks && std::fgetc(file)==EOF;
            std::fclose(file);
            if(valid)for(unsigned i=0;i<blocks;++i)for(int k=0;k<3;++k)
                if(collisionBounds[i].low[k]>collisionBounds[i].high[k] || collisionBounds[i].high[k]>64)valid=false;
            if(!valid){close();return false;}
        }
    }else {close();return false;}
    filePath(path,sizeof(path),root,"terrain.vct");file=std::fopen(path,"rb");if(!file){close();return false;}
    valid=std::fread(magic,1,4,file)==4 && !std::memcmp(magic,"VTR3",4) && std::fread(&count,4,1,file)==1 && count>0 && count<=MaxChunks &&
        std::fread(&packedTerrainSize,4,1,file)==1 && std::fread(&rawTerrainSize,4,1,file)==1 &&
        packedTerrainSize>0 && packedTerrainSize<=300000 && rawTerrainSize>0 && rawTerrainSize<=262144;
    if(valid)terrainIndex.reset(new(std::nothrow) Index[count]);
    valid=valid && terrainIndex && std::fread(terrainIndex.get(),sizeof(Index),count,file)==count && std::fgetc(file)==EOF;
    std::fclose(file);if(!valid){close();return false;}terrainIndexCount=count;
    packedTerrain.reset(new(std::nothrow) unsigned char[packedTerrainSize]);rawTerrain.reset(new(std::nothrow) unsigned char[rawTerrainSize]);
    if(!packedTerrain || !rawTerrain){close();return false;}
    filePath(path,sizeof(path),root,"terrain.bin");terrainFile=std::fopen(path,"rb");if(!terrainFile){close();return false;}
    return true;
}

void World::close() {
    independentCollision=false;collisionReadBytes=0;collisionUrgent=false;urgentRefreshes=0;collisionBlocksSkipped=terrainHits=collisionHits=0;fallbackVersion=~0u;collisionWork=CollisionWork{};
    extendedCache.reset();
    collisionBounds.reset();collisionBlockStarts.reset();
    if(chunks) std::fclose(chunks);
    if(collisionFile)std::fclose(collisionFile);
    if(terrainFile)std::fclose(terrainFile);
    terrainFile=nullptr;terrainIndexCount=0;index.reset();collisionIndex.reset();terrainIndex.reset();
    packedTerrain.reset();rawTerrain.reset();packedTerrainSize=rawTerrainSize=0;
    std::memset(excludedTextures,0,sizeof(excludedTextures));
    excludedGeometry.reset();excludedGeometryCount=0;
    collisionFile=nullptr;collisionIndexCount=0;
    viewVersion=~0u;chunks=nullptr;loaded=rich=false;residentCount=collisionCount=indexCount=pedCount=droppedCount=0;origin={};
    vehicleCountValue=guardCountValue=weaponCountValue=0;
    for(auto& tile:cache) tile.age=0;
    for(auto& tile:terrainCache)tile.age=0;
    cacheClock=0;stream=Stream{};pendingCollision.reset();
    for(auto& patch:patches)patch.age=0;
    patchClock=patchPublishes=sceneryPublishes=patchReuses=unitLimit=0;patchesDirty=false;
    groundSuspended=sceneryStarted=sceneryDone=false;
}

bool World::update(Point player,Point& shift,bool force,bool incremental,bool driving) {
    shift={};readBytes=streamUnits=0;
    if(!chunks || !std::isfinite(player.x) || !std::isfinite(player.y) || !std::isfinite(player.z))return false;
    const Point global={player.x+origin.x,player.y+origin.y,player.z+origin.z};
    if(std::fabs(global.x)>100000 || std::fabs(global.y)>100000 || std::fabs(global.z)>100000)return false;
    if(force)stream.active=false;
    // Completed tiles remain useful even if a turn makes the rest of this
    // scan obsolete. Retarget without throwing those published tiles away.
    if(stream.active && stream.patches && !groundSuspended && distance2(global,stream.global)>64)stream.active=false;
    if(!stream.active) {
        const Point next={std::floor(global.x/64)*64,std::floor(global.y/64)*64,std::floor(global.z/64)*64};
        if(!force && loaded && distance2(global,lastLoad)<9 && distance2(next,origin)==0)return true;
        stream=Stream{};stream.active=true;stream.global=global;stream.anchor=next;
        groundSuspended=sceneryStarted=sceneryDone=false;
        stream.local={global.x-next.x,global.y-next.y,global.z-next.z};
        stream.patches=incremental && loaded && independentCollision && !force;
        stream.needCollision=!independentCollision && (force || !loaded || distance2(next,origin)>0 || distance2(global,lastCollisionLoad)>=36);
        beginPhase(stream.patches ? 1 : 0);
    }
    // Boot and explicit relocations drain synchronously; normal play retains the
    // previous snapshot while bounded pieces of the next one are prepared.
    const bool bounded=incremental && loaded && !force;
#ifdef ARM9
    const uint64_t started=tickGetCount();
#endif
    while(stream.active) {
        if(!streamStep(driving ? 8192 : 2048)){stream.active=false;return false;}
        ++streamUnits;
        if(stream.phase==3) {
            if(stream.patches){lastLoad=stream.global;stream.active=false;++version;}
            else {
            if(!stream.patches && (!stream.count || (stream.needCollision && !stream.collisions))){stream.active=false;return false;}
            if(!stream.patches) {
                for(int i=0;i<stream.count;++i){resident[i]=staging[i].triangle;material[i]=staging[i].material;}
                residentCount=stream.count;droppedCount=stream.total-stream.count;
                buildRenderTiles();
                for(auto& patch:patches)patch.age=0;
                for(int i=0;i<residentCount;++i){int cell[3];for(int k=0;k<3;++k)
                    cell[k]=int(std::floor((resident[i].vertex[0][k]+resident[i].vertex[1][k]+resident[i].vertex[2][k])/1536.f+(k==0 ? stream.anchor.x : k==1 ? stream.anchor.y : stream.anchor.z)/8));
                    residentSources[i]=patchKey(cell[0],cell[1],cell[2],walkableFace[i]);}
            }
            if(stream.needCollision) {
                collisionCount=stream.collisions;std::copy_n(pendingCollision.get(),collisionCount,collision);
                lastCollisionLoad=lastCollisionRequest=stream.global;++collisionVersion;pendingCollision.reset();
            }
            shift={stream.anchor.x-origin.x,stream.anchor.y-origin.y,stream.anchor.z-origin.z};
            if(independentCollision && (shift.x || shift.y || shift.z)) {
                for(int i=0;i<collisionCount;++i)for(auto& v:collision[i].vertex) {
                    v[0]-=int(shift.x*64);v[1]-=int(shift.y*64);v[2]-=int(shift.z*64);
                }
                ++collisionVersion;
            }
            origin=stream.anchor;lastLoad=stream.global;loaded=true;stream.active=false;++version;
            }
        }
        if(bounded) {
            if(streamUnits>=(unitLimit ? unitLimit : driving ? 16u : 8u) || readBytes>=(driving ? 16384u : 8192u))break;
#ifdef ARM9
            if(tickGetCount()-started>=TICK_FREQ*(driving ? 4 : 3)/1000)break;
#endif
        }
    }
    if(independentCollision && incremental && !force && loaded){
        const Point next={std::floor(global.x/64)*64,std::floor(global.y/64)*64,std::floor(global.z/64)*64};
        shift={next.x-origin.x,next.y-origin.y,next.z-origin.z};
        if(shift.x || shift.y || shift.z){
            for(int i=0;i<residentCount;++i)for(auto& v:resident[i].vertex){v[0]-=int(shift.x*64);v[1]-=int(shift.y*64);v[2]-=int(shift.z*64);}
            for(int i=0;i<collisionCount;++i)for(auto& v:collision[i].vertex){v[0]-=int(shift.x*64);v[1]-=int(shift.y*64);v[2]-=int(shift.z*64);}
            origin=next;++collisionVersion;patchesDirty=true;
        }
    }
    if(patchesDirty){buildRenderTiles();patchesDirty=false;++version;}
    return true;
}

uint32_t World::patchKey(int x,int y,int z,bool ground) {
    return (uint32_t(x+512)&1023u) | ((uint32_t(y+512)&1023u)<<10) | ((uint32_t(z+512)&1023u)<<20) | (ground ? 0x80000000u : 0);
}
bool World::patchResident(uint32_t key) {
    for(auto& patch:patches)if(patch.age && patch.key==key){patch.age=++patchClock;++patchReuses;return true;}
    return false;
}
void World::publishPatch(Candidate* faces,int count,uint32_t key) {
    const bool ground=(key>>31)!=0;
    const int focus[3]={int(stream.global.x*64),int(stream.global.y*64),int(stream.global.z*64)};
    auto score=[&](uint32_t source){
        int64_t result=0;for(int k=0;k<3;++k){int low=(int((source>>(k*10))&1023)-512)*512;
            int p=focus[k];
            int d=p-std::max(low,std::min(low+512,p));result+=int64_t(d)*d*(k==1 ? 4 : 1);}
        return result;
    };
    auto erase=[&](uint32_t source){
        int out=0;for(int i=0;i<residentCount;++i)if(residentSources[i]!=source){if(out!=i){resident[out]=resident[i];material[out]=material[i];residentSources[out]=residentSources[i];}++out;}
        residentCount=out;for(auto& patch:patches)if(patch.age && patch.key==source)patch.age=0;
    };
    erase(key);
    // Old detail outside the near renderer cannot help coverage. Removing it
    // also keeps local fixed-point coordinates bounded across long drives.
    for(int i=0;i<residentCount;)if(score(residentSources[i])>int64_t(48*64)*(48*64))erase(residentSources[i]);else ++i;
    const int capacity=ground ? TerrainCapacity : DecorationCapacity;
    int used=0;for(int i=0;i<residentCount;++i)used+=bool(residentSources[i]>>31)==ground;
    while(used+count>capacity){
        uint32_t worst=0;int64_t distance=-1;
        for(int i=0;i<residentCount;++i)if(bool(residentSources[i]>>31)==ground){auto d=score(residentSources[i]);if(d>distance){distance=d;worst=residentSources[i];}}
        if(distance<0)break;
        if(score(key)>distance){patchesDirty=true;return;}
        int previous=residentCount;erase(worst);used-=previous-residentCount;
    }
    const int delta[3]={int((stream.anchor.x-origin.x)*64),int((stream.anchor.y-origin.y)*64),int((stream.anchor.z-origin.z)*64)};
    for(int i=0;i<count && residentCount<Capacity;++i){const int out=residentCount++;resident[out]=faces[i].triangle;material[out]=faces[i].material;residentSources[out]=key;
        for(auto& v:resident[out].vertex)for(int k=0;k<3;++k)v[k]+=delta[k];}
    auto* oldest=&patches[0];for(auto& patch:patches)if(patch.age<oldest->age)oldest=&patch;
    oldest->key=key;oldest->age=++patchClock;++patchPublishes;if(!ground)++sceneryPublishes;patchesDirty=true;
}

bool World::updateCollision(Point player,Point lookahead,unsigned readLimit) {
    collisionReadBytes=0;collisionUrgent=false;
    if(!loaded || !collisionFile || !std::isfinite(player.x+player.y+player.z+lookahead.x+lookahead.y+lookahead.z))return false;
    independentCollision=true;
    const Point actual={player.x+origin.x,player.y+origin.y,player.z+origin.z};
    auto& work=collisionWork;
    // The selected COL radius is 24 m. Reserve eight metres for the car's
    // footprint, movement catch-up and turns. A slow SD card may need many
    // frames to finish a snapshot; once this safety margin is reached, finish
    // collision now instead of treating missing road support as a solid wall.
    collisionUrgent=distance2(actual,lastCollisionLoad)>=16*16 || std::fabs(actual.y-lastCollisionLoad.y)>=5;
    if(work.active && (distance2(actual,work.global)>=16*16 || std::fabs(actual.y-work.global.y)>=5))collisionUrgent=true;
    // Turning back can make the pending centre obsolete. Never publish that
    // old neighbourhood as the only collision snapshot around the player.
    if(collisionUrgent && work.active && (distance2(actual,work.global)>12*12 || std::fabs(actual.y-work.global.y)>=5)){work=CollisionWork{};pendingCollision.reset();}
    if(!work.active) {
        if(!collisionUrgent && distance2(actual,lastCollisionRequest)<36)return true;
        Point lead={lookahead.x-player.x,0,lookahead.z-player.z};
        float length=std::sqrt(lead.x*lead.x+lead.z*lead.z);
        if(length>6){lead.x*=6/length;lead.z*=6/length;}
        work=CollisionWork{};work.active=true;
        work.requested=actual;
        work.global={actual.x+lead.x,actual.y,actual.z+lead.z};
        work.anchor={std::floor(work.global.x/64)*64,std::floor(work.global.y/64)*64,std::floor(work.global.z/64)*64};
        int column=int(work.anchor.x/64);
        work.entry=int(std::lower_bound(collisionIndex.get(),collisionIndex.get()+collisionIndexCount,column-1,[](const Index& e,int x){return e.x<x;})-collisionIndex.get());
        if(!pendingCollision)pendingCollision.reset(new(std::nothrow) RevcTriangle[CollisionCapacity]);
        if(!pendingCollision)return false;
    }
    const int anchor[3]={int(work.anchor.x*64),int(work.anchor.y*64),int(work.anchor.z*64)};
    const int actor[3]={int((work.global.x-work.anchor.x)*64),int((work.global.y-work.anchor.y+.8f)*64),int((work.global.z-work.anchor.z)*64)};
#ifdef ARM9
    const uint64_t started=tickGetCount();
#endif
    if(collisionUrgent)++urgentRefreshes;
    unsigned units=0;
    while(work.entry<collisionIndexCount && collisionIndex[work.entry].x<=int(work.anchor.x/64)+1) {
        const auto& entry=collisionIndex[work.entry];
        int offsets[3]={entry.x*4096-anchor[0],entry.y*4096-anchor[1],entry.z*4096-anchor[2]};
        bool nearby=true;
        for(int k=0;k<3;++k){int margin=k==1 ? 640 : 1536;if(actor[k]<offsets[k]-margin || actor[k]>offsets[k]+4096+margin)nearby=false;}
        if(!nearby){++work.entry;work.record=0;continue;}
        if(!collisionBlockNearby(work.entry,work.record,offsets,actor)) {
            ++collisionBlocksSkipped;work.record=std::min<unsigned>(entry.count,work.record+128);
            if(work.record==entry.count){++work.entry;work.record=0;}
            continue;
        }
        RevcTriangle buffer[128];unsigned n=std::min<unsigned>(128,entry.count-work.record);
        if(!readCollision(entry.offset+work.record*sizeof(RevcTriangle),n,buffer,collisionReadBytes))return false;
        work.record+=n;
        for(unsigned i=0;i<n;++i) {
            auto t=buffer[i];bool selected=true;unsigned score=0;
            for(int k=0;k<3;++k) {
                for(auto& v:t.vertex)v[k]+=offsets[k];
                int low=std::min({t.vertex[0][k],t.vertex[1][k],t.vertex[2][k]}),high=std::max({t.vertex[0][k],t.vertex[1][k],t.vertex[2][k]});
                int delta=actor[k]-std::max(low,std::min(high,actor[k]));
                if(k==1 && std::abs(delta)>640)selected=false;
                if(k!=1)score+=delta*delta;
            }
            if(!selected || score>1536*1536)continue;
            if(work.count==CollisionCapacity)return false;
            pendingCollision[work.count++]=t;
        }
        if(work.record==entry.count){++work.entry;work.record=0;}
        ++units;
        if(!collisionUrgent && (units>=std::max(1u,readLimit) || collisionReadBytes>=32768))return true;
#ifdef ARM9
        if(!collisionUrgent && tickGetCount()-started>=TICK_FREQ*4/1000)return true;
#endif
    }
    if(work.count) {
        int delta[3]={int((work.anchor.x-origin.x)*64),int((work.anchor.y-origin.y)*64),int((work.anchor.z-origin.z)*64)};
        for(int i=0;i<work.count;++i){collision[i]=pendingCollision[i];for(auto& v:collision[i].vertex)for(int k=0;k<3;++k)v[k]+=delta[k];}
        collisionCount=work.count;lastCollisionLoad=work.global;lastCollisionRequest=work.requested;++collisionVersion;
    }
    work.active=false;return true;
}

bool World::collisionBlockNearby(int entry,unsigned record,const int* offsets,const int* actor) const {
    if(!collisionBounds)return true;
    const auto& bounds=collisionBounds[collisionBlockStarts[entry]+record/128];
    unsigned distance=0;
    for(int k=0;k<3;++k){int d=actor[k]-std::max(int(bounds.low[k])*64+offsets[k],std::min(int(bounds.high[k])*64+offsets[k],actor[k]));
        if(k==1){if(std::abs(d)>640)return false;}else distance+=d*d;}
    return distance<=1536*1536;
}

bool World::enableExtendedCache() {
    if(!extendedCache)extendedCache.reset(new(std::nothrow) ReadCache[128]);
    return bool(extendedCache);
}
bool World::readExtended(FILE* file,unsigned kind,unsigned address,unsigned size,unsigned char* output,uint32_t& bytes) {
    ReadCache* oldest=nullptr;
    if(extendedCache && size<=8192)for(unsigned i=0;i<128;++i){auto& slot=extendedCache[i];
        if(slot.age && slot.kind==kind && slot.offset==address && slot.size==size){std::memcpy(output,slot.bytes,size);slot.age=++cacheClock;if(kind==1)++collisionHits;return true;}
        if(!oldest || slot.age<oldest->age)oldest=&slot;
    }
    if(std::fseek(file,address,SEEK_SET) || std::fread(output,1,size,file)!=size)return false;
    bytes+=size;
    if(oldest){oldest->kind=kind;oldest->offset=address;oldest->size=size;oldest->age=++cacheClock;std::memcpy(oldest->bytes,output,size);}
    return true;
}
bool World::readCollision(unsigned address,unsigned count,RevcTriangle* output,uint32_t& bytes) {
    const unsigned size=count*sizeof(RevcTriangle);
    if(extendedCache)return readExtended(collisionFile,1,address,size,reinterpret_cast<unsigned char*>(output),bytes);
    // Share the existing tile allocation on 4 MB DS launches. Reserving its
    // last 32 entries for source COL avoids a new heap allocation and repeated
    // SD reads of the same collision blocks every few metres.
    TileCache* oldest=&cache[64];const unsigned key=address|0x80000000u;
    for(unsigned i=64;i<96;++i){auto& slot=cache[i];
        if(slot.age && slot.offset==key){std::memcpy(output,slot.bytes,size);slot.age=++cacheClock;++collisionHits;return true;}
        if(slot.age<oldest->age)oldest=&slot;
    }
    if(!readExtended(collisionFile,1,address,size,reinterpret_cast<unsigned char*>(output),bytes))return false;
    oldest->offset=key;oldest->age=++cacheClock;std::memcpy(oldest->bytes,output,size);return true;
}
bool World::readDirectory(FILE* file,unsigned kind,unsigned address,unsigned size,void* output) {
    if(extendedCache)return readExtended(file,kind,address,size,static_cast<unsigned char*>(output),readBytes);
    const unsigned key=address|(kind==3 ? 0x40000000u : 0x80000000u);
    auto* oldest=&terrainCache[0];
    for(auto& cached:terrainCache){if(cached.age && cached.offset==key){std::memcpy(output,cached.bytes,size);cached.age=++cacheClock;return true;}
        if(cached.age<oldest->age)oldest=&cached;}
    if(!readExtended(file,kind,address,size,static_cast<unsigned char*>(output),readBytes))return false;
    oldest->offset=key;oldest->size=size;oldest->age=++cacheClock;std::memcpy(oldest->bytes,output,size);return true;
}

void World::beginPhase(int phase) {
    stream.phase=phase;stream.region=-1;stream.tile=0;stream.record=stream.decoded=stream.consumed=stream.packedRead=0;stream.heap=false;
    if(phase==3)return;
    const Index* list=phase==0 ? index.get() : phase==1 ? terrainIndex.get() : collisionIndex.get();
    const int n=phase==0 ? indexCount : phase==1 ? terrainIndexCount : collisionIndexCount;
    const int x=int(stream.anchor.x/64);
    stream.entry=int(std::lower_bound(list,list+n,x-1,[](const Index& e,int column){return e.x<column;})-list);
    if(phase==1)stream.base=stream.patches ? DecorationCapacity : stream.count;
    if(stream.patches) {
        stream.regionCount=stream.regionCursor=0;
        const int p[3]={int(stream.global.x*64),int(stream.global.y*64),int(stream.global.z*64)};
        auto score=[&](int entry)->uint32_t{uint32_t distance=0;const auto& e=list[entry];int lo[3]={e.x*4096,e.y*4096,e.z*4096};
            for(int k=0;k<3;++k){int d=p[k]-std::max(lo[k],std::min(lo[k]+4096,p[k]));if(std::abs(d)>1536)return ~0u;distance+=d*d*(k==1 ? 4 : 1);}return distance;};
        for(int i=stream.entry;i<n && list[i].x<=x+1;++i)if(score(i)<=uint32_t((phase==1 ? 512 : 1024)*(phase==1 ? 512 : 1024)) && stream.regionCount<64)stream.regions[stream.regionCount++]=i;
        std::sort(stream.regions,stream.regions+stream.regionCount,[&](unsigned a,unsigned b){auto x=score(a),y=score(b);return x==y ? a<b : x<y;});
        stream.entry=stream.regionCount ? stream.regions[0] : n;
    }
}

void World::nextRegion() {
    if(!stream.patches){++stream.entry;return;}
    stream.entry=++stream.regionCursor<stream.regionCount ? stream.regions[stream.regionCursor] : stream.phase==0 ? indexCount : terrainIndexCount;
}

bool World::streamStep(unsigned readSize) {
    const int phase=stream.phase;
    const Index* list=phase==0 ? index.get() : phase==1 ? terrainIndex.get() : collisionIndex.get();
    const int n=phase==0 ? indexCount : phase==1 ? terrainIndexCount : collisionIndexCount;
    const int x=int(stream.anchor.x/64),y=int(stream.anchor.y/64),z=int(stream.anchor.z/64);
    const int actor[3]={int(stream.local.x*64),int((stream.local.y+(phase==1 ? 0 : .8f))*64),int(stream.local.z*64)};
    auto compare=[](const Candidate& a,const Candidate& b){return a.score<b.score;};
    while(stream.entry<n && list[stream.entry].x<=x+1) {
        const auto& entry=list[stream.entry];
        const int offsets[3]={(entry.x-x)*4096,(entry.y-y)*4096,(entry.z-z)*4096};
        bool nearby=true;
        for(int k=0;k<3;++k) {
            const int margin=k==1 ? (phase==2 ? 640 : 2048) : (phase==1 ? 1024 : 1536);
            if(actor[k]<offsets[k]-margin || actor[k]>offsets[k]+4096+margin)nearby=false;
        }
        if(!nearby){nextRegion();continue;}
        FILE* file=phase==0 ? chunks : phase==1 ? terrainFile : collisionFile;
        if(phase==2) {
            if(!pendingCollision)pendingCollision.reset(new(std::nothrow) RevcTriangle[CollisionCapacity]);
            if(!pendingCollision)return false;
            if(!collisionBlockNearby(stream.entry,stream.consumed,offsets,actor)) {
                ++collisionBlocksSkipped;stream.consumed=std::min<unsigned>(entry.count,stream.consumed+128);
                if(stream.consumed==entry.count){++stream.entry;stream.consumed=0;}
                continue;
            }
            // Explicit offsets make interleaved file work safe and cap each read
            // at 2.5 KiB, instead of reading an entire 64 metre COL region.
            RevcTriangle buffer[128];
            unsigned amount=std::min<unsigned>(128,entry.count-stream.consumed);
            if(!readCollision(entry.offset+stream.consumed*sizeof(RevcTriangle),amount,buffer,readBytes))return false;
            stream.consumed+=amount;
            for(unsigned i=0;i<amount;++i) {
                auto t=buffer[i];bool selected=true;unsigned score=0;
                for(int k=0;k<3;++k) {
                    for(auto& v:t.vertex)v[k]+=offsets[k];
                    int low=std::min({t.vertex[0][k],t.vertex[1][k],t.vertex[2][k]});
                    int high=std::max({t.vertex[0][k],t.vertex[1][k],t.vertex[2][k]});
                    int delta=actor[k]-std::max(low,std::min(high,actor[k]));
                    if(k==1 && std::abs(delta)>640)selected=false;
                    if(k!=1)score+=delta*delta;
                }
                if(!selected || score>1536*1536)continue;
                if(stream.collisions==CollisionCapacity)return false;
                pendingCollision[stream.collisions++]=t;
            }
            if(stream.consumed==entry.count){++stream.entry;stream.consumed=0;}
            return true;
        }
        const int stride=rich ? 25 : 11;
        if(stream.region!=stream.entry) {
            const unsigned before=readBytes;
            if(phase==0) {
                unsigned char directory[512];
                if(!readDirectory(file,3,entry.offset,512,directory))return false;
                for(int i=0;i<512;++i){if(directory[i]>(rich ? 128 : 64))return false;stream.sizes[i]=directory[i];}
            }else {
                if(!readDirectory(file,4,entry.offset,sizeof(stream.sizes),stream.sizes))return false;
            }
            unsigned end=phase==0 ? 512 : 2048;
            for(int i=0;i<512;++i){stream.starts[i]=end;end+=stream.sizes[i]*(phase==0 ? stride : 1);}
            if(end!=entry.count)return false;
            stream.region=stream.entry;stream.tile=0;
            stream.tileCount=512;
            if(stream.patches) {
                stream.tileCount=0;
                auto score=[&](unsigned tile){int cells[3]={int(tile%8),int((tile/8)%8),int(tile/64)};unsigned distance=0;
                    for(int k=0;k<3;++k){int lo=offsets[k]+cells[k]*512,d=actor[k]-std::max(lo,std::min(lo+512,actor[k]));
                        if(k==1 && std::abs(d)>(phase==1 ? 128 : 1024))return ~0u;
                        distance+=d*d/(k==1 ? 16 : 1);}return distance;};
                const unsigned radius=phase==1 ? 512 : 1024;
                for(unsigned tile=0;tile<512;++tile)if(stream.sizes[tile] && score(tile)<=radius*radius)stream.tileOrder[stream.tileCount++]=tile;
                std::sort(stream.tileOrder,stream.tileOrder+stream.tileCount,[&](unsigned a,unsigned b){auto x=score(a),y=score(b);return x==y ? a<b : x<y;});
            }
            // Cached directories are bookkeeping, not a storage work piece.
            // Continue to the useful tile in this call instead of spending
            // half a slow frame's allowance re-opening the same region.
            if(!stream.patches || readBytes!=before)return true;
        }
        while(unsigned(stream.tile)<stream.tileCount) {
            const int tile=stream.patches ? stream.tileOrder[stream.tile] : stream.tile,cells[3]={tile%8,(tile/8)%8,tile/64};
            const uint32_t source=patchKey(entry.x*8+cells[0],entry.y*8+cells[1],entry.z*8+cells[2],phase==1);
            if(stream.patches && !stream.decoded && !stream.packedRead && patchResident(source)){++stream.tile;continue;}
            const unsigned size=stream.sizes[tile],address=entry.offset+stream.starts[tile];
            const int radius=phase==0 ? 1536 : 1024;
            unsigned boxDistance=0;bool selected=true;
            for(int k=0;k<3;++k) {
                int low=offsets[k]+cells[k]*512,high=low+512;
                int d=actor[k]-std::max(low,std::min(high,actor[k]));
                if(k==1 && std::abs(d)>2048)selected=false;
                boxDistance+=d*d/(k==1 ? 16 : 1);
            }
            if(!size || !selected || boxDistance>unsigned(radius*radius)){++stream.tile;continue;}
            if(phase==0) {
                TileCache* cached=nullptr;TileCache* oldest=&cache[0];
                for(unsigned i=0;i<(extendedCache ? 96u : 64u);++i){auto& slot=cache[i];if(slot.age && slot.offset==address){cached=&slot;break;}if(slot.age<oldest->age)oldest=&slot;}
                if(!cached) {
                    cached=oldest;
                    if(std::fseek(file,address,SEEK_SET) || std::fread(cached->bytes,stride,size,file)!=size)return false;
                    readBytes+=size*stride;cached->offset=address;
                }
                cached->age=++cacheClock;
                for(unsigned i=0;i<size;++i) {
                    const auto* record=cached->bytes+i*stride;Candidate c{};auto& t=c.triangle;auto& m=c.material;
                    std::memcpy(&t.color,record,2);
                    if(rich){uint16_t id;std::memcpy(&id,record+2,2);m.texture=id&0x7fff;std::memcpy(m.uv,record+13,12);}
                    if(excludedTextures[m.texture/8]&(1u<<(m.texture%8)))continue;
                    for(int k=0;k<3;++k) {
                        for(int v=0;v<3;++v){int value=record[(rich ? 4 : 2)+v*3+k];if(value>128)return false;t.vertex[v][k]=cells[k]*512+value*4+offsets[k];}
                        int low=std::min({t.vertex[0][k],t.vertex[1][k],t.vertex[2][k]}),high=std::max({t.vertex[0][k],t.vertex[1][k],t.vertex[2][k]});
                        int d=actor[k]-std::max(low,std::min(high,actor[k]));c.score+=d*d/(k==1 ? 16 : 1);
                    }
                    if(c.score>1536*1536 || (!m.texture && backdrop(t,x,y,z)))continue;
                    int ux=t.vertex[1][0]-t.vertex[0][0],uy=t.vertex[1][1]-t.vertex[0][1],uz=t.vertex[1][2]-t.vertex[0][2];
                    int vx=t.vertex[2][0]-t.vertex[0][0],vy=t.vertex[2][1]-t.vertex[0][1],vz=t.vertex[2][2]-t.vertex[0][2];
                    int64_t nx=int64_t(uy)*vz-int64_t(uz)*vy,ny=int64_t(uz)*vx-int64_t(ux)*vz,nz=int64_t(ux)*vy-int64_t(uy)*vx;
                    if(ny && ny*ny>=nx*nx+nz*nz)continue;
                    ++stream.total;
                    if(stream.count<DecorationCapacity)staging[stream.count++]=c;
                    else {
                        if(!stream.heap){std::make_heap(staging,staging+stream.count,compare);stream.heap=true;}
                        if(c.score<staging[0].score){std::pop_heap(staging,staging+stream.count,compare);staging[stream.count-1]=c;std::push_heap(staging,staging+stream.count,compare);}
                    }
                }
                if(stream.patches){publishPatch(staging,stream.count,source);stream.count=0;}
                ++stream.tile;
                if(groundSuspended){sceneryWork=stream;stream=groundWork;groundSuspended=false;stream.groundSlice=0;}
                return true;
            }
            if(!stream.decoded) {
                if(size>packedTerrainSize)return false;
                if(!stream.packedRead)for(auto& cached:terrainCache)if(cached.age && cached.offset==address){
                    std::memcpy(rawTerrain.get(),cached.bytes,cached.size);stream.decoded=cached.size;stream.record=0;
                    cached.age=++cacheClock;++terrainHits;if(!stream.patches)return true;break;
                }
                if(!stream.decoded) {
                const unsigned amount=extendedCache && size<=8192 ? size : std::min(readSize,size-stream.packedRead);
                if(extendedCache && size<=8192) {
                    if(!readExtended(file,2,address,size,packedTerrain.get(),readBytes))return false;
                    stream.packedRead=size;
                }else {
                if(size>packedTerrainSize || std::fseek(file,address+stream.packedRead,SEEK_SET) ||
                   std::fread(packedTerrain.get()+stream.packedRead,1,amount,file)!=amount)return false;
                readBytes+=amount;stream.packedRead+=amount;
                }
                if(stream.packedRead<size)return true;
                stream.packedRead=0;
                stream.decoded=unpackTerrain(packedTerrain.get(),size,rawTerrain.get(),rawTerrainSize);
                if(!stream.decoded || stream.decoded%34)return false;
                const int floorLow=offsets[1]+cells[1]*512;
                const int floorDelta=actor[1]-std::max(floorLow,std::min(floorLow+512,actor[1]));
                // Protect the small working set directly beneath/ahead of
                // the player; caching every distant roof would evict it on
                // each scan before it could ever be reused.
                if(stream.decoded<=sizeof(terrainCache[0].bytes) && boxDistance<=256*256 && std::abs(floorDelta)<=128) {
                    auto* oldest=&terrainCache[0];for(auto& cached:terrainCache)if(cached.age<oldest->age)oldest=&cached;
                    oldest->offset=address;oldest->size=stream.decoded;oldest->age=++cacheClock;
                    std::memcpy(oldest->bytes,rawTerrain.get(),stream.decoded);
                }
                stream.record=0;if(!stream.patches)return true;
                }
            }
            const unsigned end=std::min(stream.decoded/34,stream.record+128);
            for(;stream.record<end;++stream.record) {
                const auto* record=rawTerrain.get()+stream.record*34;Candidate c{};auto& t=c.triangle;auto& m=c.material;
                std::memcpy(&t.color,record,2);std::memcpy(&m.texture,record+2,2);
                if(m.texture>=32768)return false;
                if(excludedTextures[m.texture/8]&(1u<<(m.texture%8)))continue;
                std::memcpy(t.vertex,record+4,18);std::memcpy(m.uv,record+22,12);
                for(int k=0;k<3;++k) {
                    for(auto& v:t.vertex){if(v[k]<0 || v[k]>512)return false;v[k]+=offsets[k]+cells[k]*512;}
                    int low=std::min({t.vertex[0][k],t.vertex[1][k],t.vertex[2][k]}),high=std::max({t.vertex[0][k],t.vertex[1][k],t.vertex[2][k]});
                    int d=actor[k]-std::max(low,std::min(high,actor[k]));c.score+=d*d/(k==1 ? 16 : 1);
                }
                if(c.score>1024*1024 || (!m.texture && backdrop(t,x,y,z)))continue;
                ++stream.total;
                auto begin=staging+stream.base,end=begin+stream.ground;
                if(stream.ground<TerrainCapacity)staging[stream.base+stream.ground++]=c;
                else {
                    if(!stream.heap){std::make_heap(begin,end,compare);stream.heap=true;}
                    if(c.score<begin->score){std::pop_heap(begin,end,compare);end[-1]=c;std::push_heap(begin,end,compare);}
                }
            }
            if(stream.record==stream.decoded/34){
                if(stream.patches){publishPatch(staging+stream.base,stream.ground,source);stream.ground=0;stream.heap=false;}
                stream.decoded=0;++stream.tile;
                if(stream.patches && !sceneryDone && ++stream.groundSlice>=2){
                    groundWork=stream;groundSuspended=true;
                    if(sceneryStarted)stream=sceneryWork;else {beginPhase(0);sceneryStarted=true;}
                }
            }
            return true;
        }
        nextRegion();stream.region=-1;
    }
    if(stream.patches){
        if(phase==0 && groundSuspended){sceneryDone=true;stream=groundWork;groundSuspended=false;stream.groundSlice=0;}
        else if(phase==1){if(sceneryDone)beginPhase(3);else if(sceneryStarted)stream=sceneryWork;else beginPhase(0);}
        else beginPhase(3);
        return true;
    }
    if(phase==1)stream.count=stream.base+stream.ground;
    beginPhase(phase==1 && !stream.needCollision ? 3 : phase+1);
    return true;
}

float World::visualLag(Point local) const {
    if(patchPublishes) {
        // A partially completed scan can already contain the actual street.
        // Do not cover its textures with the emergency floor merely because
        // optional scenery in the rest of that scan is still pending.
        const int p[3]={int(local.x*64),int(local.y*64),int(local.z*64)};
        for(int i=0;i<residentCount;++i)if(walkableFace[i]){const auto& v=resident[i].vertex;
            if(p[0]<std::min({v[0][0],v[1][0],v[2][0]}) || p[0]>std::max({v[0][0],v[1][0],v[2][0]}) ||
               p[2]<std::min({v[0][2],v[1][2],v[2][2]}) || p[2]>std::max({v[0][2],v[1][2],v[2][2]}))continue;
            int64_t ax=v[1][0]-v[0][0],az=v[1][2]-v[0][2],bx=v[2][0]-v[0][0],bz=v[2][2]-v[0][2];
            int64_t dx=p[0]-v[0][0],dz=p[2]-v[0][2],det=ax*bz-az*bx,u=dx*bz-dz*bx,w=ax*dz-az*dx;
            if(det<0){det=-det;u=-u;w=-w;}
            if(!det || u<0 || w<0 || u+w>det)continue;
            int64_t y=int64_t(v[0][1])*det+u*(v[1][1]-v[0][1])+w*(v[2][1]-v[0][1]);
            if(std::abs(y-int64_t(p[1])*det)<=32*det)return 0;
        }
        return 16; // Actual near-ground miss: enable original-COL safety coverage.
    }
    return std::sqrt(distance2({local.x+origin.x,local.y+origin.y,local.z+origin.z},lastLoad));
}
int World::groundFallback(Point focus,int budget) {
    budget=std::max(0,std::min(128,budget));
    int key[4]={int(std::floor(focus.x/4)),int(std::floor(focus.y/2)),int(std::floor(focus.z/4)),budget};
    if(fallbackVersion==collisionVersion && !std::memcmp(key,fallbackKey,sizeof(key)))return fallbackCount;
    fallbackVersion=collisionVersion;std::memcpy(fallbackKey,key,sizeof(key));fallbackCount=0;
    struct Candidate {uint16_t face;uint32_t score;};Candidate best[128];
    auto compare=[](const Candidate& a,const Candidate& b){return a.score<b.score;};
    int p[3]={int(focus.x*64),int(focus.y*64),int(focus.z*64)};
    for(int i=0;i<collisionCount && budget;++i){const auto& t=collision[i];
        int lowY=std::min({t.vertex[0][1],t.vertex[1][1],t.vertex[2][1]}),highY=std::max({t.vertex[0][1],t.vertex[1][1],t.vertex[2][1]});if(lowY>p[1]+256 || highY<p[1]-256)continue;
        int ax=t.vertex[1][0]-t.vertex[0][0],ay=t.vertex[1][1]-t.vertex[0][1],az=t.vertex[1][2]-t.vertex[0][2],bx=t.vertex[2][0]-t.vertex[0][0],by=t.vertex[2][1]-t.vertex[0][1],bz=t.vertex[2][2]-t.vertex[0][2];
        int64_t nx=int64_t(ay)*bz-int64_t(az)*by,ny=int64_t(az)*bx-int64_t(ax)*bz,nz=int64_t(ax)*by-int64_t(ay)*bx;if(!ny || ny*ny<nx*nx+nz*nz)continue;
        uint32_t score=0;for(int k=0;k<3;++k){int low=std::min({t.vertex[0][k],t.vertex[1][k],t.vertex[2][k]}),high=std::max({t.vertex[0][k],t.vertex[1][k],t.vertex[2][k]});int d=p[k]-std::max(low,std::min(high,p[k]));score+=d*d*(k==1 ? 16 : 1);}
        Candidate c={uint16_t(i),score};if(fallbackCount<budget){best[fallbackCount++]=c;std::push_heap(best,best+fallbackCount,compare);}else if(score<best[0].score){std::pop_heap(best,best+fallbackCount,compare);best[fallbackCount-1]=c;std::push_heap(best,best+fallbackCount,compare);}
    }
    for(int i=0;i<fallbackCount;++i)fallbackFaces[i]=best[i].face;return fallbackCount;
}

bool World::backdrop(const RevcTriangle& t,int x,int y,int z) const {
    if(!excludedGeometryCount)return false;
    const int anchor[3]={x*4096,y*4096,z*4096};uint32_t hash=2166136261u;
    for(const auto& v:t.vertex)for(int k=0;k<3;++k) {
        int value=v[k]+anchor[k],absolute=std::abs(value),quarter=absolute/4;
        if(absolute%4>2 || (absolute%4==2 && (quarter&1)))++quarter;
        uint32_t q=value<0 ? uint32_t(-quarter) : uint32_t(quarter);
        for(int byte=0;byte<4;++byte)hash=(hash^((q>>(byte*8))&255))*16777619u;
    }
    return std::binary_search(excludedGeometry.get(),excludedGeometry.get()+excludedGeometryCount,hash);
}

void World::buildRenderTiles() {
    for(auto& tile:tiles)tile=RenderTile{};
    auto cell=[](int sum,int width){int v=sum/3;return v>=0 ? v/width : -((-v+width-1)/width);};
    for(int i=0;i<residentCount;++i) {
        const auto& t=resident[i];
        const int ax=t.vertex[1][0]-t.vertex[0][0],ay=t.vertex[1][1]-t.vertex[0][1],az=t.vertex[1][2]-t.vertex[0][2];
        const int bx=t.vertex[2][0]-t.vertex[0][0],by=t.vertex[2][1]-t.vertex[0][1],bz=t.vertex[2][2]-t.vertex[0][2];
        const int64_t nx=int64_t(ay)*bz-int64_t(az)*by,ny=int64_t(az)*bx-int64_t(ax)*bz,nz=int64_t(ax)*by-int64_t(ay)*bx;
        walkableFace[i]=ny && ny*ny>=nx*nx+nz*nz;
        // Floors must not pay for a dense wall in the same spatial bin.
        // Smaller wall groups also keep a nearby facade from being rejected
        // wholesale when only part of the detail budget remains.
        const int width=256;
        const uint32_t key=uint32_t(cell(t.vertex[0][0]+t.vertex[1][0]+t.vertex[2][0],width)+32) |
            (uint32_t(cell(t.vertex[0][1]+t.vertex[1][1]+t.vertex[2][1],width)+32)<<6) |
            (uint32_t(cell(t.vertex[0][2]+t.vertex[1][2]+t.vertex[2][2],width)+32)<<12) |
            (uint32_t(walkableFace[i])<<18);
        uint32_t hash=key^(key>>8);hash*=2654435761u;hash^=hash>>16;
        unsigned slot=hash&(RenderTiles-1),start=slot;
        while(tiles[slot].count && tiles[slot].key!=key){slot=(slot+1)&(RenderTiles-1);if(slot==start)break;}
        auto& tile=tiles[slot];
        if(!tile.count){tile.key=key;for(int k=0;k<3;++k)tile.low[k]=tile.high[k]=t.vertex[0][k];}
        ++tile.count;faceTile[i]=slot;renderOrder[i]=i;
        for(const auto& v:t.vertex)for(int k=0;k<3;++k){tile.low[k]=std::min(tile.low[k],v[k]);tile.high[k]=std::max(tile.high[k],v[k]);}
    }
    // Tile publication can happen every moving frame. A stable two-pass radix
    // sort keeps material batches without a comparison sort of 2,048 faces.
    uint16_t temporary[Capacity],counts[256];
    for(unsigned shift:{0u,8u}) {
        std::fill_n(counts,256,0);
        for(int i=0;i<residentCount;++i)++counts[(material[renderOrder[i]].texture>>shift)&255];
        unsigned start=0;for(auto& count:counts){unsigned n=count;count=start;start+=n;}
        for(int i=0;i<residentCount;++i){unsigned face=renderOrder[i];temporary[counts[(material[face].texture>>shift)&255]++]=face;}
        std::copy_n(temporary,residentCount,renderOrder);
    }
}
int World::visible(Point camera,float sine,float cosine,float ps,float pc,const Point* focus) {
    const int limit=renderBudget();const ViewFrustum view(camera,sine,cosine,ps,pc);
    const int key[12]={ViewFrustum::bin(view.x),ViewFrustum::bin(view.y),ViewFrustum::bin(view.z),view.si,view.co,view.sp,view.cp,focus ? int(std::floor(focus->x*2)) : 0,focus ? int(std::floor(focus->y*2)) : 0,focus ? int(std::floor(focus->z*2)) : 0,limit,focus!=nullptr};
    if(viewVersion==version && !std::memcmp(key,viewKey,sizeof(key)))return cachedVisible;
    std::memcpy(viewKey,key,sizeof(key));viewVersion=version;
    uint16_t order[RenderTiles];int n=0;
    for(unsigned i=0;i<RenderTiles;++i) {
        auto& tile=tiles[i];tile.selected=false;
        if(!tile.count || view.x<tile.low[0]-1536 || view.x>tile.high[0]+1536 || view.z<tile.low[2]-1536 || view.z>tile.high[2]+1536)continue;
        int depth;if(!view.bounds(tile.low,tile.high,depth))continue;
        tile.priority=(1u<<24)+std::max(0,depth);
        if(focus) {
            const int p[3]={int(focus->x*64),int(focus->y*64),int(focus->z*64)};bool covers=true;
            for(int k=0;k<3;++k)if(p[k]<tile.low[k]-(k==1 ? 32 : 40) || p[k]>tile.high[k]+(k==1 ? 32 : 40))covers=false;
            if(covers)tile.priority=1u<<24;
        }
        order[n++]=i;
    }
    // Spatial bins contain faces outside the view as well as visible faces.
    // Charge only the visible faces, otherwise a dense offscreen wall can
    // exclude the street in the same bin even when the street fits the GPU.
    uint16_t costs[RenderTiles]{};
    std::fill_n(faceInView,residentCount,0);
    for(int i=0;i<n;++i)tiles[order[i]].selected=true;
    for(int i=0;i<residentCount;++i)if(tiles[faceTile[i]].selected && view.triangle(resident[i])){
        faceInView[i]=1;++costs[faceTile[i]];
        if(focus && walkableFace[i]) {
            const auto& t=resident[i];uint32_t distance=0;
            const int p[3]={int(focus->x*64),int(focus->y*64),int(focus->z*64)};
            for(int k=0;k<3;++k){int low=std::min({t.vertex[0][k],t.vertex[1][k],t.vertex[2][k]}),high=std::max({t.vertex[0][k],t.vertex[1][k],t.vertex[2][k]});int d=p[k]-std::max(low,std::min(high,p[k]));distance+=d*d*(k==1 ? 16 : 1);}
            if(distance<=8*8*4096)tiles[faceTile[i]].priority=std::min(tiles[faceTile[i]].priority,distance);
        }
    }
    std::sort(order,order+n,[this](unsigned a,unsigned b){return tiles[a].priority==tiles[b].priority ? tiles[a].key<tiles[b].key : tiles[a].priority<tiles[b].priority;});
    for(int i=0;i<n;++i)tiles[order[i]].selected=false;
    int count=0;selectedTileCount=0;
    for(int i=0;i<n;++i){auto& tile=tiles[order[i]];int cost=costs[order[i]];if(!cost || count+cost>limit)continue;tile.selected=true;selectedTiles[selectedTileCount]=order[i];selectedTileCosts[selectedTileCount++]=cost;count+=cost;}
    count=0;
    for(int i=0;i<residentCount;++i){unsigned face=renderOrder[i];if(faceInView[face] && tiles[faceTile[face]].selected)drawList[count++]={int(face),0};}
    fogDistance=64.f;return cachedVisible=count;
}
}
