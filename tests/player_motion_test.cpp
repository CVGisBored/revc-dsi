#include "player_motion.h"
#include "player_motion_data.h"
#include "gameplay.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <vector>
using namespace slice;
static RevcTriangle triangle(Point a,Point b,Point c) {
    RevcTriangle t{};Point points[]={a,b,c};for(int j=0;j<3;++j){t.vertex[j][0]=std::lround(points[j].x*64);t.vertex[j][1]=std::lround(points[j].y*64);t.vertex[j][2]=std::lround(points[j].z*64);}return t;
}
static void plane(std::vector<RevcTriangle>& t,float x0,float z0,float x1,float z1,float y) {
    t.push_back(triangle({x0,y,z0},{x1,y,z0},{x1,y,z1}));t.push_back(triangle({x0,y,z0},{x1,y,z1},{x0,y,z1}));
}
int main() {
    PlayerMotion actor;PlayerMotion::Input input;input.z=-1;float distance=0;
    for(int i=0;i<120;++i)actor.tick(input,true);
    for(int i=0;i<240;++i){auto s=actor.tick(input,true);distance+=-s.z;}
    const auto& run=playerData::tracks[Animation::Run];float runSpeed=(run.keys[run.count-1].forward-run.keys[0].forward)/run.duration;
    assert(std::fabs(distance-runSpeed*4)<.02f);std::printf("Original run: %.6f m/s; four-second travel %.6f m\n",runSpeed,distance);
    input.walk=true;for(int i=0;i<120;++i)actor.tick(input,true);distance=0;
    for(int i=0;i<552;++i)distance-=actor.tick(input,true).z;
    const auto& walk=playerData::tracks[Animation::Walk];float walkSpeed=(walk.keys[walk.count-1].forward-walk.keys[0].forward)/walk.duration;
    assert(std::fabs(distance-walkSpeed*9.2f)<.03f);
    actor.reset();input.walk=false;input.sprint=true;for(int i=0;i<90;++i)actor.tick(input,true);distance=0;
    for(int i=0;i<60;++i)distance-=actor.tick(input,true).z;
    const auto& sprint=playerData::tracks[Animation::Sprint];float sprintSpeed=(sprint.keys[sprint.count-1].forward-sprint.keys[0].forward)/sprint.duration;
    assert(std::fabs(distance-sprintSpeed)<.02f);std::printf("Original sprint: %.6f m/s; one-second travel %.6f m\n",sprintSpeed,distance);
    for(int i=0;i<330;++i)actor.tick(input,true);
    assert(actor.stamina()<0 && actor.second()==Animation::Run);input.sprint=false;input.z=0;
    float tired=actor.stamina();for(int i=0;i<60;++i)actor.tick(input,true);assert(actor.stamina()>tired+24.9f);
    float preserved=actor.stamina();actor.settle(1000);assert(actor.stamina()==preserved);
    actor.rest(60);assert(std::fabs(actor.stamina()-preserved-25)<.01f);
    // Authored start delays travel; sprint is a distinct original clip.
    actor.reset();input=PlayerMotion::Input{};input.z=-1;
    distance=0;for(int i=0;i<16;++i)distance-=actor.tick(input,true).z;
    assert(std::fabs(distance-.174288f)<.002f);
    // Releasing sprint while stationary uses the authored run-stop travel.
    actor.reset();input.z=-1;input.sprint=true;for(int i=0;i<90;++i)actor.tick(input,true);
    input.z=0;input.sprint=false;float stop=0;for(int i=0;i<50;++i)stop-=actor.tick(input,true).z;
    assert(stop>1.9f && stop<2.2f);
    // Digital diagonals do not multiply the original root speed.
    actor.reset();input.x=std::sqrt(.5f);input.z=-input.x;
    for(int i=0;i<120;++i)actor.tick(input,true);distance=0;
    for(int i=0;i<240;++i){auto s=actor.tick(input,true);distance+=std::sqrt(s.x*s.x+s.z*s.z);}
    assert(std::fabs(distance-runSpeed*4)<.02f);
    std::vector<RevcTriangle> geometry;plane(geometry,-40,-40,40,40,0);
    Game game;assert(game.startExploration(geometry.data(),geometry.size(),{0,0,0}));
    actor.reset();input=PlayerMotion::Input{};input.jump=true;float apex=0;bool airborne=false,landed=false;
    for(int i=0;i<120;++i){auto s=actor.tick(input,game.playerGrounded());input.jump=false;game.movePlayer(s.x,s.z,1.f/60,s.impulse);apex=std::max(apex,game.player.y);airborne|=!game.playerGrounded();if(airborne && game.playerGrounded())landed=true;}
    assert(airborne && landed && apex>.8f && apex<.95f && std::fabs(game.player.y)<.001f);
    std::printf("Original gravity/8.5 impulse on 70 kg: jump apex %.4f m, landed\n",apex);
    // Walking off a real ledge falls to a lower supported surface instead of
    // being stopped by the old requirement for four supported footprint edges.
    geometry.clear();plane(geometry,-10,-10,10,10,-2);plane(geometry,-2,-2,2,2,0);
    assert(game.startExploration(geometry.data(),geometry.size(),{0,0,0}));
    for(int i=0;i<90;++i)game.movePlayer(.04f,0,1.f/60);
    for(int i=0;i<90;++i)game.movePlayer(0,0,1.f/60);
    assert(game.player.x>3 && game.playerGrounded() && std::fabs(game.player.y+2)<.001f);
    // A thin wall stops a maximum sprint step, including batched rendering.
    geometry.clear();plane(geometry,-10,-10,10,10,0);
    geometry.push_back(triangle({1,0,-10},{1,3,-10},{1,3,10}));geometry.push_back(triangle({1,0,-10},{1,3,10},{1,0,10}));
    assert(game.startExploration(geometry.data(),geometry.size(),{0,0,0}));
    for(int i=0;i<60;++i)game.movePlayer(sprintSpeed/60,0,1.f/60);
    assert(game.player.x<.66f && game.playerGrounded());
    // The source-sized head sphere prevents jumping through a low ceiling.
    geometry.clear();plane(geometry,-10,-10,10,10,0);plane(geometry,-10,-10,10,10,2.1f);
    assert(game.startExploration(geometry.data(),geometry.size(),{0,0,0}));apex=0;
    for(int i=0;i<80;++i){game.movePlayer(0,0,1.f/60,i==0 ? 8.5f/70*50 : 0);apex=std::max(apex,game.player.y);}
    assert(apex<.2f && game.playerGrounded());
    // The cached planes and smaller index must agree with the original
    // uncached collision calculations on a slope, including a jump/landing.
    geometry.clear();
    geometry.push_back(triangle({-10,-1,-10},{10,1,-10},{10,1,10}));
    geometry.push_back(triangle({-10,-1,-10},{10,1,10},{-10,-1,10}));
    auto uncached=geometry;uncached.resize(4097,triangle({300,300,300},{300,300,300},{300,300,300}));
    Game original;
    assert(game.startExploration(geometry.data(),geometry.size(),{0,0,0}));
    assert(original.startExploration(uncached.data(),uncached.size(),{0,0,0}));
    for(int i=0;i<240;++i) {
        float impulse=i==60 ? 8.5f/70*50 : 0;
        game.movePlayer(.02f,.015f,1.f/60,impulse);original.movePlayer(.02f,.015f,1.f/60,impulse);
        assert(std::fabs(game.player.x-original.player.x)<.0001f && std::fabs(game.player.y-original.player.y)<.0001f && std::fabs(game.player.z-original.player.z)<.0001f);
        assert(game.playerGrounded()==original.playerGrounded());
    }
    std::puts("Cached slope collision matches original calculations throughout movement, jump and landing");
    // Priority collision may move well beyond the old visual-origin box.
    // It must retain the fast grid and the same floor/wall/trace semantics.
    geometry.clear();plane(geometry,240,-310,260,-290,0);
    geometry.push_back(triangle({252,0,-310},{252,3,-310},{252,3,-290}));
    geometry.push_back(triangle({252,0,-310},{252,3,-290},{252,0,-290}));
    assert(game.startExploration(geometry.data(),geometry.size(),{250,0,-300}));
    assert(game.collisionIndexed());
    for(int i=0;i<60;++i)game.movePlayer(.05f,0,1.f/60);
    assert(game.player.x>251 && game.player.x<251.66f && game.playerGrounded());
    assert(!game.lineClear({250,1,-300},{255,1,-300}));
    std::puts("Collision grid follows translated snapshots beyond the visual origin");
    std::puts("Player source-speed, stamina, stopping, diagonal, jump, ledge, wall and ceiling checks passed");
}
