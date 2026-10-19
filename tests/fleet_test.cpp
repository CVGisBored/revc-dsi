#include "garage.h"
#include "textures.h"
#include <cassert>
#include <cmath>
#include <cstring>
#include <string>
#include <vector>
#include <unistd.h>
using namespace slice;
static RevcTriangle tri(Point a,Point b,Point c){RevcTriangle t{};Point p[]={a,b,c};for(int i=0;i<3;++i){t.vertex[i][0]=p[i].x*64;t.vertex[i][1]=p[i].y*64;t.vertex[i][2]=p[i].z*64;}return t;}
int main(int argc,char** argv){
    assert(argc==2);VehicleModels models;assert(models.open(argv[1]));VehicleAnimation animator;
    for(int id=0;id<4;++id){const auto& m=models.model(id);assert(m.detail.count>600 && m.far.count>50 && m.far.count<160);assert(m.radius>.3f && m.radius<.4f && m.seat.x<0);
        for(const auto* mesh:{&m.detail,&m.far}){assert(!std::memcmp(animator.pose(mesh->rig,mesh->triangles.get(),mesh->count,0,0,0,-1),mesh->triangles.get(),mesh->count*sizeof(RevcTriangle)));}
        // Each front door must swing away from the cabin using its original hinge.
        for(int side:{-1,1}){int part=side<0 ? 5 : 6;auto posed=animator.pose(m.detail.rig,m.detail.triangles.get(),m.detail.count,0,0,1.1f,side);float before=0,after=0;int n=0;
            for(int i=0;i<m.detail.count;++i)if(m.detail.rig.part[i]==part)for(int j=0;j<3;++j){before+=side*m.detail.triangles[i].vertex[j][0];after+=side*posed[i].vertex[j][0];++n;}assert(n && after>before+n*8);
        }
    }
    RevcTriangle floor[]={tri({-100,0,-100},{100,0,-100},{100,0,100}),tri({-100,0,-100},{100,0,100},{-100,0,100})};
    Game game;Street job;Population population;Campaign campaign;assert(game.startExploration(floor,2,{0,0,0}));assert(job.start(game,true));campaign.reset();
    for(int model=0;model<4;++model){job.reset();population.reset();auto original=job.car;Point target={20,0,20};auto& car=population.vehicles()[0];car.active=true;car.driver=true;car.position=target;car.model=model;car.health=73;car.wheelRoll=1.7f;game.player={21.9f,0,20};
        Garage::interact(game,job,population,campaign,{},&models);assert(job.inCar && job.carModel==model && job.carHealth==73 && job.transitioning());assert(std::fabs(job.wheelRoll-1.7f)<.001f);assert(car.parked && !car.driver && car.position.x==original.x && car.position.z==original.z);assert(population.activePeds()==1);
        job.drive(1,false,0,1,4);assert(job.speed==0);job.tick(90);auto seat=job.driverPosition();assert(seat.x<job.car.x);assert(std::fabs(seat.y-models.model(model).seat.y)<.001f);
        job.interact();assert(!job.inCar);job.tick(90);Garage::interact(game,job,population,campaign,{},&models);assert(job.inCar && job.carModel==model);job.tick(90);job.carHealth=0;job.tick(1);assert(!job.inCar && job.phase!=Street::Failed);job.tick(90);
        // The old starter remains usable after abandoning the wreck.
        game.player={car.position.x+1.9f,0,car.position.z};Garage::interact(game,job,population,campaign,{},&models);assert(job.inCar && job.carHealth==100 && job.carModel==0);
    }
    assert(Garage::overlap({0,0,0},0,0,{0,0,4},0,0));assert(!Garage::overlap({0,0,0},0,0,{0,0,5},0,0));assert(!Garage::overlap({0,0,0},0,0,{0,4,0},0,0));
    // VTX2 aliases must share one decoded page, and eviction must preserve bytes.
    char path[]="/tmp/revc-texture-cache-XXXXXX";int fd=mkstemp(path);assert(fd>=0);FILE* f=fdopen(fd,"wb");assert(f);uint32_t count=11,unique=10,start=12+count*4;fwrite("VTX2",4,1,f);fwrite(&count,4,1,f);fwrite(&unique,4,1,f);for(unsigned i=0;i<count;++i){uint32_t offset=start+(i==10 ? 0 : i)*sizeof(TexturePage);fwrite(&offset,4,1,f);}for(unsigned i=0;i<unique;++i){TexturePage p{};p.palette[0]=i;p.pixels[0]=i+10;fwrite(&p,sizeof(p),1,f);}fclose(f);
    TextureFile textures;assert(textures.open(path));TexturePage page;assert(textures.load(1,page) && textures.diskReads()==1);assert(textures.load(11,page) && textures.diskReads()==1 && textures.cacheHits()==1);for(int i=2;i<=10;++i)assert(textures.load(i,page) && page.palette[0]==i-1);assert(textures.load(1,page) && page.pixels[0]==10 && textures.diskReads()==11);assert(!textures.load(0,page) && !textures.load(12,page));textures.close();unlink(path);
    puts("Four fleet meshes, outward doors, hijacking, driver seats, wreck escape, retained cars, contacts and texture alias/LRU cache passed");
}
