#include "world.h"
#include "campaign.h"
#include "campaign_data.h"
#include "ken_missions_data.h"
#include "expanded_missions_data.h"
#include <cassert>
#include <cmath>
#include <cstdio>
static slice::World world;static slice::Game game;static slice::Campaign campaign;static slice::Street actors;
static bool visit(slice::Point global,bool optional=false) {
    auto old=world.anchor();slice::Point local{global.x-old.x,global.y-old.y,global.z-old.z},shift;
    assert(world.update(local,shift,true));auto a=world.anchor();local={global.x-a.x,global.y-a.y,global.z-a.z};
    bool supported=game.startExploration(world.triangles(),world.count(),local);
    for(int i=1;!supported && i<=8;++i){auto foot=local;foot.y-=i*.25f;supported=game.startExploration(world.triangles(),world.count(),foot);}
    if(!supported && optional)return false;assert(supported);actors.car=game.player;actors.speed=0;
    std::printf("Visit %.2f %.2f %.2f -> %.2f %.2f %.2f\n",global.x,global.y,global.z,game.player.x+a.x,game.player.y+a.y,game.player.z+a.z);return true;
}
static void tick(int frames=1){campaign.tick(game,actors,world.anchor(),frames,false);}
static void waitPassed(){for(int i=0;i<7;++i)tick(30);assert(campaign.state()==slice::Campaign::Office || campaign.state()==slice::Campaign::CampaignComplete);}
static void office(){actors.health=100;actors.inCar=false;auto a=world.anchor();auto local=campaign.target(a);visit({local.x+a.x,local.y+a.y,local.z+a.z});tick();assert(campaign.briefing() && campaign.pendingCutscene());
    auto saved=campaign;campaign.finishCutscene(false);assert(campaign.briefing() && !campaign.pendingCutscene());assert(*campaign.dialogue());campaign=saved;
    campaign.finishCutscene(true);assert(!campaign.briefing() && !campaign.pendingCutscene());}
