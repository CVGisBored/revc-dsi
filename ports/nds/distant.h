#pragma once
#include "world.h"
namespace slice {
class DistantWorld {
public:
    static constexpr int Capacity=1536,Budget=900,MaxObjects=64;
    bool open(const char* path);
    bool update(Point global,Point anchor,bool incremental=false,bool allowWork=true);
    bool streaming() const {return pending;}
    int visible(Point camera,float sine,float cosine,float ps,float pc,Point focus,int budget=Budget);
    const RevcTriangle& triangle(int i) const {return resident[list[i]].triangle;}
    const World::Material& material(int i) const {return resident[list[i]].material;}
    unsigned readBytes() const {return bytes;}
    unsigned reusedObjects() const {return reused;}
    unsigned snapshotRevision() const {return committed;}
    Point centre() const {return last;}
    int visibleObjectCount() const {return visibleGroups;}
    int visibleObjectTriangles(int i) const {return drawCosts[i];}
    ~DistantWorld(){if(file)std::fclose(file);}
private:
    struct Index {int16_t anchor[3],low[3],high[3];uint16_t faces;uint32_t offset,size;};
    struct Entry {RevcTriangle triangle;World::Material material;};
    struct Group {uint16_t first,count,id;int16_t low[3],high[3];};
    struct Candidate {uint16_t id;uint32_t score;};
    FILE* file=nullptr;
    std::unique_ptr<Index[]> index;unsigned objects=0,count=0,bytes=0,reused=0;
    Entry resident[Capacity]{},staging[Capacity]{};
    Group groups[MaxObjects]{},pendingGroups[MaxObjects]{};unsigned groupCount=0,pendingGroupCount=0;
    uint16_t list[Budget]{},drawGroups[MaxObjects]{},drawCosts[MaxObjects]{};int visibleGroups=0;
    Candidate selected[MaxObjects]{};unsigned selections=0,cursor=0,packedRead=0,decoded=0,record=0;
    bool pending=false;unsigned pendingCount=0;
    Point request{},origin{},last{};bool loaded=false;
    std::unique_ptr<unsigned char[]> packed,raw;unsigned packedSize=0,rawSize=0;
    int viewKey[11]{};unsigned revision=0,viewRevision=0,committed=0;int cachedCount=0;
    void select(Point global);
    bool step();
};
}
