#include "street.h"
#include <cstdio>
#include <cstdlib>
#include <cmath>

static void require(bool value,const char* message) {
    if(!value) {std::fprintf(stderr,"FAIL: %s\n",message);std::exit(1);}
}
static RevcTriangle tri(slice::Point a,slice::Point b,slice::Point c) {
    RevcTriangle result{};slice::Point p[3]={a,b,c};
    for(int i=0;i<3;++i) {
        result.vertex[i][0]=p[i].x*64;result.vertex[i][1]=p[i].y*64;result.vertex[i][2]=p[i].z*64;
    }
    return result;
}
int main() {
    RevcTriangle floor[]={tri({-20,0,-20},{20,0,-20},{20,0,20}),tri({-20,0,-20},{20,0,20},{-20,0,20})};
    slice::Game game;slice::Street job;
    require(game.startExploration(floor,2,{0,0,0}),"grounded startup");
    require(job.start(game),"mission sites and short vehicle route");
    const auto spawn=game.player;
    require(!job.shoot(0,1),"unarmed firing blocked");
    job.interact();require(job.armed && job.ammo==12 && job.phase==slice::Street::Guard,"pistol pickup and ammo");
    game.pause();require(!job.shoot(0,1),"pause freezes gun input");job.tick(200);require(job.health==100,"pause freezes guard");game.pause();
    game.player=job.car;job.interact();require(!job.inCar,"guard locks car until defeated");game.player=spawn;
    for(int i=0;i<3;++i) {
        float dx=job.guard.x-game.player.x,dz=job.guard.z-game.player.z,d=std::sqrt(dx*dx+dz*dz);
        require(job.shoot(dx/d,-dz/d),"fire pistol");
        require(!job.shoot(dx/d,-dz/d),"fire cooldown");job.tick(15);
    }
    require(job.guardHealth==0 && job.phase==slice::Street::Vehicle,"guard defeat unlocks vehicle");
    require(job.health<100,"alerted guard damages player");
    job.reload();job.tick(70);require(job.ammo==12 && job.reserve==33,"reload transfers reserve ammo");
    game.player={job.car.x+1.9f,job.car.y,job.car.z};job.interact();require(job.inCar,"enter car");
    require(job.transitioning(),"entry is animated");
    job.drive(1,false,0,1,4);require(job.speed==0,"entry locks throttle");
    job.tick(30);require(job.doorAngle()>1,"door opens during entry");
    game.pause();job.tick(90);require(job.transitioning(),"pause freezes door animation");game.pause();
    job.tick(60);require(!job.transitioning() && job.doorAngle()==0,"entry closes door");
    for(int i=0;i<160 && job.phase!=slice::Street::Complete;++i) {
        const float remaining=job.car.z-job.destination.z;
        const float stopping=job.speed*job.speed/(2*slice::Street::BrakeDeceleration)+.7f;
        job.drive(remaining>stopping ? 1 : 0,remaining<=stopping,0,1,1);job.tick(1);
    }
    require(job.phase==slice::Street::Complete && job.cash==250,"park vehicle to complete delivery and reward");
    job.tick(100);require(job.cash==250,"reward awarded once");
    job.interact();require(!job.inCar,"safe exit from stopped car");
    require(job.transitioning(),"exit is animated");job.tick(90);
    job.rebase({64,0,-64});game.rebase({64,0,-64});job.reset();
    require(std::fabs(game.player.x+64-spawn.x)<.01f && std::fabs(game.player.z-64-spawn.z)<.01f,"mission rebase preserves initial global spawn");
    require(job.phase==slice::Street::Pickup && job.health==100 && !job.armed,"retry resets mission inventory");
    job.health=0;job.tick(1);require(job.phase==slice::Street::Failed,"death fails mission");
    job.reset();job.carHealth=0;job.tick(1);require(job.phase==slice::Street::Failed,"destroyed vehicle fails mission");
    const RevcTriangle wall[]={tri({-20,0,-20},{20,0,-20},{20,0,20}),tri({-20,0,-20},{20,0,20},{-20,0,20}),
                              tri({-20,0,-2},{20,0,-2},{20,3,-2}),tri({-20,0,-2},{20,3,-2},{-20,3,-2})};
    game.bindGeometry(wall,4);
    require(!game.lineClear({0,1,0},{0,1,-5}),"wall blocks bullets");
    require(game.lineClear({0,1,0},{3,1,0}),"unobstructed line passes");
    game.player={0,0,2};game.state=slice::Game::Playing;
    game.vehicleMove(0,-2,0,1);
    require(game.player.z>=.54f,"original-sized vehicle hull cannot cross wall");
    job.reset();game.bindGeometry(wall,4);game.player={0,0,.57f};job.car=game.player;
    job.inCar=true;job.speed=.2f;job.drive(1,false,0,1,1);
    require(job.carHealth<100 && job.speed==0,"fast impact damages and stops car");
    int damage=job.carHealth;job.drive(1,false,0,1,4);
    require(job.carHealth==damage,"stationary wall contact must not drain vehicle health");
    job.speed=0;job.bodyTilt={.06f,0,.08f};
    for(int i=0;i<60;++i)job.drive(0,true,0,1,4);
    require(std::fabs(job.bodyTilt.x)+std::fabs(job.bodyTilt.z)<.0001f,"body springs settle after stopping");
    std::puts("Mission pickup, combat, guard AI, reload, driving, reward, failure, pause and rebase tests passed");
}
