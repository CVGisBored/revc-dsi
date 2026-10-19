#include "population.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include "../../generated/fleet_data.h"
namespace slice {
namespace {
float distance2(Point a,Point b){float x=a.x-b.x,z=a.z-b.z;return x*x+z*z;}
Point nearest(Point p,Point a,Point b) {
    float dx=b.x-a.x,dz=b.z-a.z,len=dx*dx+dz*dz;
    float t=len>0 ? ((p.x-a.x)*dx+(p.z-a.z)*dz)/len : 0;t=std::max(0.f,std::min(1.f,t));
    return {a.x+dx*t,a.y+(b.y-a.y)*t,a.z+dz*t};
}
int heading(float x,float z){return int(std::atan2(x,-z)*5215.189175f)&32767;}
}
unsigned Population::nextRandom(){random=random*1664525u+1013904223u;return random;}
bool Population::open(const char* root) {
    close();char path[512],magic[4];uint32_t count=0;
    std::snprintf(path,sizeof(path),"%s/world/paths.vcp",root);paths=std::fopen(path,"rb");if(!paths)return false;
    bool good=std::fread(magic,1,4,paths)==4 && !std::memcmp(magic,"VPT2",4) &&
        std::fread(&count,4,1,paths)==1 && count>0 && count<=2048 && std::fread(index,sizeof(Index),count,paths)==count;
    if(!good){close();return false;}indexCount=count;
    uint64_t end=8+count*sizeof(Index);
    for(int i=0;i<indexCount;++i) {
        if(index[i].offset!=end || !index[i].count){close();return false;}
        end+=index[i].count*sizeof(Edge);
    }
    if(std::fseek(paths,0,SEEK_END) || std::ftell(paths)!=long(end)){close();return false;}
    std::snprintf(path,sizeof(path),"%s/world/traffic.vcm",root);FILE* f=std::fopen(path,"rb");if(!f){close();return false;}
    good=std::fread(magic,1,4,f)==4 && !std::memcmp(magic,"VCM1",4) && std::fread(&count,4,1,f)==1 && count>0 && count<=420 &&
        std::fread(model,sizeof(RevcTriangle),count,f)==count && std::fgetc(f)==EOF;
    std::fclose(f);if(!good){close();return false;}modelCount=count;
    std::snprintf(path,sizeof(path),"%s/world/traffic.vcu",root);f=std::fopen(path,"rb");if(!f){close();return false;}
    good=std::fread(magic,1,4,f)==4 && !std::memcmp(magic,"VCU1",4) && std::fread(&count,4,1,f)==1 && count==unsigned(modelCount) &&
        std::fread(modelCoordinates,sizeof(RevcUV),count,f)==count && std::fgetc(f)==EOF;
    std::fclose(f);if(!good){close();return false;}
    std::snprintf(path,sizeof(path),"%s/world/traffic-lod.vcm",root);f=std::fopen(path,"rb");if(!f){close();return false;}
    good=std::fread(magic,1,4,f)==4 && !std::memcmp(magic,"VCM1",4) && std::fread(&count,4,1,f)==1 && count>0 && count<=160 &&
        std::fread(distantModel,sizeof(RevcTriangle),count,f)==count && std::fgetc(f)==EOF;
    std::fclose(f);if(!good){close();return false;}distantCount=count;return true;
}
void Population::reset(){loaded=pending=false;pathBytes=0;for(auto& p:peds)p=Ped{};for(auto& car:cars)car=Traffic{};spawnClock=0;simulationTurn=0;trafficLimit=TrafficCount;obstacleCount=0;random=0x613;}
void Population::close(){if(paths)std::fclose(paths);paths=nullptr;indexCount=edgeCount=modelCount=0;reset();}
void Population::rebase(Point shift) {
    auto apply=[shift](Point& p){p.x-=shift.x;p.y-=shift.y;p.z-=shift.z;};
    for(auto& p:peds)apply(p.position);
    for(auto& car:cars)apply(car.position);
    for(int i=0;i<edgeCount;++i){apply(edges[i].a);apply(edges[i].b);}
    origin.x+=shift.x;origin.y+=shift.y;origin.z+=shift.z;
}
bool Population::refresh(Game& game,Point anchor,Point missionCar,bool incremental,bool allowWork) {
    pathBytes=0;if(!paths)return false;
    if(!allowWork && loaded)return true;
    Point global={game.player.x+anchor.x,game.player.y+anchor.y,game.player.z+anchor.z};
    if(!pending) {
        if(loaded && distance2(global,lastLoad)<64)return true;
        pending=true;pendingCount=0;pathRegion=0;pathRecord=0;request=global;
    }
    global=request;
    int x=int(std::floor(global.x/64)),z=int(std::floor(global.z/64));
    while(pathRegion<indexCount) {
        const auto& block=index[pathRegion];if(std::abs(block.x-x)>1 || std::abs(block.z-z)>1){++pathRegion;pathRecord=0;continue;}
        Edge buffer[64];unsigned amount=std::min<unsigned>(64u,block.count-pathRecord);
        if(std::fseek(paths,block.offset+pathRecord*sizeof(Edge),SEEK_SET) ||
           std::fread(buffer,sizeof(Edge),amount,paths)!=amount)return false;
        pathBytes+=amount*sizeof(Edge);pathRecord+=amount;
        for(unsigned n=0;n<amount;++n) {
            Edge edge=buffer[n];if(edge.kind>1)return false;
            float score=distance2(global,nearest(global,edge.a,edge.b));if(score>32*32)continue;
            int slot=pendingCount;
            if(slot==128){slot=int(std::max_element(pendingScores,pendingScores+128)-pendingScores);if(score>=pendingScores[slot])continue;}
            else ++pendingCount;
            pendingEdges[slot]=edge;pendingScores[slot]=score;
        }
        if(pathRecord==block.count){++pathRegion;pathRecord=0;}
        if(incremental && loaded)return true;
    }
    origin=anchor;edgeCount=pendingCount;
    for(int i=0;i<edgeCount;++i) {
        edges[i]=pendingEdges[i];auto& edge=edges[i];
        edge.a={edge.a.x-origin.x,edge.a.y-origin.y,edge.a.z-origin.z};
        edge.b={edge.b.x-origin.x,edge.b.y-origin.y,edge.b.z-origin.z};
    }
    pending=false;
    for(auto& p:peds) {
        if(!p.active)continue;
        p.edge=-1;float best=4;
        for(int i=0;i<edgeCount;++i)if(!edges[i].kind) {
            Point route=nearest(p.position,edges[i].a,edges[i].b);if(std::fabs(route.y-p.position.y)>2)continue;
            float d=distance2(p.position,route);
            if(d<best){best=d;p.edge=i;}
        }
        if(p.edge<0 || distance2(p.position,game.player)>18*18)p.active=false;
    }
    for(auto& car:cars)if(car.active && !car.parked){car.edge=-1;float best=4;for(int i=0;i<edgeCount;++i)if(edges[i].kind) {
        Point route=nearest(car.position,edges[i].a,edges[i].b);if(std::fabs(route.y-car.position.y)>2)continue;
        float d=distance2(car.position,route);if(d<best){best=d;car.edge=i;}
    }if(car.edge<0 || distance2(car.position,game.player)>34*34)car.active=false;
    else {const auto& edge=edges[car.edge];float a=car.yaw/5215.189175f;car.reverse=(edge.b.x-edge.a.x)*std::sin(a)-(edge.b.z-edge.a.z)*std::cos(a)<0;}}

    loaded=true;lastLoad=global;spawn(game,missionCar);return true;
}
bool Population::spawn(Game& game,Point missionCar) {
    for(int who=0;who<PedCount;++who)if(!peds[who].active) {
        for(int attempt=0;attempt<edgeCount;++attempt) {
            int i=int((nextRandom()>>8)%unsigned(edgeCount));const auto& edge=edges[i];if(edge.kind)continue;
            float t=(nextRandom()%800+100)/1000.f;
            Point p={edge.a.x+(edge.b.x-edge.a.x)*t,edge.a.y+(edge.b.y-edge.a.y)*t,edge.a.z+(edge.b.z-edge.a.z)*t};
            float d=distance2(p,game.player);if(d<25 || d>14*14 || distance2(p,missionCar)<16 || !game.placeActor(p))continue;
            bool crowded=false;for(const auto& other:peds)if(other.active && distance2(p,other.position)<4)crowded=true;
            if(crowded)continue;
            auto& ped=peds[who];ped=Ped{};ped.active=true;ped.position=p;ped.edge=i;ped.reverse=nextRandom()&1;ped.clock=nextRandom()%92;break;
        }
    }
    for(int slot=0;slot<trafficLimit;++slot)if(!cars[slot].active)for(int attempt=0;attempt<edgeCount*2;++attempt) {
        const int i=int((nextRandom()>>8)%unsigned(edgeCount));if(!edges[i].kind)continue;
        auto& car=cars[slot];
        float t=(nextRandom()%900+50)/1000.f;
        Point p={edges[i].a.x+(edges[i].b.x-edges[i].a.x)*t,edges[i].a.y+(edges[i].b.y-edges[i].a.y)*t,edges[i].a.z+(edges[i].b.z-edges[i].a.z)*t};float d=distance2(p,game.player);
        if(d<64 || d>23*23 || distance2(p,missionCar)<36 || !game.placeActor(p))continue;
        bool occupied=false;for(const auto& other:cars)if(other.active && distance2(p,other.position)<49)occupied=true;
        if(occupied)continue;
        float dx=edges[i].b.x-edges[i].a.x,dz=edges[i].b.z-edges[i].a.z,length=std::sqrt(dx*dx+dz*dz);
        Point tilt;if(length<.1f || !game.vehiclePose(p,dx/length,-dz/length,&tilt))continue;
        car=Traffic{};car.position=p;car.tilt=tilt;car.edge=i;car.yaw=heading(dx,dz);car.active=true;car.model=slot%4;break;
    }
    return true;
}
int Population::successor(int edge,bool& reverse) const {
    Point end=reverse ? edges[edge].a : edges[edge].b;
    int result=-1;float best=.3f;
    for(int i=0;i<edgeCount;++i)if(i!=edge && edges[i].kind==edges[edge].kind) {
        float a=distance2(end,edges[i].a)+(end.y-edges[i].a.y)*(end.y-edges[i].a.y);
        float b=distance2(end,edges[i].b)+(end.y-edges[i].b.y)*(end.y-edges[i].b.y);
        if(a<best){best=a;result=i;reverse=false;}if(b<best){best=b;result=i;reverse=true;}
    }
    if(result<0){reverse=!reverse;return edge;}return result;
}
void Population::tick(Game& game,Point missionCar,int alert,int frames) {
    if(game.state!=Game::Playing || !loaded)return;
    frames=std::max(0,std::min(4,frames));
    spawnClock+=frames;if(spawnClock>=120){spawnClock=0;spawn(game,missionCar);}
    for(auto& car:cars)if(car.active && car.parked && car.mission<0 && distance2(car.position,game.player)>48*48)car.active=false;
    const int turn=int(simulationTurn++%PedCount);
    for(int index=0;index<PedCount;++index) {
        auto& p=peds[index];if(!p.active)continue;
        p.clock+=frames;p.moving=false;
        if(distance2(p.position,game.player)>18*18){p.active=false;continue;}
        const Traffic* threatCar=nullptr;float threatDistance=16;
        for(const auto& other:cars)if(other.active){float d=distance2(p.position,other.position);if(d<threatDistance){threatDistance=d;threatCar=&other;}}
        if(alert && distance2(p.position,game.player)<12*12){p.flee=300;p.fleeCar=false;p.wait=0;}
        else if(threatCar){p.flee=90;p.fleeCar=true;p.wait=0;}
        p.flee=std::max(0,p.flee-frames);p.wait=std::max(0,p.wait-frames);
        if(p.wait || p.edge<0)continue;
        if(index!=turn){p.moving=true;continue;}
        Point target=p.reverse ? edges[p.edge].a : edges[p.edge].b;
        float dx=target.x-p.position.x,dz=target.z-p.position.z;
        if(p.flee){Point threat=p.fleeCar && threatCar ? threatCar->position : game.player;dx=p.position.x-threat.x;dz=p.position.z-threat.z;}
        float length=std::sqrt(dx*dx+dz*dz);
        if(length<.35f){p.edge=successor(p.edge,p.reverse);p.wait=int(nextRandom()%90);continue;}
        float speed=(p.flee ? runSpeed[index%2] : walkSpeed[index%2])*frames*PedCount/length;
        Point candidate=p.position;
        bool moved=game.actorMove(candidate,dx*speed,dz*speed),crowded=false;
        for(int other=0;other<PedCount;++other)if(other!=index && peds[other].active && distance2(candidate,peds[other].position)<.49f)crowded=true;
        if(moved && !crowded && !trafficBlocks(candidate) && distance2(candidate,missionCar)>10 && distance2(candidate,game.player)>.81f) {
            p.position=candidate;p.moving=true;p.yaw=heading(dx,dz);
        }else{p.reverse=!p.reverse;p.wait=30;}
    }
    for(auto& car:cars)if(car.active && !car.parked && car.edge>=0 && car.health>0) {
        if(distance2(car.position,game.player)>34*34){car.active=false;continue;}
        Point target=car.reverse ? edges[car.edge].a : edges[car.edge].b;
        float dx=target.x-car.position.x,dz=target.z-car.position.z,length=std::sqrt(dx*dx+dz*dz);
        if(length<1.1f){car.edge=successor(car.edge,car.reverse);continue;}
        int wanted=heading(dx,dz),difference=((wanted-car.yaw+16384)&32767)-16384;
        int turn=std::max(-160*frames,std::min(160*frames,difference));
        int direction=(car.yaw+turn)&32767;float angle=direction/5215.189175f,si=std::sin(angle),co=std::cos(angle);
        car.steering+=(std::max(-.5f,std::min(.5f,difference/5215.189175f))-car.steering)*.25f;
        bool stop=distance2(car.position,game.player)<25 || distance2(car.position,missionCar)<36;
        for(int i=0;i<obstacleCount;++i)if(distance2(car.position,obstaclePoints[i])<36)stop=true;
        for(const auto& other:cars)if(&other!=&car && other.active && distance2(other.position,car.position)<49) {
            float x=other.position.x-car.position.x,z=other.position.z-car.position.z;
            if(x*si-z*co>0 && std::fabs(x*co+z*si)<2.4f)stop=true;
        }
        for(const auto& ped:peds)if(ped.active && distance2(ped.position,car.position)<9)stop=true;
        car.speed=stop ? std::max(0.f,car.speed-.01f*frames) : std::min(.10f,car.speed+.0015f*frames);
        Point before=car.position;
        if(car.speed && game.vehicleActorMove(car.position,si*car.speed*frames,-co*car.speed*frames,si,co,&car.tilt)) {
            car.yaw=direction;car.wheelRoll=std::fmod(car.wheelRoll-std::sqrt(distance2(before,car.position))/fleetData::models[car.model%4].radius,6.2831853f);
        } else car.speed=0;
    }
}
int Population::activePeds() const {int count=0;for(const auto& p:peds)count+=p.active;return count;}
bool Population::blocks(Point point) const {
    for(const auto& p:peds)if(p.active && std::fabs(point.y-p.position.y)<1.8f && distance2(point,p.position)<.49f)return true;
    return trafficBlocks(point);
}
bool Population::trafficBlocks(Point point) const {
    for(const auto& car:cars)if(car.active && std::fabs(point.y-car.position.y)<2.4f) {
        float angle=car.yaw/5215.189175f,s=std::sin(angle),c=std::cos(angle);
        float x=point.x-car.position.x,z=point.z-car.position.z;
        if(std::fabs(x*c+z*s)<1.3f && std::fabs(x*s-z*c)<2.8f)return true;
    }
    return false;
}
}

namespace slice {
int Population::activeCars() const {int n=0;for(const auto& c:cars)n+=c.active;return n;}
int Population::storeParked(const Traffic& value) {for(int i=0;i<VehicleCapacity;++i)if(!cars[i].active){cars[i]=value;cars[i].active=true;cars[i].parked=true;cars[i].driver=false;cars[i].speed=0;return i;}return -1;}
void Population::ejectDriver(Game& game,const Traffic& car) {
    if(!car.driver)return;
    float a=car.yaw/5215.189175f,si=std::sin(a),co=std::cos(a);
    for(float side:{-1.f,1.f}) {
        Point p={car.position.x+side*1.9f*co,car.position.y,car.position.z+side*1.9f*si};
        if(!game.actorMove(p,0,0))continue;
        int slot=0;for(int i=0;i<PedCount;++i)if(!peds[i].active || distance2(peds[i].position,game.player)>distance2(peds[slot].position,game.player))slot=i;
        auto& ped=peds[slot];ped=Ped{};ped.active=true;ped.position=p;ped.flee=300;ped.edge=0;ped.moving=true;return;
    }
}
}
