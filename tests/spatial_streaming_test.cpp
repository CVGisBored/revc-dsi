#include "world.h"
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <vector>
static slice::World indexed,linear;
int main(int argc,char** argv) {
    assert(argc==3 && indexed.open(argv[1]) && linear.open(argv[2]));
    unsigned bytes=0,referenceBytes=0,skipped=0,cold=0,warm=0;
    for(int site=0;site<24;++site) {
        slice::Point global={-150,10,-350+site*4.f},shift;
        auto a=indexed.anchor(),b=linear.anchor();
        assert(indexed.update({global.x-a.x,global.y-a.y,global.z-a.z},shift,true));
        assert(linear.update({global.x-b.x,global.y-b.y,global.z-b.z},shift,true));
        bytes+=indexed.lastReadBytes();referenceBytes+=linear.lastReadBytes();
        assert(indexed.count()==linear.count());
        // Bounds are only a broad phase: no selected COL face may change.
        assert(!std::memcmp(indexed.triangles(),linear.triangles(),indexed.count()*sizeof(RevcTriangle)));
    }
    skipped=indexed.skippedCollisionBlocks();assert(skipped>0 && bytes<referenceBytes);
    auto a=indexed.anchor();slice::Point p={-150-a.x,10-a.y,-350-a.z},shift;
    assert(indexed.update(p,shift,true));p.x-=shift.x;p.y-=shift.y;p.z-=shift.z;
    // A six metre lead must not start new reads once an unchanged request has
    // completed. The old test against the lead centre could loop indefinitely.
    p.z+=8;auto lead=p;lead.z+=6;
    unsigned frames=0;
    do {assert(indexed.updateCollision(p,lead));assert(++frames<256);}while(indexed.collisionStreaming());
    auto revision=indexed.collisionRevision();
    for(int frame=0;frame<100;++frame){assert(indexed.updateCollision(p,lead));assert(indexed.lastCollisionReadBytes()==0 && indexed.collisionRevision()==revision);}
    assert(indexed.open(argv[1]));slice::Point seed={-150,10,-350};
    assert(indexed.update(seed,shift,true));cold=indexed.lastReadBytes();
    a=indexed.anchor();p={seed.x-a.x,seed.y-a.y,seed.z-a.z};
    assert(indexed.update(p,shift,true));warm=indexed.lastReadBytes();
    assert(indexed.terrainCacheHits()>0 && warm<cold);
    printf("Spatial stream: %u vs %u total read bytes across 24 road snapshots; %u skipped COL blocks; identical collision faces\n",bytes,referenceBytes,skipped);
    printf("Stationary six metre lookahead: 100 frames, zero reads/rebuilds; floor cache: cold %u, warm %u bytes, %u decoded hits\n",cold,warm,indexed.terrainCacheHits());
    assert(indexed.open(argv[1]) && indexed.enableExtendedCache());unsigned extendedBytes=0;
    for(int site=0;site<24;++site){slice::Point global={-150,10,-350+site*4.f};auto origin=indexed.anchor();
        assert(indexed.update({global.x-origin.x,global.y-origin.y,global.z-origin.z},shift,true));extendedBytes+=indexed.lastReadBytes();}
    assert(extendedBytes<bytes && indexed.collisionCacheHits()>0);
    printf("DSi RAM cache: %u vs %u DS read bytes on identical 24-snapshot route; %u source COL cache hits\n",extendedBytes,bytes,indexed.collisionCacheHits());
}
