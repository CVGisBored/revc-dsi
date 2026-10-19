#pragma once
#include "sector.h"
#include <cstdint>
#include <memory>

namespace slice {
class Animation {
public:
    enum Clip { Idle, Walk, Run, Fire, Reload, Sprint, StartWalk, RunStop, RunStopR, JumpLaunch, JumpGlide, JumpLand, Punch, ClipCount };
    bool open(const char* path,int maxTriangles);
    void advance(Clip next,unsigned ticks,bool restart=false,float rate=1.f);
    const RevcTriangle* mesh() const;
    const RevcTriangle* meshAt(Clip clip,unsigned ticks) const;
    const RevcTriangle* poseMesh(Clip first,float phase,Clip second,float secondPhase,float blend,bool allowLoops=true) const;
    const RevcUV* uv() const {return coordinates.get();}
    int count() const {return triangles;}
    float speed(Clip selected) const {return selected>=Idle && selected<ClipCount ? sequence[selected].speed : 0;}
    unsigned frame() const;
    void reset() {clip=Idle;clock=0;fraction=0;}
private:
    std::unique_ptr<RevcTriangle[]> data;
    struct Vertex {int16_t xyz[3];};
    struct Face {uint16_t color,index[3];};
    std::unique_ptr<RevcUV[]> coordinates;
    std::unique_ptr<Vertex[]> vertices,posed;
    std::unique_ptr<Face[]> faces;
    unsigned vertexCount=0;
    mutable unsigned cachedFrame=~0u,cachedClip=~0u;
    const RevcTriangle* decode(Clip selected,unsigned frame) const;
    struct Sequence {unsigned frames=0,duration=0,offset=0;float speed=0;} sequence[ClipCount];
    int triangles=0;
    Clip clip=Idle;
    uint32_t clock=0;
    float fraction=0;
};
}
