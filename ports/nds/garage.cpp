#include "garage.h"
#include <cmath>
#include <algorithm>
namespace slice {
namespace {float distance2(Point a,Point b){return (a.x-b.x)*(a.x-b.x)+(a.z-b.z)*(a.z-b.z);}}
Population::Traffic Garage::snapshot(const Street& job) {
    Population::Traffic old;old.position=job.car;old.tilt=job.carTilt;old.yaw=job.carYaw;old.model=job.carModel;old.health=job.carHealth;old.active=old.parked=true;old.driver=false;old.wheelRoll=job.wheelRoll;old.mission=job.missionVehicle;old.missionToken=job.missionToken;return old;
}
void Garage::interact(Game& game,Street& job,Population& population,Campaign& campaign,Point anchor,const VehicleModels* models) {
    if(job.inCar || job.transitioning()){job.interact();return;}
    float best=job.carHealth>0 ? distance2(game.player,job.car) : 1e9f;int choice=-1,mission=-1;
    auto* cars=population.vehicles();
    for(int i=0;i<Population::VehicleCapacity;++i)if(cars[i].active && cars[i].health>0 && std::fabs(cars[i].position.y-game.player.y)<2) {
        float d=distance2(game.player,cars[i].position);if(d<best){best=d;choice=i;mission=-1;}
    }
    const auto* scripted=campaign.vehicles();
    for(int i=0;i<campaign.vehicleCount();++i)if(!scripted[i].borrowed && scripted[i].placed && scripted[i].health>0) {
        auto p=scripted[i].global;p.x-=anchor.x;p.y-=anchor.y;p.z-=anchor.z;
        float d=distance2(game.player,p);if(std::fabs(p.y-game.player.y)<2 && d<best){best=d;choice=-1;mission=i;}
    }
    if(best>12.25f)return;
    if(choice<0 && mission<0){job.interact();return;}
    auto old=snapshot(job);Population::Traffic target;
    int storage=choice;
    if(mission>=0) {
        storage=-1;for(int i=0;i<Population::VehicleCapacity;++i)if(!cars[i].active){storage=i;break;}if(storage<0)return;
        const auto& v=scripted[mission];target.position={v.global.x-anchor.x,v.global.y-anchor.y,v.global.z-anchor.z};target.yaw=v.yaw;target.model=missionModel(v.model);target.health=v.health;target.mission=mission;target.missionToken=campaign.vehicleToken();target.driver=false;
    } else target=cars[choice];
    if(!job.takeVehicle(target.position,target.yaw,target.health,target.model))return;
    population.ejectDriver(game,target);cars[storage]=old;
    if(mission>=0)campaign.borrowVehicle(mission);
    job.missionVehicle=target.mission;job.missionToken=target.missionToken;job.wheelRoll=target.wheelRoll;
    if(models){const auto& model=models->model(job.carModel);job.vehicleGeometry(model.seat,model.radius,model.wheelbase);}
}
void Garage::sync(Street& job,Population& population,Campaign& campaign,Point anchor) {
    if(job.missionVehicle>=0 && !campaign.syncVehicle(job.missionVehicle,job.missionToken,job.car,anchor,job.carYaw,job.carHealth))job.missionVehicle=-1;
    for(int i=0;i<Population::VehicleCapacity;++i){auto& car=population.vehicles()[i];if(car.active && car.mission>=0 && !campaign.syncVehicle(car.mission,car.missionToken,car.position,anchor,car.yaw,car.health))car.mission=-1;}
}
bool Garage::overlap(Point a,int yawA,int modelA,Point b,int yawB,int modelB) {
    if(std::fabs(a.y-b.y)>2)return false;
    float dx=b.x-a.x,dz=b.z-a.z;if(dx*dx+dz*dz>100)return false;
    float aa=yawA/5215.189175f,ab=yawB/5215.189175f;
    float ax[2]={std::cos(aa),std::sin(aa)},az[2]={std::sin(aa),-std::cos(aa)};
    float bx[2]={std::cos(ab),std::sin(ab)},bz[2]={std::sin(ab),-std::cos(ab)};
    float wa=modelA==3 ? 1.05f : .95f,la=modelA==3 ? 3.1f : 2.4f,wb=modelB==3 ? 1.05f : .95f,lb=modelB==3 ? 3.1f : 2.4f;
    for(const float* axis:{ax,az,bx,bz}) {
        float ca=wa*std::fabs(axis[0]*ax[0]+axis[1]*ax[1])+la*std::fabs(axis[0]*az[0]+axis[1]*az[1]);
        float cb=wb*std::fabs(axis[0]*bx[0]+axis[1]*bx[1])+lb*std::fabs(axis[0]*bz[0]+axis[1]*bz[1]);
        if(std::fabs(dx*axis[0]+dz*axis[1])>=ca+cb)return false;
    }
    return true;
}
void Garage::guardDrive(Street& job,const Population& population,const Campaign& campaign,Point anchor,Point before) {
    bool blocked=false;for(int i=0;i<Population::VehicleCapacity;++i){const auto& c=population.vehicles()[i];if(c.active && overlap(job.car,job.carYaw,job.carModel,c.position,c.yaw,c.model))blocked=true;}
    for(int i=0;i<campaign.vehicleCount();++i){const auto& c=campaign.vehicles()[i];if(c.placed && !c.borrowed){Point p={c.global.x-anchor.x,c.global.y-anchor.y,c.global.z-anchor.z};if(overlap(job.car,job.carYaw,job.carModel,p,c.yaw,missionModel(c.model)))blocked=true;}}
    if(blocked)job.stopAt(before);
}
}
