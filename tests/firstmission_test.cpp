#include "world.h"
#include "campaign.h"
#include "campaign_data.h"
#include <cassert>
#include <cstdio>
static slice::World world;static slice::Game game;static slice::Campaign mission;
int main(int argc,char** argv) {
    assert(argc==2);
    assert(world.open(argv[1]));auto seed=slice::missionData::Hotel; slice::Point shift;
    assert(world.update(seed,shift,true));seed.x-=shift.x;seed.y-=shift.y;seed.z-=shift.z;
    assert(game.startExploration(world.triangles(),world.count(),seed));
    slice::Street actors;actors.health=actors.carHealth=100;actors.inCar=true;
    mission.reset();mission.tick(game,actors,world.anchor(),1,false);assert(mission.state()==slice::Campaign::Hotel);
    actors.inCar=false;mission.tick(game,actors,world.anchor(),1,false);
    auto actual=game.player;auto anchor=world.anchor();
    std::printf("Actual XYZ %.4f %.4f %.4f; target %.4f %.4f %.4f\n",actual.x+anchor.x,actual.y+anchor.y,actual.z+anchor.z,slice::missionData::Hotel.x,slice::missionData::Hotel.y,slice::missionData::Hotel.z);
    std::printf("Hotel supported feet %.4f, authored trigger %.4f, stage %d\n",actual.y+anchor.y,slice::missionData::Hotel.y,mission.state());
    assert(mission.state()==slice::Campaign::Phone);mission.finishCall();assert(mission.state()==slice::Campaign::FirstComplete && !mission.hasTarget());
    mission.tick(game,actors,world.anchor(),60,true);assert(mission.state()==slice::Campaign::FirstComplete && actors.cash==0);
}
