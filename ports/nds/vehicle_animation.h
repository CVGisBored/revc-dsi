#pragma once
#include "sector.h"
#include "gameplay.h"
#include <cstdint>
namespace slice {
// Seven rigid parts share the existing UV atlas and polygon allocation.
class VehicleAnimation {
public:
    struct Rig {int count=0;int16_t pivot[7][3]{};uint8_t part[640]{};};
    static bool loadRig(const char* path,int count,Rig& rig);
    bool open(const char* path,int count);
    const RevcTriangle* pose(const RevcTriangle* source,int count,float wheelRoll,float steering,float door,int side,Point bodyTilt={});
    const RevcTriangle* pose(const Rig& rig,const RevcTriangle* source,int count,float wheelRoll,float steering,float door,int side,Point bodyTilt={});
private:
    Rig own;
    RevcTriangle result[640]{};
};
}
