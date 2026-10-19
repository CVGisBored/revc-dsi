#include "world.h"
#include "distant.h"
#include "population.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <algorithm>
#include <cmath>
static slice::World world,reference;
static slice::DistantWorld farWorld,farReference;
static slice::Population population;
static unsigned maxBytes=0,totalFrames=0;
static slice::World priority,blocking,legacy;
static void longRoad(const char* root,bool vehicle,bool slow=false) {
    assert(priority.open(root) && blocking.open(root) && legacy.open(root));
    slice::Point global={-150,10,-350},shift;
    assert(priority.update(global,shift,true) && blocking.update(global,shift,true) && legacy.update(global,shift,true));
    slice::Game moving,expected,legacyGame;
    auto p=slice::Point{global.x-priority.anchor().x,global.y-priority.anchor().y,global.z-priority.anchor().z};
    assert(moving.startExploration(priority.triangles(),priority.count(),p));
    assert(expected.startExploration(blocking.triangles(),blocking.count(),p));
    assert(legacyGame.startExploration(legacy.triangles(),legacy.count(),p));
    auto start=moving.player;auto initialAnchor=priority.anchor();
    unsigned collision=priority.collisionRevision(),early=0,peak=0,rebases=0;
    const float speed=vehicle ? 40.f : 14.668074f;
    const int frames=int(90/(speed*4/60));
    for(int frame=0;frame<frames;++frame) {
        auto lead=moving.player;lead.z+=6;
        auto version=priority.revision();
        assert(priority.updateCollision(moving.player,lead,slow ? 1 : 16));peak=std::max(peak,priority.lastCollisionReadBytes());
        if(!priority.urgentCollisionRefresh())assert(priority.lastCollisionReadBytes()<=32768+2560);
        if(priority.collisionRevision()!=collision && priority.revision()==version)++early;
        // Deliberately starve decorative work every other frame. Collision must
        // continue to publish and support movement while that snapshot waits.
        if(frame%2==0) {
            assert(priority.update(lead,shift,false,true));moving.rebase(shift);
            if(shift.x || shift.z)++rebases;
            auto oldLead=legacyGame.player;oldLead.z+=6;
            assert(legacy.update(oldLead,shift,false,true));legacyGame.rebase(shift);legacyGame.bindGeometry(legacy.triangles(),legacy.count());
        }
        if(collision!=priority.collisionRevision()) {moving.bindGeometry(priority.triangles(),priority.count());collision=priority.collisionRevision();}
        assert(blocking.update(expected.player,shift));expected.rebase(shift);expected.bindGeometry(blocking.triangles(),blocking.count());
        for(int tick=0;tick<4;++tick) {
            if(vehicle){moving.vehicleMove(0,speed/60,0,-1);expected.vehicleMove(0,speed/60,0,-1);legacyGame.vehicleMove(0,speed/60,0,-1);}
            else {moving.movePlayer(0,speed/60,1.f/60);expected.movePlayer(0,speed/60,1.f/60);legacyGame.movePlayer(0,speed/60,1.f/60);}
        }
        auto a=priority.anchor(),b=blocking.anchor();
        assert(std::fabs(moving.player.x+a.x-expected.player.x-b.x)<.04f);
        assert(std::fabs(moving.player.y+a.y-expected.player.y-b.y)<.04f);
        assert(std::fabs(moving.player.z+a.z-expected.player.z-b.z)<.04f);
    }
    float travel=moving.player.z+priority.anchor().z-start.z-initialAnchor.z;
    float oldTravel=legacyGame.player.z+legacy.anchor().z-start.z-initialAnchor.z;
    std::printf("Prioritized %s: %.2fm, %u independent collision commits, %u rebases, peak COL IO %u bytes\n",vehicle ? "car at 144km/h" : "source sprint",travel,early,rebases,peak);
    assert(travel>85 && early>2);
    if(slow){std::printf("Slow SD model, one background collision read per frame: %.2fm, %u urgent refreshes\n",travel,priority.urgentCollisionRefreshes());assert(priority.urgentCollisionRefreshes()>0);}
    std::printf("Old render-gated collision on same road: %.2fm\n",oldTravel);
    if(!slow)assert(oldTravel+10<travel);
    if(!vehicle && !slow)assert(rebases>0);
    priority.close();blocking.close();legacy.close();
}
static void slowTurnaround(const char* root) {
    assert(priority.open(root) && blocking.open(root));slice::Point seed={-150,10,-350},shift;
    assert(priority.update(seed,shift,true) && blocking.update(seed,shift,true));
    slice::Game moving,expected;auto a=priority.anchor();slice::Point p={seed.x-a.x,seed.y-a.y,seed.z-a.z};
    assert(moving.startExploration(priority.triangles(),priority.count(),p));assert(expected.startExploration(blocking.triangles(),blocking.count(),p));
    unsigned revision=priority.collisionRevision();float travel=0;
    for(int frame=0;frame<96;++frame){float direction=frame<32 || frame>=64 ? 1.f : -1.f;auto lead=moving.player;lead.z+=direction*6;
        assert(priority.updateCollision(moving.player,lead,1));
        if(frame%2==0){assert(priority.update(lead,shift,false,true));moving.rebase(shift);}
        if(revision!=priority.collisionRevision()){moving.bindGeometry(priority.triangles(),priority.count());revision=priority.collisionRevision();}
        assert(blocking.update(expected.player,shift));expected.rebase(shift);expected.bindGeometry(blocking.triangles(),blocking.count());
        auto before=moving.player;
        for(int tick=0;tick<4;++tick){moving.vehicleMove(0,direction*.3f,0,-direction);expected.vehicleMove(0,direction*.3f,0,-direction);}
        travel+=std::fabs(moving.player.z-before.z);auto ma=priority.anchor(),ea=blocking.anchor();
        assert(std::fabs(moving.player.z+ma.z-expected.player.z-ea.z)<.04f);assert(std::fabs(moving.player.y+ma.y-expected.player.y-ea.y)<.04f);
    }
    assert(travel>110 && priority.urgentCollisionRefreshes()>0);std::printf("Slow SD turnarounds: %.2fm, %u urgent refreshes; matches blocking COL\n",travel,priority.urgentCollisionRefreshes());priority.close();blocking.close();
}
static void visualPace(const char* root){
 unsigned frames[2]{},bytes[2]{};
 for(int mode=0;mode<2;++mode){assert(priority.open(root));slice::Point seed={-150,10,-350},shift;assert(priority.update(seed,shift,true));auto a=priority.anchor();slice::Point query={seed.x-a.x,seed.y-a.y,seed.z-a.z+12};
  do{assert(priority.update(query,shift,false,true,mode==1));query.x-=shift.x;query.y-=shift.y;query.z-=shift.z;bytes[mode]+=priority.lastReadBytes();++frames[mode];assert(priority.lastStreamUnits()<=(mode ? 16u : 8u));assert(priority.lastReadBytes()<(mode ? 24576u : 11392u));assert(frames[mode]<256);}while(priority.streaming());priority.close();}
 assert(frames[1]<frames[0]);std::printf("Visual snapshot, identical cold neighbourhood: walking %u frames / %u bytes; driving %u frames / %u bytes\n",frames[0],bytes[0],frames[1],bytes[1]);
}
static void coveragePace(const char* root,float step=1.2f){
 char path[1024];std::snprintf(path,sizeof(path),"%s/world/distant.vcd",root);
 assert(priority.open(root) && farWorld.open(path) && farReference.open(path));slice::Point global={-150,10,-350},shift;assert(priority.update(global,shift,true));auto anchor=priority.anchor();assert(farWorld.update(global,anchor) && farReference.update(global,anchor));
 unsigned start=farWorld.snapshotRevision(),oldStart=farReference.snapshotRevision(),hits=0,read=0,oldRead=0,busy=0;float lag=0,oldLag=0;
 for(int frame=0;frame<int(115.2f/step);++frame){global.z+=step;anchor=priority.anchor();slice::Point player={global.x-anchor.x,global.y-anchor.y,global.z-anchor.z},lead=player;lead.z+=6;
  assert(priority.updateCollision(player,lead));assert(priority.update(lead,shift,false,true,true));anchor=priority.anchor();busy+=priority.lastReadBytes()>0;
  auto prefetched=global;prefetched.z+=6;assert(farWorld.update(prefetched,anchor,true,!priority.urgentCollisionRefresh()));hits+=farWorld.reusedObjects();read+=farWorld.readBytes();
  assert(farReference.update(global,anchor,true,priority.lastReadBytes()==0));oldRead+=farReference.readBytes();
  lag=std::max(lag,std::fabs(global.z-farWorld.centre().z));oldLag=std::max(oldLag,std::fabs(global.z-farReference.centre().z));
 }
 unsigned fresh=farWorld.snapshotRevision()-start,old=farReference.snapshotRevision()-oldStart;
 std::printf("Moving coverage at %.0fkm/h: %u busy IO frames; fresh %u vs idle-only %u commits; max lag %.2f vs %.2fm; %u object reuses, %u vs %u read bytes\n",step*60/4*3.6f,busy,fresh,old,lag,oldLag,hits,read,oldRead);
 // Tile reuse deliberately creates idle IO frames; continuous disk activity
 // is no longer a useful success condition. Coverage must still keep pace.
 assert(fresh>=old && hits>0 && lag<=oldLag && read>0);priority.close();
}
static slice::Point local(slice::Point global,slice::Point anchor){return {global.x-anchor.x,global.y-anchor.y,global.z-anchor.z};}
static void check(slice::Point global,bool expectCollision) {
    auto anchor=world.anchor();auto revision=world.revision(),collision=world.collisionRevision();
    int count=world.count();slice::Point shift;
    unsigned frames=0;
    do {
        assert(world.update(local(global,world.anchor()),shift,false,true));
        assert(world.lastStreamUnits()<=8);maxBytes=std::max(maxBytes,world.lastReadBytes());
        // The final small read may cross the byte admission budget; no
        // whole-neighbourhood read may happen in one incremental update.
        assert(world.lastReadBytes()<8192+3200);
        ++frames;assert(frames<256);
        if(world.streaming()) {
            assert(world.revision()==revision && world.collisionRevision()==collision);
            assert(world.count()==count && world.anchor().x==anchor.x && world.anchor().z==anchor.z);
            assert(shift.x==0 && shift.y==0 && shift.z==0);
        }
    }while(world.streaming());
    assert(frames>1 && world.revision()==revision+1);
    assert(world.collisionRevision()==collision+unsigned(expectCollision));
    assert(reference.update(local(global,reference.anchor()),shift));
    assert(world.count()==reference.count() && world.renderCount()==reference.renderCount());
    assert(!std::memcmp(world.triangles(),reference.triangles(),world.count()*sizeof(RevcTriangle)));
    auto p=local(global,world.anchor());
    for(int i=0;i<12;++i) {
        float yaw=i*3.14159265f/6,si=std::sin(yaw),co=std::cos(yaw);
        slice::Point camera={p.x-3*si,p.y+1.65f,p.z+3*co};
        int n=world.visible(camera,si,co,.18f,.984f,&p);
        assert(n==reference.visible(camera,si,co,.18f,.984f,&p));
        for(int j=0;j<n;++j)assert(!std::memcmp(&world.visibleTriangle(j),&reference.visibleTriangle(j),sizeof(RevcTriangle)));
    }
    totalFrames+=frames;
    std::printf("Snapshot %.2f %.2f: %u incremental frames, collision refresh %d\n",global.x,global.z,frames,expectCollision);
}
int main(int argc,char** argv) {
    assert(argc==2 && world.open(argv[1]) && reference.open(argv[1]));
    slice::Point p={12.95f,10.03125f,-.39f},shift;
    assert(world.update(p,shift,true) && reference.update(p,shift,true));
    check({16.05f,p.y,p.z},false);
    check({20.05f,p.y,p.z},true);
    check({64.1f,p.y,p.z},true);
    // Continue across a region boundary at the source sprint speed at 30 FPS.
    // Verify the active COL snapshot never leaves the moving query unsupported.
    slice::Game game,baseline;auto moving=local({12.95f,p.y,p.z},world.anchor());
    assert(world.update(moving,shift,true));moving=local({12.95f,p.y,p.z},world.anchor());
    assert(game.startExploration(world.triangles(),world.count(),moving));
    assert(reference.update(local({12.95f,p.y,p.z},reference.anchor()),shift,true));
    assert(baseline.startExploration(reference.triangles(),reference.count(),local({12.95f,p.y,p.z},reference.anchor())));
    unsigned binds=0;auto collision=world.collisionRevision();float metres=0;
    float leadX=0,leadZ=0;
    for(int frame=0;frame<240;++frame) {
        auto query=game.player;query.x+=leadX;query.z+=leadZ;
        assert(world.update(query,shift,false,true));game.rebase(shift);
        if(collision!=world.collisionRevision()){game.bindGeometry(world.triangles(),world.count());collision=world.collisionRevision();++binds;}
        assert(reference.update(baseline.player,shift));baseline.rebase(shift);baseline.bindGeometry(reference.triangles(),reference.count());
        auto before=game.player;
        for(int tick=0;tick<2;++tick){game.movePlayer(0,-14.668074f/60,1.f/60);baseline.movePlayer(0,-14.668074f/60,1.f/60);}
        metres+=std::fabs(game.player.z-before.z);
        leadX=(game.player.x-before.x)*18;leadZ=(game.player.z-before.z)*18;
        float lead=std::sqrt(leadX*leadX+leadZ*leadZ);if(lead>9){leadX*=9/lead;leadZ*=9/lead;}
        auto ga=world.anchor(),ba=reference.anchor();
        assert(std::fabs(game.player.x+ga.x-baseline.player.x-ba.x)<.03f);
        assert(std::fabs(game.player.y+ga.y-baseline.player.y-ba.y)<.03f);
        assert(std::fabs(game.player.z+ga.z-baseline.player.z-ba.z)<.03f);
        assert(game.playerGrounded()==baseline.playerGrounded());
    }
    std::printf("Movement %.2fm binds %u\n",metres,binds);assert(metres>6 && binds>0);
    assert(population.open(argv[1]) && population.refresh(game,world.anchor(),{0,0,0}));
    game.player.x+=8.5f;unsigned pathFrames=0,pathPeak=0;
    do {
        assert(population.refresh(game,world.anchor(),{0,0,0},true));
        pathPeak=std::max(pathPeak,population.readBytes());assert(population.readBytes()<=64*28);
        assert(++pathFrames<256);
    }while(population.streaming());
    assert(pathFrames>1);std::printf("Population paths: peak %u bytes over %u incremental frames\n",pathPeak,pathFrames);
    char path[1024];std::snprintf(path,sizeof(path),"%s/world/distant.vcd",argv[1]);
    assert(farWorld.open(path) && farReference.open(path));
    slice::Point global={12.95f,p.y,-.39f},anchor=world.anchor();
    assert(farWorld.update(global,anchor) && farReference.update(global,anchor));
    global.z-=15;unsigned frames=0,farMax=0;
    do {assert(farWorld.update(global,anchor,true));farMax=std::max(farMax,farWorld.readBytes());assert(farWorld.readBytes()<4096+2048);assert(++frames<1024);}while(farWorld.streaming());
    assert(frames>1 && farReference.update(global,anchor));
    auto focus=local(global,anchor);slice::Point camera={focus.x,focus.y+1.65f,focus.z+3};
    int n=farWorld.visible(camera,0,1,.18f,.984f,focus);
    assert(n==farReference.visible(camera,0,1,.18f,.984f,focus));
    assert(n==farWorld.visible(camera,0,1,.18f,.984f,focus));
    for(int i=0;i<n;++i)assert(!std::memcmp(&farWorld.triangle(i),&farReference.triangle(i),sizeof(RevcTriangle)));
    std::printf("Incremental world: peak read %u bytes; near snapshots %u frames; far peak %u over %u frames; sprint %.2fm with %u collision binds\n",maxBytes,totalFrames,farMax,frames,metres,binds);
    longRoad(argv[1],false);longRoad(argv[1],true);longRoad(argv[1],true,true);longRoad(argv[1],false,true);slowTurnaround(argv[1]);visualPace(argv[1]);coveragePace(argv[1]);coveragePace(argv[1],2.4f);
}
