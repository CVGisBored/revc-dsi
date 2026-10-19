#include "world.h"
#include "render_budget.h"
#include "campaign_data.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <vector>
using namespace slice;
static World world;static Game game;
static bool supports(const RevcTriangle& t,Point p){
 float x=p.x*64,z=p.z*64,ax=t.vertex[0][0],az=t.vertex[0][2],bx=t.vertex[1][0],bz=t.vertex[1][2],cx=t.vertex[2][0],cz=t.vertex[2][2];
 float det=(bx-ax)*(cz-az)-(bz-az)*(cx-ax);if(std::fabs(det)<1)return false;
 float u=((x-ax)*(cz-az)-(z-az)*(cx-ax))/det,v=((bx-ax)*(z-az)-(bz-az)*(x-ax))/det;
 if(u<-.002f || v<-.002f || u+v>1.002f)return false;
 float y=t.vertex[0][1]+u*(t.vertex[1][1]-t.vertex[0][1])+v*(t.vertex[2][1]-t.vertex[0][1]);return std::fabs(y-p.y*64)<24;
}
static bool drawn(Point p,int n){for(int i=0;i<n;++i)if(supports(world.visibleTriangle(i),p))return true;return false;}
int main(int argc,char** argv){assert(argc==2 && world.open(argv[1]));
 int adaptive=900;for(int i=0;i<3600;++i)adaptive=RenderBudget::slower(adaptive);assert(adaptive==512);
 for(int scene:{1536,1760}){int planned=487+122;for(int cost:{80,122,112,80,88,80,320,552})if(RenderBudget::admits(scene,planned,cost))planned+=cost;
  for(int actual:{planned,planned-368}){int near=RenderBudget::nearby(scene,actual,128);assert(near>=512);assert(RenderBudget::distant(scene,actual,near)>=256);assert(actual+near+RenderBudget::distant(scene,actual,near)+64==scene);}}
int samples=0,missing=0,starved=0;
 for(int site=0;site<27;++site){auto global=site==2 ? missionData::Hotel : missionData::Spawn;if(site==1)global.x-=6;if(site==2){global.x-=16;global.z+=3;}
  if(site>=3)global={-150,10,-350+(site-3)*4.f};
  auto old=world.anchor();Point request={global.x-old.x,global.y-old.y,global.z-old.z},shift;assert(world.update(request,shift,true));auto anchor=world.anchor();Point local={global.x-anchor.x,global.y-anchor.y,global.z-anchor.z};
  bool supported=game.startExploration(world.triangles(),world.count(),local);for(int i=1;!supported && i<20;++i){auto q=local;q.y-=i*.25f;supported=game.startExploration(world.triangles(),world.count(),q);}assert(supported);
  for(int angle=0;angle<16;++angle){float yaw=angle*6.2831853f/16,si=std::sin(yaw),co=std::cos(yaw),pitch=.22f;Point camera={game.player.x-si*3,game.player.y+1.8f,game.player.z+co*3};
   world.renderBudget(900);int reference=world.visible(camera,si,co,std::sin(pitch),std::cos(pitch),&game.player);std::vector<Point> floor;
   for(float ahead:{0.f,2.f,4.f,6.f})for(float side:{-1.5f,0.f,1.5f}){Point p={game.player.x+si*ahead+co*side,game.player.y,game.player.z-co*ahead+si*side};if(game.placeActor(p,.3f) && drawn(p,reference))floor.push_back(p);}
   world.renderBudget(512);int n=world.visible(camera,si,co,std::sin(pitch),std::cos(pitch),&game.player);assert(n<=512);int sum=0;for(int i=0;i<world.visibleTileCount();++i)sum+=world.visibleTileTriangles(i);assert(sum==n);
   for(auto p:floor){++samples;if(!drawn(p,n))++missing;}
   world.renderBudget(128);n=world.visible(camera,si,co,std::sin(pitch),std::cos(pitch),&game.player);for(auto p:floor)if(!drawn(p,n))++starved;
  }
 }
 printf("Near-world floor samples: %d, missing at 512: %d, missing at 128: %d\n",samples,missing,starved);
 fflush(stdout);assert(samples>200);assert(missing==0);
 // Stop visual snapshots entirely while a car keeps moving. Already loaded
 // source COL must still provide a bounded, continuous floor under the car.
 assert(world.open(argv[1]));Point seed={-150,10,-350},shift;assert(world.update(seed,shift,true));auto origin=world.anchor();Point start={seed.x-origin.x,seed.y-origin.y,seed.z-origin.z};assert(game.startExploration(world.triangles(),world.count(),start));unsigned revision=world.collisionRevision();int recovered=0,holes=0;
 for(int frame=0;frame<40;++frame){auto lead=game.player;lead.z+=6;assert(world.updateCollision(game.player,lead,1));if(revision!=world.collisionRevision()){game.bindGeometry(world.triangles(),world.count());revision=world.collisionRevision();}
  for(int tick=0;tick<4;++tick)assert(game.vehicleMove(0,.5f,0,-1));
  if(world.visualLag(game.player)>10){int fallback=world.groundFallback(game.player);assert(fallback>0 && fallback<=128);assert(world.groundFallback(game.player)==fallback);
   for(float ahead:{0.f,3.f,6.f}){auto p=game.player;p.z+=ahead;if(!game.placeActor(p,.1f))continue;++recovered;bool found=false;for(int i=0;i<fallback;++i)found|=supports(world.fallbackTriangle(i),p);holes+=!found;}
  }
 }
 std::printf("Frozen visual stream at driving speed: %d source-ground samples, %d fallback holes, %.1fm visual lag\n",recovered,holes,world.visualLag(game.player));assert(recovered>90 && holes==0);

}
