#include "world.h"
#include "distant.h"
#include "campaign_data.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <algorithm>
static slice::World world;static slice::DistantWorld farWorld;static slice::Game game;
int main(int argc,char** argv){
 assert(argc==2 && world.open(argv[1]));char path[1024];std::snprintf(path,sizeof(path),"%s/world/distant.vcd",argv[1]);assert(farWorld.open(path));
 auto p=slice::missionData::Hotel;p.x-=16;p.z+=3;slice::Point shift;assert(world.update(p,shift,true));auto anchor=world.anchor();p={p.x-anchor.x,p.y-anchor.y,p.z-anchor.z};
 bool safe=game.startExploration(world.triangles(),world.count(),p);for(int i=1;!safe && i<=20;++i){auto q=p;q.y-=i*.25f;safe=game.startExploration(world.triangles(),world.count(),q);}assert(safe);
 slice::Point global{game.player.x+anchor.x,game.player.y+anchor.y,game.player.z+anchor.z};assert(farWorld.update(global,anchor));unsigned bytes=farWorld.readBytes();assert(farWorld.update(global,anchor) && farWorld.readBytes()==0);
 int lodViews=0;float highest=0;
 for(int view=0;view<12;++view){float yaw=view*3.14159265f/6,si=std::sin(yaw),co=std::cos(yaw),pitch=.18f,ps=std::sin(pitch),pc=std::cos(pitch);
  slice::Point camera{game.player.x-si*3,game.player.y+1.65f,game.player.z+co*3};
  int far=farWorld.visible(camera,si,co,ps,pc,game.player,700);
  int budget=std::min(900,1760-520-64-far);world.renderBudget(budget);
  int near=world.visible(camera,si,co,ps,pc,&game.player);
  assert(near<=budget && far<=700 && near+far+520+64<=1760);assert(world.fogEnd()==64);
  assert(farWorld.visible(camera,si,co,ps,pc,game.player,700)==far);
  int complete=0;for(int i=0;i<farWorld.visibleObjectCount();++i)complete+=farWorld.visibleObjectTriangles(i);
  assert(complete==far);
  int tiles=0;for(int i=0;i<world.visibleTileCount();++i)tiles+=world.visibleTileTriangles(i);assert(tiles==near);
  assert(world.visible(camera,si,co,ps,pc,&game.player)==near);
  for(int i=0;i<far;++i)for(auto& v:farWorld.triangle(i).vertex)highest=std::max(highest,v[1]/64.f-game.player.y);
  for(int i=0;i<near;++i)for(auto& v:world.visibleTriangle(i).vertex)highest=std::max(highest,v[1]/64.f-game.player.y);
  lodViews+=far>0;std::printf("Ocean View angle %d: near %d, distant %d, fog %.0fm\n",view*30,near,far,world.fogEnd());
 }
 assert(lodViews>=8 && highest>12);std::printf("Ocean View: %d views have distant geometry; upper facade %.2fm; far cold IO %u, stationary IO 0\n",lodViews,highest,bytes);
}
