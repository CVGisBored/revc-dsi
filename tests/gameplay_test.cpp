#include "gameplay.h"
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <limits>
#include <algorithm>

static void require(bool condition,const char* message) {
    if(!condition) {std::fprintf(stderr,"FAIL: %s\n",message);std::exit(1);}
}
static RevcTriangle triangle(slice::Point a,slice::Point b,slice::Point c) {
    RevcTriangle t{}; const slice::Point p[3]={a,b,c};
    for(int i=0;i<3;++i) {
        t.vertex[i][0]=static_cast<int16_t>(p[i].x*64);
        t.vertex[i][1]=static_cast<int16_t>(p[i].y*64);
        t.vertex[i][2]=static_cast<int16_t>(p[i].z*64);
    }
    return t;
}
static void traverse(slice::Game& game) {
    for(int i=1;i<game.routeLength();++i) {
        auto target=game.routePoint(i);
        for(int k=0;k<40 && game.state==slice::Game::Playing;++k) {
            float dx=target.x-game.player.x,dz=target.z-game.player.z;
            float distance=std::sqrt(dx*dx+dz*dz);
            if(distance>.001f) {
                const float scale=std::min(.15f/distance,1.f);
                require(game.move(dx*scale,dz*scale),"reachable route must permit traversal");
            }
            game.tick();
        }
    }
    require(game.state==slice::Game::Won && game.collected==3,"route must complete three checkpoints");
}
int main(int argc,char** argv) {
    const RevcTriangle fixtures[]={
        triangle({-15,0,-15},{15,0,-15},{15,0,15}),
        triangle({-15,0,-15},{15,0,15},{-15,0,15}),
        triangle({5,0,-15},{5,3,-15},{5,3,15}),
        triangle({5,0,-15},{5,3,15},{5,0,15})};
    slice::Game game;
    require(!game.init(nullptr,0),"missing geometry must fail safely");
    require(game.init(fixtures,4),"flat test arena must initialize");
    auto camera=game.cameraPosition({0,1.1f,0},{8,1.1f,0});
    require(camera.x>4.6f && camera.x<4.71f,"camera stops before the nearest wall");
    require(game.lineClear({0,1.1f,0},camera),"pulled camera remains visible from its focus");
    auto clearCamera=game.cameraPosition({0,1.1f,0},{-3,2.3f,0});
    require(clearCamera.x==-3 && clearCamera.y==2.3f,"unobstructed camera retains requested offset");
    const RevcTriangle ramp[]={triangle({-10,2,-10},{10,2,-10},{10,-2,10}),triangle({-10,2,-10},{10,-2,10},{-10,-2,10})};
    game.bindGeometry(ramp,2);slice::Point car={0,0,0},tilt;
    require(game.vehiclePose(car,0,1,&tilt),"car footprint follows a bridge ramp");
    require(tilt.x>.19f && tilt.x<.21f && std::fabs(tilt.z)<.001f,"vehicle pitch follows front/rear ground contacts");
    game.bindGeometry(fixtures,4);
    require(!game.move(std::numeric_limits<float>::quiet_NaN(),0),"reject nonfinite movement");
    require(!game.move(1000,0),"reject unbounded displacement");
    game.player={0,0,0};
    for(int i=0;i<100;++i) game.move(.14f,0);
    require(game.player.x<4.61f && game.player.x>4.3f,"wall must block running player");
    game.player={0,0,0};
    for(int i=0;i<150;++i) game.move(0,.14f);
    require(game.player.z<14.61f && game.player.z>14.3f,"ledge must retain entire player footprint");
    game.restart(); const int time=game.framesLeft;
    game.pause(); auto before=game.player;
    require(!game.move(.1f,0),"pause blocks movement");
    game.tick(); require(game.framesLeft==time && game.player.x==before.x,"pause freezes timer");
    game.pause(); traverse(game);
    game.restart();
    for(int i=0;i<120*60;++i) game.tick();
    require(game.state==slice::Game::TimedOut,"timeout ends challenge");
    game.restart(); require(game.state==slice::Game::Playing && game.collected==0,"retry resets state");
    game.tick(180); require(game.framesLeft==117*60,"timer accepts elapsed time after slow rendering");
    game.tick(-10); require(game.framesLeft==117*60,"timer cannot gain time from invalid deltas");
    const RevcTriangle gap[]={
        triangle({-2,0,-2},{2,0,-2},{2,0,2}),triangle({-2,0,-2},{2,0,2},{-2,0,2}),
        triangle({8,0,-2},{12,0,-2},{12,0,2}),triangle({8,0,-2},{12,0,2},{8,0,2})};
    require(!game.init(gap,4),"isolated platforms cannot create a disconnected route");
    if(argc==2) {
        require(revcSectorLoad(argv[1])==0,"load actual upstream-decoded assets");
        require(game.init(revcSectorTriangles(),revcSectorTriangleCount()),"actual sector must contain a route");
        std::printf("Actual sector: %d route nodes, spawn %.2f %.2f %.2f\n",
                    game.routeLength(),game.player.x,game.player.y,game.player.z);
        traverse(game);
        revcSectorShutdown();
    }
    std::puts("Gameplay collision, reachability, win, pause, timeout and retry tests passed");
}
