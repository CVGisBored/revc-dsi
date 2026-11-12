#include "world.h"
#include "campaign.h"
#include "mission_library.h"
#include <cassert>
#include <cstdio>
#include <cmath>
static slice::World world;static slice::Game game;static slice::Campaign campaign;static slice::Street actors;
static bool visit(slice::Point global){
 auto a=world.anchor();slice::Point shift,p{global.x-a.x,global.y-a.y,global.z-a.z};if(!world.update(p,shift,true))return false;
 a=world.anchor();p={global.x-a.x,global.y-a.y,global.z-a.z};game.bindGeometry(world.triangles(),world.count());
 for(int ring=0;ring<8;++ring)for(int side=0;side<8;++side){auto q=p;float angle=side*.785398163f;q.x+=std::sin(angle)*ring*.5f;q.z+=std::cos(angle)*ring*.5f;
  if(game.placeActor(q,8)){game.player=q;game.state=slice::Game::Playing;actors.inCar=false;actors.health=100;return true;}}
 return false;
}
static void tick(){campaign.tick(game,actors,world.anchor(),1,false);}
static void skipScenes(){for(int n=0;n<10 && campaign.pendingCutscene();++n){campaign.finishCutscene(true);tick();}if(campaign.briefing())campaign.advanceBriefing(true);}
int main(int argc,char** argv){
 assert(argc==2 && world.open(argv[1]));assert(slice::MissionLibrary::count()==97);campaign.reset();unsigned routes=0,points=0,combat=0;
 for(unsigned i=0;i<slice::MissionLibrary::count();++i){const auto& m=slice::MissionLibrary::mission(i);assert(m.id==i && m.routeCount<=8 && m.movieCount<=8);
  if(m.kind==slice::MissionLibrary::Catalog){assert(!campaign.selectMission(i));continue;}
  if(m.kind!=slice::MissionLibrary::Route)continue;
  if(!visit(m.start)){std::fprintf(stderr,"Unsupported contact %u\n",i);return 4;}
  assert(m.routeCount && campaign.selectMission(i));assert(campaign.briefing());
  if(campaign.pendingCutscene()){campaign.finishCutscene(false);assert(campaign.briefing());}
  campaign.advanceBriefing(true);const int cash=actors.cash;
  for(unsigned p=0;p<m.routeCount;++p){if(!visit(m.route[p])){std::fprintf(stderr,"Unsupported route %u point %u at %.2f %.2f %.2f\n",i,p,m.route[p].x,m.route[p].y,m.route[p].z);return 2;}
   tick();skipScenes();assert(campaign.routeStep()>=p+1);++points;}
  if(m.action){assert(campaign.state()==slice::Campaign::LibraryCombat);++combat;
   // Resolve genuine placed targets using the production strike/LOS path.
   for(int n=0;n<campaign.npcCount()+campaign.vehicleCount();++n){
    bool car=n>=campaign.npcCount();int k=car ? n-campaign.npcCount() : n;int attempts=0;
    while((car ? campaign.vehicles()[k].health : campaign.npcs()[k].health)>0){
     assert(++attempts<30);auto t=car ? campaign.vehicles()[k].global : campaign.npcs()[k].global;bool hit=false;
     for(int side=0;side<16 && !hit;++side){auto q=t;float angle=side*.392699082f;q.x+=std::sin(angle)*5;q.z+=std::cos(angle)*5;if(!visit(q))continue;tick();auto a=world.anchor();t=car ? campaign.vehicles()[k].global : campaign.npcs()[k].global;
      float dx=t.x-a.x-game.player.x,dz=t.z-a.z-game.player.z,d=std::sqrt(dx*dx+dz*dz);if(d>.01f)hit=campaign.strike(game,actors,a,dx/d,-dz/d,true);}
     if(!hit){std::fprintf(stderr,"Unreachable combat target mission %u target %d placed %d\n",i,n,car ? campaign.vehicles()[k].placed : campaign.npcs()[k].placed);return 3;}
    }
   }
   tick();skipScenes();
  }
  tick();skipScenes();assert(campaign.state()==slice::Campaign::LibraryPassed && campaign.missionDone(i));assert(actors.cash==cash+int(m.reward));
  for(int n=0;n<8;++n)tick();assert(actors.cash==cash+int(m.reward));
  actors.health=0;tick();assert(campaign.state()==slice::Campaign::Failed);campaign.retry(actors);assert(campaign.briefing());
  ++routes;std::printf("%u %s: route, targets, movie fallback and reward passed\n",i,m.title);std::fflush(stdout);
 }
 // Replaying a completed route must not pay its reward twice.
 const auto& replay=slice::MissionLibrary::mission(23);const int cash=actors.cash;
 assert(campaign.selectMission(23));skipScenes();
 for(unsigned k=0;k<replay.routeCount;++k){assert(visit(replay.route[k]));tick();skipScenes();}
 tick();skipScenes();assert(campaign.state()==slice::Campaign::LibraryPassed && actors.cash==cash);
 assert(!campaign.selectMission(97));unsigned story=0;for(int i=slice::MissionLibrary::nextStory();i>=0;i=slice::MissionLibrary::nextStory(i)){assert(++story<=97);assert(slice::MissionLibrary::mission(i).kind==slice::MissionLibrary::Route);}
 std::printf("%u early adaptations, %u ground-supported checkpoints, %u combat objectives, %u story links passed\n",routes,points,combat,story);
}