static void hit(slice::Point global,bool bullet) {
    auto a=world.anchor();float dx=global.x-a.x-game.player.x,dz=global.z-a.z-game.player.z,d=std::sqrt(dx*dx+dz*dz);
    assert(d>0);bool struck=campaign.strike(game,actors,a,dx/d,-dz/d,bullet);
    // These checks relocate between objectives; choose a supported attack side
    // with genuine line of sight rather than firing through original walls.
    for(int side=0;!struck && side<8;++side){auto site=global;float angle=side*3.14159265f/4;site.x+=std::sin(angle)*2.6f;site.z+=std::cos(angle)*2.6f;if(!visit(site,true))continue;actors.inCar=false;a=world.anchor();dx=global.x-a.x-game.player.x;dz=global.z-a.z-game.player.z;d=std::sqrt(dx*dx+dz*dz);struck=campaign.strike(game,actors,a,dx/d,-dz/d,bullet);}
    assert(struck);tick(30);
}
static void attackPosition(slice::Point global) {
    bool supported=false;for(int side=0;!supported && side<8;++side){auto p=global;float a=side*.785398163f;p.x+=std::sin(a)*2.9f;p.z+=std::cos(a)*2.9f;supported=visit(p,true);}assert(supported);actors.inCar=false;tick();
}
int main(int argc,char** argv){
    assert(argc==2 && world.open(argv[1]));campaign.reset();actors.health=actors.carHealth=100;actors.guardHealth=0;
    visit(slice::missionData::Hotel);tick();assert(campaign.talking());campaign.finishCall();waitPassed();assert(campaign.completed()==1);
    office();assert(campaign.state()==slice::Campaign::Clothes);visit(slice::missionData::Clothes);tick();assert(campaign.state()==slice::Campaign::Yacht);
    visit(slice::missionData::Yacht);tick();assert(campaign.state()==slice::Campaign::Escort);tick();assert(campaign.hasPassenger());
    actors.inCar=true;tick();assert(campaign.passengerInCar());visit(slice::missionData::Club);tick();assert(campaign.state()==slice::Campaign::Passed && actors.cash==100);waitPassed();
    office();assert(campaign.state()==slice::Campaign::Malibu);visit(slice::kenData::Malibu);tick();assert(campaign.state()==slice::Campaign::ChefFight);
    attackPosition(slice::kenData::Chef);for(int i=0;i<4;++i)hit(campaign.npcs()[0].global,false);assert(campaign.state()==slice::Campaign::PhonePickup);
    visit(slice::kenData::Chef);tick();assert(campaign.state()==slice::Campaign::Escape);tick();actors.inCar=true;tick();assert(campaign.passengerInCar());actors.inCar=false;tick();
    for(int i=0;i<3 && campaign.state()==slice::Campaign::Escape;++i){auto target=campaign.npcs()[i].global;attackPosition(target);while(campaign.state()==slice::Campaign::Escape && campaign.npcs()[i].health>0)hit(campaign.npcs()[i].global,true);}
    assert(campaign.state()==slice::Campaign::AmmoShop);actors.inCar=true;tick(); // Rejoin Lance at the supported fight location.
    visit(slice::kenData::Ammo);tick();assert(campaign.state()==slice::Campaign::ReturnHotel);
    visit(slice::missionData::Hotel);tick();actors.inCar=false;tick();assert(campaign.state()==slice::Campaign::Passed && actors.cash==300);waitPassed();
    office();assert(campaign.state()==slice::Campaign::JuryOne);
    {auto saved=campaign;auto savedActors=actors;attackPosition(slice::kenData::Jurors[0]);
     for(int k=0;k<4 && campaign.state()!=slice::Campaign::Failed;++k)hit(campaign.npcs()[0].global,false);
     assert(campaign.state()==slice::Campaign::Failed);campaign.retry(actors);assert(campaign.state()==slice::Campaign::Office && actors.health==100 && actors.cash==300);
     campaign=saved;actors=savedActors;}

    {auto saved=campaign;auto token=campaign.vehicleToken();campaign.borrowVehicle(0);
     auto anchor=world.anchor();auto target=campaign.vehicles()[0].global;int hp=63;
     slice::Point moved{target.x-anchor.x+8,target.y-anchor.y,target.z-anchor.z+4};
     assert(campaign.syncVehicle(0,token,moved,anchor,8192,hp));assert(hp==63);
     assert(campaign.vehicles()[0].borrowed && campaign.vehicles()[0].yaw==8192);
     assert(std::fabs(campaign.vehicles()[0].global.x-target.x-8)<.001f);
     hp=100;assert(campaign.syncVehicle(0,token,moved,anchor,8192,hp) && hp==63);
     campaign.reset();assert(!campaign.syncVehicle(0,token,moved,anchor,0,hp));campaign=saved;}

    for(int i=0;i<2;++i){attackPosition(slice::kenData::JurorCars[i]);auto target=campaign.vehicles()[i].global;auto anchor=world.anchor();assert(campaign.blocks({target.x-anchor.x,target.y-anchor.y,target.z-anchor.z},anchor));for(int k=0;k<4;++k)hit(target,false);}
    assert(campaign.state()==slice::Campaign::Passed && actors.cash==700);waitPassed();
    office();assert(campaign.state()==slice::Campaign::RiotClothes);visit(slice::kenData::RiotClothes);tick();assert(campaign.state()==slice::Campaign::RiotWorkers && campaign.outfit()==3);
    for(int i=0;i<4;++i){attackPosition(slice::kenData::Workers[i]);hit(campaign.npcs()[i].global,false);}
    assert(campaign.state()==slice::Campaign::RiotVans);
    for(int i=0;i<3 && campaign.state()==slice::Campaign::RiotVans;++i){attackPosition(slice::kenData::Vans[i]);auto target=campaign.vehicles()[i].global;while(campaign.state()==slice::Campaign::RiotVans && campaign.vehicles()[i].health>0)hit(target,true);}
    assert(campaign.state()==slice::Campaign::Passed && actors.cash==1700);waitPassed();assert(campaign.state()==slice::Campaign::Office && campaign.completed()==5);
    office();assert(campaign.state()==slice::Campaign::SwineEntrance);
    visit(slice::extraData::Penthouse);tick();assert(campaign.state()==slice::Campaign::SwineFight);
    attackPosition(slice::extraData::Gonzalez);for(int i=0;i<8 && campaign.state()==slice::Campaign::SwineFight;++i)hit(campaign.npcs()[0].global,true);
    assert(campaign.state()==slice::Campaign::SwineEscape);auto away=slice::extraData::Penthouse;away.x+=35;visit(away);
    for(int i=0;i<31;++i)tick(30);assert(campaign.state()==slice::Campaign::Passed && actors.cash==1950);waitPassed();
    office();assert(campaign.state()==slice::Campaign::MallMeet);visit(slice::extraData::Courier);tick();assert(campaign.state()==slice::Campaign::MallFight);
    for(int i=0;i<3 && campaign.state()==slice::Campaign::MallFight;++i){attackPosition(campaign.npcs()[i].global);while(campaign.state()==slice::Campaign::MallFight && campaign.npcs()[i].health>0)hit(campaign.npcs()[i].global,true);}
    assert(campaign.state()==slice::Campaign::ChipPickup || campaign.state()==slice::Campaign::ChipDelivery);visit(slice::extraData::Courier);tick();assert(campaign.state()==slice::Campaign::ChipDelivery);
    visit(slice::extraData::Cortez);tick();assert(campaign.state()==slice::Campaign::Passed && actors.cash==2450);waitPassed();
    office();assert(campaign.state()==slice::Campaign::GolfClothes);visit(slice::extraData::GolfClothes);tick();assert(campaign.state()==slice::Campaign::GolfEntrance);
    visit(slice::extraData::GolfEntrance);tick();assert(campaign.state()==slice::Campaign::GolfFight);
    attackPosition(slice::extraData::GolfTarget);for(int i=0;i<8 && campaign.state()==slice::Campaign::GolfFight;++i)hit(campaign.npcs()[0].global,false);
    assert(campaign.state()==slice::Campaign::Passed && actors.cash==2950);waitPassed();assert(campaign.state()==slice::Campaign::CampaignComplete && campaign.completed()==8);
    tick(30);assert(actors.cash==2950);std::puts("Eight mission chain: authored coordinates, escorts, combat, juror cars, riot and once-only rewards passed");
}
