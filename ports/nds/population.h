#pragma once
#include "gameplay.h"
#include <cstdio>
namespace slice {
class Population {
public:
    static constexpr int PedCount=3;
    static constexpr int TrafficCount=6,VehicleCapacity=10;
    struct Ped {Point position{};int edge=-1,yaw=0;unsigned clock=0;int wait=0,flee=0;bool active=false,reverse=false,moving=false,fleeCar=false;};
    struct Traffic {Point position{},tilt{};int edge=-1,yaw=0,model=1,health=100,mission=-1;unsigned missionToken=0;bool active=false,reverse=false,parked=false,driver=true;float speed=0,wheelRoll=0,steering=0;};
    bool open(const char* root);
    void close();
    void reset();
    void rebase(Point shift);
    bool refresh(Game& game,Point anchor,Point missionCar,bool incremental=false,bool allowWork=true);
    unsigned readBytes() const {return pathBytes;}
    bool streaming() const {return pending;}
    void tick(Game& game,Point missionCar,int alert,int frames);
    const Ped* pedestrians() const {return peds;}
    const Traffic& vehicle() const {return cars[0];}
    Traffic* vehicles() {return cars;}
    const Traffic* vehicles() const {return cars;}
    int activeCars() const;
    int storeParked(const Traffic& car);
    void ejectDriver(Game& game,const Traffic& car);
    void trafficBudget(int limit) {trafficLimit=limit<0 ? 0 : limit>TrafficCount ? TrafficCount : limit;}
    void obstacles(const Point* points,int n) {obstacleCount=n>3 ? 3 : n;for(int i=0;i<obstacleCount;++i)obstaclePoints[i]=points[i];}
    const RevcTriangle* trafficMesh() const {return model;}
    const RevcTriangle* trafficLOD() const {return distantModel;}
    int trafficLODCount() const {return distantCount;}
    int trafficTriangles() const {return modelCount;}
    int activePeds() const;
    bool blocks(Point p) const;
    void gaits(float maleWalk,float femaleWalk,float maleRun,float femaleRun) {if(maleWalk>0)walkSpeed[0]=maleWalk/60;if(femaleWalk>0)walkSpeed[1]=femaleWalk/60;if(maleRun>0)runSpeed[0]=maleRun/60;if(femaleRun>0)runSpeed[1]=femaleRun/60;}
    ~Population(){close();}
    const RevcUV* trafficUV() const {return modelCoordinates;}
private:
    RevcUV modelCoordinates[420]{};
    struct Index {int16_t x,z;uint32_t offset,count;};
    struct Edge {uint32_t kind;Point a,b;};
    Index index[2048]{};int indexCount=0;
    Edge edges[128]{};int edgeCount=0;
    Edge pendingEdges[128]{};float pendingScores[128]{};
    bool pending=false;int pendingCount=0,pathRegion=0;unsigned pathRecord=0,pathBytes=0;
    Point request{};
    FILE* paths=nullptr;
    Ped peds[PedCount]{};Traffic cars[VehicleCapacity]{};
    RevcTriangle distantModel[160]{};int distantCount=0;
    RevcTriangle model[420]{};int modelCount=0;
    Point origin{},lastLoad{};bool loaded=false;
    unsigned random=0x613;
    unsigned spawnClock=0;
    unsigned simulationTurn=0;
    int trafficLimit=TrafficCount,obstacleCount=0;Point obstaclePoints[3]{};
    float walkSpeed[2]={1.1324831f/60,1.2463688f/60},runSpeed[2]={6.3924756f/60,6.1087165f/60};
    unsigned nextRandom();
    bool spawn(Game& game,Point missionCar);
    int successor(int edge,bool& reverse) const;
    bool trafficBlocks(Point p) const;
};
}
