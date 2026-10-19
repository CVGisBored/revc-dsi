#pragma once
#include "gameplay.h"
#include <cstdio>
#include <memory>

namespace slice {
class World {
public:
    static constexpr int Capacity=2048, MapBudget=900, MaxChunks=16384;
    static constexpr int TerrainCapacity=768, DecorationCapacity=Capacity-TerrainCapacity;
    static constexpr int CollisionCapacity=4096, MaxCollisionChunks=8192;
    struct Material {uint16_t texture=0;int16_t uv[3][2]{};};
    bool open(const char* root);
    int openError() const {return openStage;}
    void close();
    bool update(Point player,Point& shift,bool force=false,bool incremental=false,bool driving=false);
    // Prioritized collision work publishes independently of decorative snapshots.
    bool updateCollision(Point player,Point lookahead,unsigned readLimit=16);
    bool urgentCollisionRefresh() const {return collisionUrgent;}
    unsigned urgentCollisionRefreshes() const {return urgentRefreshes;}
    unsigned skippedCollisionBlocks() const {return collisionBlocksSkipped;}
    unsigned terrainCacheHits() const {return terrainHits;}
    unsigned collisionCacheHits() const {return collisionHits;}
    unsigned publishedPatches() const {return patchPublishes;}
    unsigned publishedSceneryPatches() const {return sceneryPublishes;}
    unsigned reusedPatches() const {return patchReuses;}
    void streamUnitLimit(unsigned units) {unitLimit=units;}
    // Optional larger working set uses the extra RAM available in DSi mode.
    bool enableExtendedCache();
    float visualLag(Point local) const;
    int groundFallback(Point focus,int budget=128);
    const RevcTriangle& fallbackTriangle(int i) const {return collision[fallbackFaces[i]];}
    int visible(Point camera,float sine,float cosine,float pitchSine,float pitchCosine,const Point* focus=nullptr);
    const RevcTriangle* triangles() const {return collision;}
    int count() const {return collisionCount;}
    int renderCount() const {return residentCount;}
    int visibleTileCount() const {return selectedTileCount;}
    int visibleTileTriangles(int i) const {return selectedTileCosts[i];}
    const RevcTriangle& visibleTriangle(int i) const {return resident[drawList[i].index];}
    const Material& visibleMaterial(int i) const {return material[drawList[i].index];}
    bool textured() const {return rich;}
    bool originalCollision() const {return collisionFile!=nullptr;}
    const RevcTriangle* playerMesh() const {return ped;}
    int playerCount() const {return pedCount;}
    const RevcTriangle* vehicleMesh() const {return vehicle;}
    const RevcTriangle* guardMesh() const {return guard;}
    const RevcTriangle* weaponMesh() const {return weapon;}
    const RevcTriangle* vehicleLOD() const {return vehicleDistant;}
    int vehicleLODCount() const {return vehicleDistantCount;}
    int vehicleCount() const {return vehicleCountValue;}
    int guardCount() const {return guardCountValue;}
    int weaponCount() const {return weaponCountValue;}
    int chunkCount() const {return indexCount;}
    int dropped() const {return droppedCount;}
    float fogEnd() const {return fogDistance;}
    Point anchor() const {return origin;}
    unsigned revision() const {return version;}
    unsigned collisionRevision() const {return collisionVersion;}
    bool streaming() const {return stream.active;}
    void cancelStreaming() {stream.active=false;collisionWork.active=false;pendingCollision.reset();}
    unsigned lastStreamUnits() const {return streamUnits;}
    uint32_t lastReadBytes() const {return readBytes+collisionReadBytes;}
    uint32_t lastCollisionReadBytes() const {return collisionReadBytes;}
    bool collisionStreaming() const {return collisionWork.active;}
    void renderBudget(int limit) {drawBudget=limit<128 ? 128 : limit>MapBudget ? MapBudget : limit;}
    int renderBudget() const {return drawBudget ? drawBudget : MapBudget;}
    const RevcUV* vehicleUV() const {return vehicleCoordinates;}
private:
    RevcUV vehicleCoordinates[600]{};
    struct Index {int16_t x,y,z,pad; uint32_t offset,count;};
    struct Candidate {RevcTriangle triangle;Material material;uint32_t score;};
    struct Draw {int index;int depth;};
    std::unique_ptr<Index[]> index;
    Candidate staging[Capacity]{};
    RevcTriangle resident[Capacity]{},ped[550]{};
    Material material[Capacity]{};
    RevcTriangle collision[CollisionCapacity]{};
    std::unique_ptr<Index[]> collisionIndex,terrainIndex;
    std::unique_ptr<unsigned char[]> packedTerrain,rawTerrain;
    uint32_t packedTerrainSize=0,rawTerrainSize=0;
    unsigned char excludedTextures[4096]{};
    std::unique_ptr<uint32_t[]> excludedGeometry;
    uint32_t excludedGeometryCount=0;
    bool backdrop(const RevcTriangle& triangle,int x,int y,int z) const;
    FILE* collisionFile=nullptr;
    FILE* terrainFile=nullptr;
    int collisionIndexCount=0;
    int terrainIndexCount=0;
    Point lastCollisionLoad{};
    Point lastCollisionRequest{};
    struct CollisionBounds {uint8_t low[3],high[3];};
    std::unique_ptr<CollisionBounds[]> collisionBounds;
    std::unique_ptr<uint32_t[]> collisionBlockStarts;
    bool collisionBlockNearby(int entry,unsigned record,const int* offsets,const int* actor) const;
    unsigned collisionBlocksSkipped=0;
    bool readCollision(unsigned address,unsigned count,RevcTriangle* output,uint32_t& bytes);
    struct ReadCache {uint32_t offset=0,age=0,size=0,kind=0;unsigned char bytes[8192];};
    std::unique_ptr<ReadCache[]> extendedCache;
    bool readExtended(FILE* file,unsigned kind,unsigned address,unsigned size,unsigned char* output,uint32_t& bytes);
    bool readDirectory(FILE* file,unsigned kind,unsigned address,unsigned size,void* output);
    unsigned collisionHits=0;
    bool independentCollision=false;
    uint32_t collisionReadBytes=0;
    bool collisionUrgent=false;unsigned urgentRefreshes=0;
    struct CollisionWork {bool active=false;Point global{},anchor{},requested{};int entry=0,count=0;unsigned record=0;} collisionWork;
    // Pending geometry never replaces the active collision/render snapshot until complete.
    std::unique_ptr<RevcTriangle[]> pendingCollision;
    struct Stream {
        bool active=false,needCollision=false,heap=false,patches=false;
        int phase=0,entry=0,tile=0,region=0,count=0,total=0,base=0,ground=0,collisions=0;
        unsigned record=0,decoded=0,consumed=0,packedRead=0;
        uint32_t sizes[512]{},starts[512]{};
        uint16_t tileOrder[512]{};unsigned tileCount=0;
        uint16_t regions[64]{};unsigned regionCount=0,regionCursor=0,groundSlice=0;
        Point global{},anchor{},local{};
    } stream;
    Stream groundWork{},sceneryWork{};
    bool groundSuspended=false,sceneryStarted=false,sceneryDone=false;
    uint32_t residentSources[Capacity]{};
    struct Patch {uint32_t key=0,age=0;};Patch patches[128]{};
    uint32_t patchClock=0;unsigned patchPublishes=0,sceneryPublishes=0,patchReuses=0,unitLimit=0;
    bool patchesDirty=false;
    bool patchResident(uint32_t key);
    void publishPatch(Candidate* faces,int count,uint32_t key);
    static uint32_t patchKey(int x,int y,int z,bool ground);
    void nextRegion();
    unsigned collisionVersion=0,streamUnits=0;
    bool streamStep(unsigned readSize=2048);
    void beginPhase(int phase);
    RevcTriangle vehicleDistant[160]{};int vehicleDistantCount=0;
    RevcTriangle vehicle[600]{},guard[150]{},weapon[60]{};
    int vehicleCountValue=0,guardCountValue=0,weaponCountValue=0;
    Draw drawList[MapBudget]{};
    uint8_t faceInView[Capacity]{},walkableFace[Capacity]{};
    uint16_t fallbackFaces[128]{};unsigned fallbackVersion=0;int fallbackKey[4]{},fallbackCount=0;
    struct RenderTile {int16_t low[3]{},high[3]{};uint16_t count=0;uint32_t key=0,priority=0;bool selected=false;};
    static constexpr unsigned RenderTiles=512;
    RenderTile tiles[RenderTiles]{};
    uint16_t faceTile[Capacity]{},renderOrder[Capacity]{},selectedTiles[RenderTiles]{},selectedTileCosts[RenderTiles]{};
    int selectedTileCount=0;
    void buildRenderTiles();
    int indexCount=0,residentCount=0,collisionCount=0,pedCount=0,droppedCount=0;
    FILE* chunks=nullptr;
    Point origin{},lastLoad{};
    bool loaded=false;
    bool rich=false;
    float fogDistance=0;
    unsigned version=0;
    uint32_t readBytes=0;
    struct TileCache {uint32_t offset=0,age=0;unsigned char bytes[128*25]{};};
    TileCache cache[96]{};
    uint32_t cacheClock=0;
    struct TerrainCache {uint32_t offset=0,age=0,size=0;unsigned char bytes[2048]{};};
    TerrainCache terrainCache[16]{};
    unsigned terrainHits=0;
    int openStage=0;
    int drawBudget=0;
    int viewKey[12]{};unsigned viewVersion=0;int cachedVisible=0;
};
}
