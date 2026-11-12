#include "campaign.h"
#include "world.h"
#include "garage.h"
#include <cassert>
#include <cmath>
#include <cstdio>
using namespace slice;
static World world;
static Game game;
static Street actors;
static Campaign campaign;
static Population population;
static Point anchor(){return world.anchor();}
static Point local(Point p){auto a=anchor();return {p.x-a.x,p.y-a.y,p.z-a.z};}
static void visit(Point p,bool air=false){
    Point shift;
    if(air){
        assert(world.updateCollision(local(p),local(p),65536,true));
        assert(world.update(local(p),shift,false,true,true));
    }else{
        // Once independent flight collision is enabled, request the landing
        // surface before publishing the new visual origin, as in main.cpp.
        if(campaign.flying())assert(world.updateCollision(local(p),local(p),65536,true));
        if(!world.update(local(p),shift,true)){
            std::fprintf(stderr,"Could not stream ground at %.2f %.2f %.2f\n",p.x,p.y,p.z);assert(false);
        }
        assert(world.updateCollision(local(p),local(p),65536));
    }
    game.rebase(shift);actors.rebase(shift);population.rebase(shift);
    game.bindGeometry(world.triangles(),world.count());
    game.player=local(p);
    if(!air){
        bool found=false;
        for(int ring=0;ring<=5 && !found;++ring)for(int step=0;step<8 && !found;++step){
            auto q=local(p);q.x+=std::sin(step*.785398f)*ring*.5f;q.z+=std::cos(step*.785398f)*ring*.5f;
            if(game.placeActor(q,8)){game.player=q;found=true;}
        }
        if(!found)std::fprintf(stderr,"Unreachable location %.2f %.2f %.2f\n",p.x,p.y,p.z);
        assert(found);
    }
    game.state=Game::Playing;
}
static void tick(bool action=false,int frames=1){campaign.tick(game,actors,anchor(),frames,action);}
static void start(unsigned i){
    assert(campaign.selectMission(i) && campaign.briefing() && !campaign.pendingCutscene());
    actors.inCar=false;actors.health=actors.carHealth=100;
    campaign.advanceBriefing();visit(MissionLibrary::mission(i).start);tick();
}
static void purchase(unsigned id){
    const auto& m=MissionLibrary::mission(id);
    actors.cash=int(m.targets)-1;start(id);tick(true);
    assert(!campaign.missionDone(id) && !campaign.pendingCutscene());
    actors.cash+=41;actors.inCar=true;tick(true);actors.inCar=false;tick();
    assert(actors.cash==int(m.targets)+40);
    unsigned before=campaign.completed();
    tick(true);
    assert(actors.cash==40 && campaign.missionDone(id));
    if(m.movieCount){assert(campaign.pendingCutscene());campaign.finishCutscene(false);tick();}
    assert(campaign.state()==Campaign::LibraryPassed && campaign.completed()==before+1);
    start(id);tick(true);assert(campaign.state()==Campaign::LibraryPassed && actors.cash==40 && !campaign.pendingCutscene());
}
static void dispatch(unsigned id){
    const auto& m=MissionLibrary::mission(id);
    const int model=id==77 ? VehicleModels::FireTruck : id==78 ? VehicleModels::Police : VehicleModels::MrWhoopee;
    start(id);const int cash=actors.cash;
    assert(campaign.vehicleCount()==1 && campaign.vehicles()[0].placed);
    actors.car={500,0,500};population.reset();
    Garage::interact(game,actors,population,campaign,anchor());
    assert(actors.inCar && actors.carModel==model && actors.transitioning());
    actors.tick(90);tick(true);
    for(unsigned step=0;step<m.routeCount;++step){
        visit(m.route[step]);tick();
        if(id==78){
            assert(campaign.state()==Campaign::LibraryCombat && campaign.npcCount()==1);
            actors.inCar=false;
            for(int k=0;k<3;++k){
                bool hit=false;
                for(int side=0;side<16 && !hit;++side){
                    auto pos=campaign.npcs()[0].global;auto q=pos;
                    q.x+=std::sin(side*.392699f)*4;q.z+=std::cos(side*.392699f)*4;
                    visit(q);tick();pos=local(campaign.npcs()[0].global);
                    float x=pos.x-game.player.x,z=pos.z-game.player.z,d=std::hypot(x,z);
                    if(d>.1f)hit=campaign.strike(game,actors,anchor(),x/d,-z/d,true);
                }
                assert(hit);
            }
            tick();
        }else{
            actors.inCar=true;actors.carModel=0;tick(true,30);assert(campaign.routeStep()==step);
            actors.carModel=model;actors.speed=.3f;tick(true,30);assert(campaign.routeStep()==step);
            actors.speed=0;
            if(id==77){tick(true,30);tick(true,30);assert(campaign.routeStep()==step);tick(true,30);}
            else {assert(campaign.npcCount()==1 && campaign.npcs()[0].placed);tick(true);}
        }
        assert(campaign.routeStep()==step+1);
    }
    assert(campaign.state()==Campaign::LibraryPassed && actors.cash==cash+int(m.reward));
    tick(true,30);assert(actors.cash==cash+int(m.reward));
}
static unsigned flight(unsigned id){
    const auto& m=MissionLibrary::mission(id);const auto& course=MissionLibrary::course(id);
    start(id);const int cash=actors.cash;
    actors.inCar=true;tick(true);assert(!campaign.flying());actors.inCar=false;tick(true);
    assert(campaign.flying());
    float y=game.player.y;campaign.fly(game,anchor(),0,0,1,4,0);assert(game.player.y>y);
    for(unsigned i=0;i<course.count;++i){
        auto p=course.points[i];p.y+=10;visit(p,true);tick();assert(campaign.routeStep()==i);
        visit(course.points[i],true);tick();assert(campaign.routeStep()==i+1);
    }
    assert(campaign.flying());visit(m.start);tick();
    assert(campaign.state()==Campaign::LibraryPassed && !campaign.flying() && actors.cash==cash+100);
    return course.count;
}
int main(int argc,char** argv){
    assert(argc==2 && world.open(argv[1]));visit(Campaign::spawn());assert(actors.start(game,true));campaign.reset();
    for(unsigned i=36;i<=50;++i)purchase(i);
    for(unsigned i:{77u,78u,83u}){std::printf("Service %u\n",i);std::fflush(stdout);dispatch(i);}
    unsigned gates=0;
    for(unsigned i=84;i<=87;++i)gates+=flight(i);
    assert(gates==85 && campaign.completed()==22);
    dispatch(77);assert(campaign.completed()==22);
    for(int i=0;i<7;++i)tick(false,30);
    assert(campaign.state()==Campaign::FreeRoam && !campaign.hasTarget());
    std::puts("15 purchases, 3 dedicated service vehicles, 4 flights, 85 altitude-gated checkpoints and repeat payouts passed");
}
