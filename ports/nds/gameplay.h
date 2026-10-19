#pragma once
#include "sector.h"

// Small, independent gameplay slice. The upstream CGame loop is not running yet.
namespace slice {
struct Point { float x, y, z; };
class Game {
public:
    enum State { LoadingFailed, Playing, Paused, Won, TimedOut };
    bool init(const RevcTriangle* triangles, int count,const Point* preferred=nullptr);
    bool startExploration(const RevcTriangle* triangles,int count,Point preferred);
    void restart();
    bool move(float dx, float dz);
    bool movePlayer(float dx,float dz,float seconds,float impulse=0);
    bool playerGrounded() const {return grounded;}
    bool collisionIndexed() const {return indexed;}
    void resetPlayerPhysics() {verticalSpeed=0;grounded=true;}
    float playerVerticalSpeed() const {return verticalSpeed;}
    bool actorMove(Point& p,float dx,float dz) const;
    bool placeActor(Point& p,float tolerance=3.f) const;
    bool vehicleActorMove(Point& p,float dx,float dz,float sine,float cosine,Point* tilt=nullptr) const;
    bool vehiclePose(Point& p,float sine,float cosine,Point* tilt=nullptr) const;
    bool vehicleMove(float dx,float dz,float sine,float cosine,Point* tilt=nullptr);
    bool lineClear(Point a,Point b) const;
    Point cameraPosition(Point focus,Point desired) const;
    void tick(int elapsedFrames = 1);
    void pause();
    void bindGeometry(const RevcTriangle* triangles,int count);
    void rebase(Point shift);
    void explore(bool enabled);
    bool exploring=false;
    Point player{}, checkpoint{};
    State state = LoadingFailed;
    int collected = 0, framesLeft = 0;
    // Exposed for a host traversal test using exactly the runtime collision path.
    int routeLength() const { return routeCount; }
    Point routePoint(int i) const { return route[i]; }
private:
    float verticalSpeed=0;bool grounded=false;
    const RevcTriangle* mesh = nullptr;
    int meshCount = 0;
    Point spawn{}, route[128]{};
    int routeCount = 0, targets[3]{};
    bool indexed=false;
    int cellFixed=0;
    int gridOriginX=0,gridOriginZ=0;
    struct FloorPlane {float x,z,height,inverseDet;};
    FloorPlane floorPlanes[4096]{};bool planesCached=false;
    static constexpr int GridSide=50,GridCells=GridSide*GridSide;
    uint32_t starts[GridCells+1]{};
    uint16_t references[65536]{};
    mutable unsigned char traceVisited[8192]{};
    int bucket(float x,float z) const;
    bool ground(float x, float z, float low, float high, float& y) const;
    bool blocked(Point p,float footprint=.4f) const;
    bool blockedPlayer(Point p) const;
    bool step(Point& p, float dx, float dz) const;
    float traceFraction(Point a,Point b,bool nearest) const;
};
}
