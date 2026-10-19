#include "world.h"
#include "population.h"
#include "street.h"
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <limits>
#include <algorithm>

static void require(bool condition,const char* message) {
    if(!condition) {std::fprintf(stderr,"FAIL: %s\n",message);std::exit(1);}
}
static slice::World world;
static slice::Population population;
static slice::Game game;
static void refresh() {
    auto before=game.player,origin=world.anchor();slice::Point shift;
    if(!world.update(game.player,shift)) {
        std::fprintf(stderr,"Stream position: %.3f %.3f %.3f\n",before.x+origin.x,before.y+origin.y,before.z+origin.z);
        require(false,"stream nearby map");
    }
    game.rebase(shift);game.bindGeometry(world.triangles(),world.count());
    auto after=game.player,next=world.anchor();
    require(std::fabs(before.x+origin.x-after.x-next.x)<.001f &&
            std::fabs(before.y+origin.y-after.y-next.y)<.001f &&
            std::fabs(before.z+origin.z-after.z-next.z)<.001f,"rebase preserves world position");
    require(world.count()>0 && world.count()<=slice::World::CollisionCapacity,"bounded collision residency");
}
int main(int argc,char** argv) {
    require(argc==2,"asset root argument");
    require(revcSectorLoad(argv[1])==0,"bootstrap upstream asset loading");
    require(game.init(revcSectorTriangles(),revcSectorTriangleCount()),"initial challenge route");
    require(world.open(argv[1]),"world index and original player mesh load");
    require(world.chunkCount()==4925,"full map index coverage");
    require(world.playerCount()==487,"simplified original player geometry");
    refresh();
    require(world.originalCollision(),"original COL collision is active");
    require(world.lastReadBytes()<=512*1024,"startup geometry IO must stay bounded");
    std::printf("Startup geometry IO: %u bytes\n",world.lastReadBytes());
    const uint32_t coldBytes=world.lastReadBytes();slice::Point repeatShift;
    require(world.update(game.player,repeatShift,true),"repeat cached neighbourhood");
    const uint32_t warmBytes=world.lastReadBytes();
    require(warmBytes<coldBytes,"cached tiles reduce repeated geometry reads");
    std::printf("Warm geometry IO: %u bytes (cold %u)\n",warmBytes,coldBytes);
    require(world.update(game.player,repeatShift),"unchanged neighbourhood");
    require(world.lastReadBytes()==0,"no storage IO when player stays in loaded neighbourhood");
    const auto preferred=game.player;
    require(game.startExploration(world.triangles(),world.count(),preferred),"fast exploration startup");
    require(game.routeLength()==0 && game.exploring,"startup must not run route flood search");
    slice::Street street;
    require(street.start(game),"mission actors and drivable corridor in actual city");
    require(population.open(argv[1]) && population.refresh(game,world.anchor(),street.car),"original local population paths load");
    require(population.activePeds()>0 && population.vehicle().active,"pedestrians and road traffic spawn on original paths");
    slice::Point trafficStart[slice::Population::VehicleCapacity];
    int initialCars=population.activeCars();require(initialCars>=3,"multiple original-path traffic cars spawn");
    for(int i=0;i<slice::Population::VehicleCapacity;++i)trafficStart[i]=population.vehicles()[i].position;
    for(int tick=0;tick<240;++tick) {
        population.tick(game,street.car,tick<60 ? 1 : 0,1);
        for(int i=0;i<slice::Population::PedCount;++i) {
            const auto& p=population.pedestrians()[i];if(!p.active)continue;
            auto grounded=p.position;require(game.placeActor(grounded,.03f),"NPC retains original COL support");
            require(std::fabs(grounded.y-p.position.y)<.03f,"NPC stays above the support surface");
        }
    }
    int movedCars=0;
    for(int i=0;i<slice::Population::VehicleCapacity;++i){const auto& car=population.vehicles()[i];float dx=car.position.x-trafficStart[i].x,dz=car.position.z-trafficStart[i].z;if(car.active && dx*dx+dz*dz>.25f)++movedCars;}
    require(movedCars>=2,"multiple traffic cars drive along source paths");
    std::printf("Traffic: %d initial cars, %d moved over four seconds\n",initialCars,movedCars);
    auto beforePause=population.pedestrians()[0].clock;game.pause();population.tick(game,street.car,1,4);
    require(population.pedestrians()[0].clock==beforePause,"pause freezes ambient NPC simulation");game.pause();
    const auto foot=game.player;
    int contacts=world.visible({foot.x,foot.y+2.3f,foot.z+2.8f},0,1,.427f,.904f,&foot);
    for(int i=0;i<contacts;++i)require(world.visibleMaterial(i).texture!=4009 && world.visibleMaterial(i).texture!=4047,
        "distant island backdrops cannot cover the player at spawn");
    std::printf("Street job car %.2f %.2f; delivery %.2f %.2f\n",street.car.x,street.car.z,street.destination.x,street.destination.z);
    street.interact();require(street.armed,"actual-map weapon pickup");
    for(int i=0;i<3;++i) {
        float dx=street.guard.x-game.player.x,dz=street.guard.z-game.player.z,d=std::sqrt(dx*dx+dz*dz);
        require(street.shoot(dx/d,-dz/d),"actual-map firing");street.tick(15);
    }
    require(street.guardHealth==0,"actual-map guard can be defeated from start area");
    street.interact();
    require(street.inCar,"actual-map car boarding");
    street.tick(90); // Finish door/entry sequence before the short driving test.
    const float heading=street.carYaw*6.28318530718f/32768;
    const float sine=std::sin(heading),cosine=std::cos(heading);
    for(int i=0;i<180 && street.phase!=slice::Street::Complete;++i) {
        float dx=street.destination.x-street.car.x,dz=street.destination.z-street.car.z;
        float stopping=street.speed*street.speed/(2*slice::Street::BrakeDeceleration)+.7f;
        bool brake=dx*dx+dz*dz<stopping*stopping;
        street.drive(brake ? 0 : 1,brake,sine,cosine,1);street.tick(1);
        slice::Point shift;
        require(world.update(game.player,shift),"mission driving streams city");
        game.rebase(shift);street.rebase(shift);game.bindGeometry(world.triangles(),world.count());
    }
    require(street.phase==slice::Street::Complete && street.cash==250,"actual-map delivery completion");
    street.reset();refresh();
    game.restart();require(game.state==slice::Game::Playing,"restart exploration without a route");
    game.player=preferred;refresh();
    game.explore(false);
    require(game.init(world.triangles(),world.count(),&preferred),"route through full map collision");
    int drawn=world.visible({game.player.x,game.player.y+2.3f,game.player.z+2.8f},0,1,.426f,.905f);
    std::printf("Full map: %d chunks; resident %d; capped %d; draw %d+%d; fog %.2fm\n",
                world.chunkCount(),world.count(),world.dropped(),drawn,world.playerCount(),world.fogEnd());
    require(drawn>0 && drawn<=slice::World::MapBudget,"bounded map draw count");
    world.renderBudget(128);
    require(world.visible({game.player.x,game.player.y+2.3f,game.player.z+2.8f},0,1,.426f,.905f)<=128,"reduced adaptive render budget");
    world.renderBudget(slice::World::MapBudget);
    const int actors=520+world.vehicleCount()+120+slice::Population::PedCount*80+population.trafficTriangles();
    const int map=std::max(128,std::min(slice::World::MapBudget,1960-actors-32));
    const int polygons=std::min(1980,map+actors+32);
    require(polygons<2048 && polygons*3<6144,"actual animated actor, traffic, terrain and map hardware budget");
    require(world.fogEnd()==64,"stable distant fog limit");
    for(int i=1;i<game.routeLength();++i) {
        for(int k=0;k<60 && game.state==slice::Game::Playing;++k) {
            auto target=game.routePoint(i);float dx=target.x-game.player.x,dz=target.z-game.player.z;
            float length=std::sqrt(dx*dx+dz*dz);
            if(length>.02f) {
                const float step=std::min(.15f/length,1.f);
                require(game.move(dx*step,dz*step),"challenge traversal with full map collision");
            }
            game.tick();refresh();
        }
    }
    require(game.state==slice::Game::Won,"full map must retain challenge completion");
    game.explore(true);const int time=game.framesLeft;game.tick(7200);
    require(game.state==slice::Game::Playing && game.framesLeft==time,"exploration disables timer");
    for(slice::Point global : {slice::Point{63.9f,10,0},slice::Point{64.1f,10,0},
                              slice::Point{-64.1f,10,0},
                              slice::Point{-533.83f,6.61f,-2644.51f},
                              slice::Point{-1915.18f,13.49f,-326.64f},
                              slice::Point{-1259.9f,10.61f,-723.1f},
                              slice::Point{-861.97f,9.25f,-784.29f}}) {
        auto anchor=world.anchor();game.player={global.x-anchor.x,global.y-anchor.y,global.z-anchor.z};
        refresh();
    }
    slice::Point shift;require(!world.update({std::numeric_limits<float>::infinity(),0,0},shift),"reject invalid stream location");
    world.close();require(world.count()==0 && world.playerCount()==0,"close resets residency");
    revcSectorShutdown();
    std::puts("Full-map streaming, player, draw budget, challenge, exploration and rebasing tests passed");
}
