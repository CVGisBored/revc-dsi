// Opening story adaptation and live city navigation. The map streams from NitroFS.
#include <nds.h>
#include <filesystem.h>
#include <calico/system/tick.h>
#include <cstdio>
#include <algorithm>
#include <cmath>
#include <cstring>
#include "world.h"
#include "view.h"
#include "distant.h"
#include "street.h"
#include "animation.h"
#include "player_motion.h"
#include "actor_texture.h"
#include "textures.h"
#include "texture_plan.h"
#include "water.h"
#include "lighting.h"
#include "population.h"
#include "campaign.h"
#include "citymap.h"
#include "weapon_wheel.h"
#include "cinematic.h"
#include "frontend.h"
#include "vehicle_animation.h"
#include "vehicle_models.h"
#include "garage.h"
#include "render_budget.h"
#include "projection.h"
#include "rom_storage.h"
#include "gameplay_display.h"
#ifdef REVC_INSPECT_OCEAN_VIEW
#include "campaign_data.h"
#endif

namespace {
PrintConsole hud;
slice::Game physics;
slice::World world;
slice::DistantWorld distant;
slice::Animation chefAnimation,buddyAnimation,workerAnimation;
bool missionNear=false;
unsigned textureUploads=0,textureFrame=0;
bool textureUploadAllowed=true;
unsigned renderProfile[5]{};
int sceneBudget=1760,gpuVertices=0,gpuPolygons=0;unsigned safeGpuFrames=0;bool gpuOverflow=false;
slice::Street job;
slice::Animation playerAnimation,guardAnimation;
slice::Animation driverAnimation,trafficDriverAnimation;
slice::VehicleModels vehicleModels;
slice::ActorTexture fleetTexture,pedestrianTexture;
slice::VehicleAnimation vehicleAnimation;
slice::PlayerMotion playerMotion;
bool playerAction=false;
slice::ActorTexture playerTexture;
slice::Animation civilianAnimation[2];
slice::Population population;
slice::Campaign campaign;
slice::CityMap citymap;
unsigned playerOutfit=1;
int guardYaw=0;
unsigned boundRevision=0;
int yaw=0,cameraPitch=2300,drawn=0,submitted=0;
unsigned animationClock=0;
slice::TextureFile textureFile;
slice::DetailTextureFile detailTextureFile;
slice::DetailTexturePage detailTexturePage;
bool detailTextureUploadAllowed=false;unsigned settledTextureFrames=0;
slice::Water water;
slice::Lighting lighting;
slice::TexturePage texturePage;
struct CachedTexture {int source=-1,handle=0;unsigned age=0,usedFrame=0,bytes=0,scale=1,priority=64;};
constexpr int TextureCacheSlots=48;
CachedTexture textureCache[TextureCacheSlots];
unsigned textureUploadLimit=2;uint64_t textureUploadTicks=0;
unsigned textureRequestPriority=64;
unsigned textureClock=0,worldTextureBytes=0,boundTextureScale=1,textureFailures=0;
unsigned char smallTexturePage[32*32];
constexpr unsigned ClipFarPolygons=1u<<12; // POLYGON_ATTR: clip far-plane intersections.
bool textureBind(unsigned id,bool coverage=false,bool detail=false) {
    boundTextureScale=1;
    if(!id){glBindTexture(GL_TEXTURE,0);return true;}
    unsigned highKey=detailTextureFile.key(id);if(!highKey)detail=false;
    // A resident close texture also serves its ordinary/distant material alias.
    if(!detail && highKey)for(auto& item:textureCache)if(item.source==int(highKey|0x40000000u)){
        glBindTexture(GL_TEXTURE,item.handle);item.age=++textureClock;item.usedFrame=textureFrame;boundTextureScale=0;return true;}
    const unsigned key=detail ? highKey|0x40000000u : textureFile.key(id);
    CachedTexture* slot=nullptr;auto* oldest=&textureCache[0];
    for(auto& item:textureCache){if(item.source==int(key)){slot=&item;break;}
        if(item.source<0 || (oldest->source>=0 && (item.priority>oldest->priority || (item.priority==oldest->priority && item.age<oldest->age))))oldest=&item;}
    bool promote=slot && !coverage && slot->scale==2;
    if(!slot || promote) {
        if(!textureUploadAllowed || textureUploads>=textureUploadLimit || textureUploadTicks>=TICK_FREQ*2/1000 ||
           (!slot && oldest->source>=0 && oldest->priority<=textureRequestPriority && textureFrame-oldest->usedFrame<6)){
            if(slot){glBindTexture(GL_TEXTURE,slot->handle);boundTextureScale=slot->scale;return true;}
            if(detail)return textureBind(id,coverage,false);
            glBindTexture(GL_TEXTURE,0);return false;
        }
        struct Account {uint64_t start;~Account(){textureUploadTicks+=tickGetCount()-start;}} account{tickGetCount()};
        ++textureUploads;if(!slot)slot=oldest;
        if(detail ? !detailTextureFile.load(id,detailTexturePage) : !textureFile.load(id,texturePage)){
            ++textureFailures;glBindTexture(GL_TEXTURE,0);return false;}
        const unsigned pageFlags=detail ? detailTexturePage.flags : texturePage.flags;
        const bool small=!detail && coverage && !(pageFlags&1);
        const unsigned bytes=detail ? 16384 : small ? 1024 : 4096;
        while(worldTextureBytes-slot->bytes+bytes>144*1024) {
            CachedTexture* victim=nullptr;
            for(auto& item:textureCache)if(&item!=slot && item.bytes &&
                (item.priority>textureRequestPriority || textureFrame-item.usedFrame>=6) &&
                (!victim || item.priority>victim->priority || (item.priority==victim->priority && item.age<victim->age)))victim=&item;
            if(!victim){glBindTexture(GL_TEXTURE,0);return false;}
            glDeleteTextures(1,&victim->handle);glGenTextures(1,&victim->handle);
            worldTextureBytes-=victim->bytes;victim->source=-1;victim->age=victim->bytes=0;
        }
        if(slot->bytes && slot->bytes!=bytes) {
            glDeleteTextures(1,&slot->handle);glGenTextures(1,&slot->handle);
            worldTextureBytes-=slot->bytes;slot->bytes=0;slot->source=-1;
        }
        const unsigned char* pixels=detail ? detailTexturePage.pixels : texturePage.pixels;
        if(small){for(int y=0;y<32;++y)for(int x=0;x<32;++x)smallTexturePage[y*32+x]=texturePage.pixels[y*128+x*2];pixels=smallTexturePage;}
        glBindTexture(GL_TEXTURE,slot->handle);
        int flags=GL_TEXTURE_WRAP_S|GL_TEXTURE_WRAP_T;if(pageFlags&1)flags|=GL_TEXTURE_COLOR0_TRANSPARENT;
        const auto size=detail ? TEXTURE_SIZE_128 : small ? TEXTURE_SIZE_32 : TEXTURE_SIZE_64;
        if(!glTexImage2D(GL_TEXTURE,0,GL_RGB256,size,size,0,flags,pixels)){++textureFailures;glBindTexture(GL_TEXTURE,0);return false;}
        glColorTableEXT(GL_TEXTURE,0,256,0,0,detail ? detailTexturePage.palette : texturePage.palette);
        worldTextureBytes=worldTextureBytes-slot->bytes+bytes;slot->bytes=bytes;slot->scale=detail ? 0 : small ? 2 : 1;slot->source=key;slot->priority=textureRequestPriority;
    } else glBindTexture(GL_TEXTURE,slot->handle);
    slot->age=++textureClock;slot->usedFrame=textureFrame;boundTextureScale=slot->scale;return true;
}
unsigned modulate(unsigned color,unsigned texture) {
    unsigned out=0;for(int k=0;k<3;++k)out|=(((color>>(k*5))&31)*((texture>>(k*5))&31)/31)<<(k*5);return out;
}
struct Camera {int x,y,z;int pitch=2300;};
Camera renderCamera{};
float actorDistance2(slice::Point p){float x=p.x-physics.player.x,z=p.z-physics.player.z;return x*x+z*z;}
bool actorVisible(slice::Point p) {
    if(actorDistance2(p)>32*32)return false;
    float x=p.x-renderCamera.x/64.f,y=p.y+.9f-renderCamera.y/64.f,z=p.z-renderCamera.z/64.f;
    float si=sinLerp(yaw)/4096.f,co=cosLerp(yaw)/4096.f,ps=sinLerp(renderCamera.pitch)/4096.f,pc=cosLerp(renderCamera.pitch)/4096.f;
    float f=x*si-z*co,side=x*co+z*si,depth=f*pc-y*ps,up=y*pc+f*ps;
    return depth>=-3 && std::fabs(side)<=std::max(0.f,depth)*.9f+4 && std::fabs(up)<=std::max(0.f,depth)*.68f+4;
}
void fog(float end) {
    static int previous=-1;
    const int quarter=static_cast<int>(end*4);
    if(quarter==previous) return;
    previous=quarter;end=quarter*.25f;
    const float start=end*.65f;
    auto depth=[](float z) {return slice::Projection::depth(z)*32767;};
    const int offset=static_cast<int>(depth(start));float span=depth(end)-offset;
    int shift=0;while(shift<10 && (1024>>(shift+1))*31>=span) ++shift;
    const int step=1024>>shift;
    const unsigned color=lighting.sky();
    glFogShift(shift);glFogOffset(offset);glFogColor(color&31,(color>>5)&31,(color>>10)&31,31);
    for(int i=0;i<32;++i) {
        const float d=std::min(32767,offset+i*step)/32767.f;
        const float z=slice::Projection::distance(d);
        glFogDensity(i,static_cast<int>(127*std::max(0.f,std::min(1.f,(z-start)/(end-start)))));
    }
}
void mesh(slice::Point p,const RevcTriangle* triangles,int count,int heading,int scale=1,slice::Point tilt={},const RevcUV* uv=nullptr) {
    if(!actorVisible(p) || submitted+count>1980){glBindTexture(GL_TEXTURE,0);return;}
    if(!uv)glBindTexture(GL_TEXTURE,0);
    submitted+=count;
    glPushMatrix();glTranslatef32(static_cast<int>(p.x*64),static_cast<int>((p.y+.03f)*64),static_cast<int>(p.z*64));
    glRotateYi(-heading);
    if(tilt.x)glRotateXi(int(tilt.x*5215.189175f));
    if(tilt.z)glRotateZi(int(tilt.z*5215.189175f));
    glBegin(GL_TRIANGLES);
    for(int i=0;i<count;++i) {
        glColor(uv ? RGB15(31,31,31) : triangles[i].color);
        for(int j=0;j<3;++j) {
            if(uv)glTexCoord2t16(uv[i].uv[j][0],uv[i].uv[j][1]);
            const auto& v=triangles[i].vertex[j];
            glVertex3v16(static_cast<int>(v[0]*scale),static_cast<int>(v[1]*scale),static_cast<int>(v[2]*scale));
        }
    }
    glEnd();glPopMatrix(1);glBindTexture(GL_TEXTURE,0);
}
void fleetMesh(slice::Point position,int model,int heading,slice::Point tilt,float roll,float steering,bool detailed,float door=0,int side=-1,slice::Point body={}) {
    const auto& resource=vehicleModels.model(model);const auto& data=detailed ? resource.detail : resource.far;
    if(!data.count)return;
    fleetTexture.bind();
    mesh(position,vehicleAnimation.pose(data.rig,data.triangles.get(),data.count,roll,steering,door,side,body),data.count,heading,1,tilt,data.uv.get());
}
int fleetTriangles(int model,bool detail){const auto& data=vehicleModels.model(model);return detail ? data.detail.count : data.far.count;}
void parking() {
    if(!campaign.hasTarget())return;
    auto p=campaign.target(world.anchor());
    physics.placeActor(p,4);
    if(std::fabs(p.x-physics.player.x)>15 || std::fabs(p.z-physics.player.z)>15) return;
    glColor(RGB15(31,5,23));glBegin(GL_QUADS);
    // Parking stripes replace the former gold checkpoint cubes.
    for(int side : {-1,1}) {
        int x=static_cast<int>((p.x+side*1.6f)*64),y=static_cast<int>(p.y*64)+3,z=static_cast<int>(p.z*64);
        glVertex3v16(x-4,y,z-190);glVertex3v16(x+4,y,z-190);
        glVertex3v16(x+4,y,z+190);glVertex3v16(x-4,y,z+190);
    }
    glEnd();
}
void drawWater(const Camera& camera) {
    const auto anchor=world.anchor();
    const float x=anchor.x+camera.x/64.f+230.f;
    const float y=-(anchor.z+camera.z/64.f)-1250.f;
    const int lowX=static_cast<int>(std::floor((x-64+400+2048)/32));
    const int highX=static_cast<int>(std::floor((x+64+400+2048)/32));
    const int lowY=static_cast<int>(std::floor((y-64+2048)/32));
    const int highY=static_cast<int>(std::floor((y+64+2048)/32));
    if(!water.texture())return;bool waterTextured=textureBind(water.texture());
    const int waterUV=slice::TexturePlan::uv(1024,boundTextureScale);
    glPolyFmt(POLY_ALPHA(lighting.waterAlpha())|POLY_CULL_NONE|ClipFarPolygons|POLY_FOG|POLY_ID(3));
    glColor(waterTextured ? lighting.water() : modulate(lighting.water(),textureFile.average(water.texture())));glBegin(GL_QUADS);
    for(int gx=lowX;gx<=highX;++gx)for(int gy=lowY;gy<=highY;++gy) {
        float height;if(!water.cell(gx,gy,height))continue;
        const int vx=static_cast<int>(((gx-64)*32-400-230-anchor.x)*64);
        const int vz=static_cast<int>((-((gy-64)*32+1250)-anchor.z)*64);
        const int vy=static_cast<int>((height-.5f-anchor.y)*64);
        glTexCoord2t16(0,0);glVertex3v16(vx,vy,vz);
        glTexCoord2t16(0,waterUV);glVertex3v16(vx,vy,vz-2048);
        glTexCoord2t16(waterUV,waterUV);glVertex3v16(vx+2048,vy,vz-2048);
        glTexCoord2t16(waterUV,0);glVertex3v16(vx+2048,vy,vz);
    }
    glEnd();glBindTexture(GL_TEXTURE,0);
}
void draw(const Camera& camera) {
    // Clipping adds vertices beyond the three submitted per triangle. Watch
    // actual hardware usage as well as the CPU's primitive reservation.
    if(GFX_CONTROL&GL_POLY_OVERFLOW){sceneBudget=std::max(1536,sceneBudget-128);gpuOverflow=true;safeGpuFrames=0;glEnable(GL_POLY_OVERFLOW);}
    renderCamera=camera;textureUploads=0;++textureFrame;textureUploadTicks=0;uint64_t section=tickGetCount();
    glMatrixMode(GL_MODELVIEW);glLoadIdentity();glRotateXi(camera.pitch);glRotateYi(yaw);
    glTranslatef32(-camera.x,-camera.y,-camera.z);
    auto nearPlayer=[](slice::Point p){return actorVisible(p);};
    // Reserve nearby world coverage before admitting optional actor meshes.
    // Plan with the standing-player cost even while driving, so occupancy
    // cannot upgrade the same car at the expense of the surrounding street.
    const auto* residents=population.pedestrians();
    auto anchor=world.anchor();
    const auto* enemies=campaign.npcs();const auto* vehicles=campaign.vehicles();
    bool drawCars[slice::Population::VehicleCapacity]{},drawPeds[slice::Population::PedCount]{};
    bool drawEnemies[4]{},drawMissionCars[3]{},drawGuard=false,drawPassenger=false;
    float eyeX=camera.x/64.f-physics.player.x,eyeY=camera.y/64.f-physics.player.y-1.1f,eyeZ=camera.z/64.f-physics.player.z;
    const bool showPlayer=!job.inCar && !job.transitioning() && eyeX*eyeX+eyeY*eyeY+eyeZ*eyeZ>.36f;
    const bool showDriver=(job.inCar || job.transitioning()) && nearPlayer(job.car);
    int actorPolygons=showPlayer ? playerAnimation.count() : showDriver ? driverAnimation.count() : 0;
    int plannedActors=(showPlayer || showDriver) ? playerAnimation.count() : 0;
    if(nearPlayer(job.car)){int cost=fleetTriangles(job.carModel,false);actorPolygons+=cost;plannedActors+=cost;}
    struct ActorChoice {float distance;int cost;bool* admitted;};
    ActorChoice choices[32];int choiceCount=0;
    auto offer=[&](slice::Point p,int cost,bool& admitted){if(nearPlayer(p))choices[choiceCount++]={actorDistance2(p),cost,&admitted};};
    if(job.guardHealth>0)offer(job.guard,guardAnimation.count(),drawGuard);
    if(campaign.hasPassenger() && !campaign.passengerInCar())offer(campaign.passenger(anchor),guardAnimation.count(),drawPassenger);
    for(int i=0;i<slice::Population::VehicleCapacity;++i){const auto& c=population.vehicles()[i];if(c.active)offer(c.position,fleetTriangles(c.model,false),drawCars[i]);}
    for(int i=0;i<slice::Population::PedCount;++i)if(!missionNear && residents[i].active)offer(residents[i].position,civilianAnimation[i%2].count(),drawPeds[i]);
    for(int i=0;i<campaign.npcCount();++i)if(enemies[i].health>0 && enemies[i].placed)offer({enemies[i].global.x-anchor.x,enemies[i].global.y-anchor.y,enemies[i].global.z-anchor.z},80,drawEnemies[i]);
    for(int i=0;i<campaign.vehicleCount();++i)if(!vehicles[i].borrowed && vehicles[i].health>0 && vehicles[i].placed)offer({vehicles[i].global.x-anchor.x,vehicles[i].global.y-anchor.y,vehicles[i].global.z-anchor.z},fleetTriangles(slice::Garage::missionModel(vehicles[i].model),false),drawMissionCars[i]);
    std::sort(choices,choices+choiceCount,[](const ActorChoice& a,const ActorChoice& b){return a.distance<b.distance;});
    for(int i=0;i<choiceCount;++i)if(slice::RenderBudget::admits(sceneBudget,plannedActors,choices[i].cost)){*choices[i].admitted=true;plannedActors+=choices[i].cost;actorPolygons+=choices[i].cost;}
    int detailedCar=-1;float detailDistance=12*12;int detailCost=0;
    auto detailCandidate=[&](int id,slice::Point p,int model,bool driver){float d=actorDistance2(p);if(nearPlayer(p) && d<detailDistance){detailDistance=d;detailedCar=id;detailCost=fleetTriangles(model,true)-fleetTriangles(model,false)+(driver ? trafficDriverAnimation.count() : 0);}};
    detailCandidate(0,job.car,job.carModel,false);
    for(int i=0;i<slice::Population::VehicleCapacity;++i)if(drawCars[i]){const auto& c=population.vehicles()[i];detailCandidate(1+i,c.position,c.model,c.driver && c.health>0);}
    for(int i=0;i<campaign.vehicleCount();++i)if(drawMissionCars[i]){const auto& c=vehicles[i];detailCandidate(1+slice::Population::VehicleCapacity+i,{c.global.x-anchor.x,c.global.y-anchor.y,c.global.z-anchor.z},slice::Garage::missionModel(c.model),false);}
    if(detailedCar>=0 && slice::RenderBudget::admits(sceneBudget,plannedActors,detailCost))actorPolygons+=detailCost;
    else detailedCar=-1;
    const bool carDetailed=detailedCar==0;
    const int fallback=world.visualLag(physics.player)>10 ? world.groundFallback(physics.player) : 0;
    const int requested=world.renderBudget();
    world.renderBudget(slice::RenderBudget::nearby(sceneBudget,actorPolygons,requested)-fallback);
    drawn=world.visible({camera.x/64.f,camera.y/64.f,camera.z/64.f},
        sinLerp(yaw)/4096.f,cosLerp(yaw)/4096.f,
        sinLerp(camera.pitch)/4096.f,cosLerp(camera.pitch)/4096.f,&physics.player);
    world.renderBudget(requested);
    const int farCount=distant.visible({camera.x/64.f,camera.y/64.f,camera.z/64.f},sinLerp(yaw)/4096.f,cosLerp(yaw)/4096.f,sinLerp(camera.pitch)/4096.f,cosLerp(camera.pitch)/4096.f,physics.player,slice::RenderBudget::distant(sceneBudget,actorPolygons,drawn+fallback));
    renderProfile[0]=(tickGetCount()-section)*1000/TICK_FREQ;section=tickGetCount();
    // Resolve texture residency before submission. Near materials used to
    // inherit 32-pixel pages uploaded by the coverage shell drawn first.
    slice::TexturePlan texturePlan;
    auto offerTexture=[&](const RevcTriangle& t,unsigned id,bool coverage){
        int dx=int(t.vertex[0][0])+t.vertex[1][0]+t.vertex[2][0]-camera.x*3;
        int dy=int(t.vertex[0][1])+t.vertex[1][1]+t.vertex[2][1]-camera.y*3;
        int dz=int(t.vertex[0][2])+t.vertex[1][2]+t.vertex[2][2]-camera.z*3;
        texturePlan.offer(id,std::abs(dx)+std::abs(dy)+std::abs(dz),coverage);
    };
    for(int i=0;i<drawn;++i)offerTexture(world.visibleTriangle(i),world.visibleMaterial(i).texture,false);
    for(int i=0;i<farCount;++i)offerTexture(distant.triangle(i),distant.material(i).texture,true);
    texturePlan.offer(water.texture(),100000,true);texturePlan.sort();
    for(auto& slot:textureCache)slot.priority=64;
    for(unsigned i=0;i<texturePlan.count();++i){const auto& r=texturePlan[i];
        const unsigned key=textureFile.key(r.id),highKey=detailTextureFile.key(r.id);
        for(auto& slot:textureCache)if(slot.source==int(key) || (highKey && slot.source==int(highKey|0x40000000u)))slot.priority=std::min(slot.priority,i);
    }
    for(unsigned i=0;i<texturePlan.count() && textureUploads<textureUploadLimit && textureUploadTicks<TICK_FREQ*2/1000;++i){const auto& r=texturePlan[i];textureRequestPriority=i;textureBind(r.id,r.coverage);}
    if(detailTextureUploadAllowed)for(unsigned i=0;i<texturePlan.count();++i){const auto& r=texturePlan[i];
        if(!r.coverage && r.distance<3072 && detailTextureFile.key(r.id)){textureRequestPriority=i;textureBind(r.id,false,true);break;}}
    textureUploadAllowed=false; // Rendering only binds resident pages; no SD reads.
    fog(world.fogEnd());glPolyFmt(POLY_ALPHA(31)|POLY_CULL_NONE|ClipFarPolygons|POLY_FOG);
    // Exact COL floor coverage bridges a late terrain snapshot; its lower
    // depth lets authored textured scenery replace it without covering feet.
    if(fallback){glBindTexture(GL_TEXTURE,0);glColor(lighting.surface(RGB15(15,15,14)));glBegin(GL_TRIANGLES);
        for(int i=0;i<fallback;++i)for(const auto& v:world.fallbackTriangle(i).vertex)glVertex3v16(v[0],int(v[1])-8,v[2]);glEnd();}
    // The complete coverage shell stays available under the near detail.
    // A tiny depth separation lets coincident original detail win the depth test.
    for(int layer=0;layer<2;++layer) {
        const bool coverage=layer==0;const int count=coverage ? farCount : drawn;
        const uint64_t start=tickGetCount();
        if(coverage) {
            auto rounded=[](int n,int d){return n<0 ? -((-n+d/2)/d) : (n+d/2)/d;};
            const int co=cosLerp(camera.pitch);glPushMatrix();
            glTranslatef32(rounded(sinLerp(yaw)*co*4,1<<24),rounded(-sinLerp(camera.pitch)*4,4096),rounded(-cosLerp(yaw)*co*4,1<<24));
        }
        bool begun=false,textured=false;int previous=-1;
        for(int i=0;i<count;++i) {
            const auto& t=coverage ? distant.triangle(i) : world.visibleTriangle(i);
            const auto& material=coverage ? distant.material(i) : world.visibleMaterial(i);
            if(previous!=material.texture) {
                if(begun)glEnd();begun=false;textured=textureBind(material.texture,coverage);previous=material.texture;
            }
            if(!begun){glBegin(GL_TRIANGLES);begun=true;}
            glColor(textured || !material.texture ? lighting.surface(t.color) : modulate(lighting.surface(t.color),textureFile.average(material.texture)));

            for(int v=0;v<3;++v){glTexCoord2t16(slice::TexturePlan::uv(material.uv[v][0],boundTextureScale),slice::TexturePlan::uv(material.uv[v][1],boundTextureScale));glVertex3v16(t.vertex[v][0],t.vertex[v][1],t.vertex[v][2]);}
        }
        if(begun)glEnd();glBindTexture(GL_TEXTURE,0);if(coverage)glPopMatrix(1);
        renderProfile[coverage ? 2 : 1]=(tickGetCount()-start)*1000/TICK_FREQ;
    }
    submitted=drawn+fallback+farCount+64; // Water, markers and near-plane clipping reserve.
    section=tickGetCount();
    drawWater(camera);
    renderProfile[3]=(tickGetCount()-section)*1000/TICK_FREQ;section=tickGetCount();
    glPolyFmt(POLY_ALPHA(31)|POLY_CULL_NONE|ClipFarPolygons|POLY_FOG);
    if(physics.state!=slice::Game::LoadingFailed) {
        parking();
        if(showPlayer){playerTexture.bind();mesh(physics.player,playerAction ? playerAnimation.mesh() : playerAnimation.poseMesh(playerMotion.first(),playerMotion.firstPhase(),playerMotion.second(),playerMotion.secondPhase(),playerMotion.blend()),playerAnimation.count(),playerMotion.heading(),1,{},playerAnimation.uv());}
        fleetMesh(job.car,job.carModel,job.carYaw,job.carTilt,job.wheelRoll,job.steering(),carDetailed,job.doorAngle(),job.doorSide(),job.bodyTilt);
        if(showDriver) {
            auto clip=slice::Animation::Idle;
            float phase=0;
            if(job.transitioning()) {
                clip=job.entering() ? (job.doorSide()<0 ? slice::Animation::Walk : slice::Animation::Run) : (job.doorSide()<0 ? slice::Animation::Fire : slice::Animation::Reload);
                phase=std::max(0.f,std::min(.999f,(job.transitionPhase()-.2f)/.6f));
            }
            playerTexture.bind();mesh(job.driverPosition(),driverAnimation.poseMesh(clip,phase,clip,phase,0,false),driverAnimation.count(),job.carYaw,1,{},driverAnimation.uv());
        }
        if(drawGuard){pedestrianTexture.bind();mesh(job.guard,guardAnimation.mesh(),guardAnimation.count(),guardYaw,1,{},guardAnimation.uv());}
        if(!job.storyMode && !job.armed) mesh(job.weapon,world.weaponMesh(),world.weaponCount(),0,3);
        if(drawPassenger) {
            auto& animation=campaign.passengerMale() ? buddyAnimation : guardAnimation;
            pedestrianTexture.bind();mesh(campaign.passenger(world.anchor()),animation.meshAt(campaign.passengerMoving() ? (campaign.passengerRunning() ? slice::Animation::Run : slice::Animation::Walk) : slice::Animation::Idle,animationClock),animation.count(),campaign.passengerYaw(),1,{},animation.uv());
        }
        for(int i=0;i<campaign.npcCount();++i)if(drawEnemies[i]){auto& animation=enemies[i].model==slice::Campaign::ChefModel ? chefAnimation : enemies[i].model==slice::Campaign::WorkerModel ? workerAnimation : civilianAnimation[0];
            pedestrianTexture.bind();mesh({enemies[i].global.x-anchor.x,enemies[i].global.y-anchor.y,enemies[i].global.z-anchor.z},animation.meshAt(enemies[i].moving ? slice::Animation::Run : slice::Animation::Idle,animationClock),animation.count(),enemies[i].yaw,1,{},animation.uv());}
        for(int i=0;i<campaign.vehicleCount();++i)if(drawMissionCars[i]) {
            int model=slice::Garage::missionModel(vehicles[i].model);
            fleetMesh({vehicles[i].global.x-anchor.x,vehicles[i].global.y-anchor.y,vehicles[i].global.z-anchor.z},model,vehicles[i].yaw,{},0,0,detailedCar==1+slice::Population::VehicleCapacity+i);
        }
        for(int i=0;i<slice::Population::VehicleCapacity;++i){const auto& car=population.vehicles()[i];if(!drawCars[i])continue;
            bool detailed=detailedCar==1+i;
            fleetMesh(car.position,car.model,car.yaw,car.tilt,car.wheelRoll,car.steering,detailed);
            if(detailed && car.driver && car.health>0){const auto& model=vehicleModels.model(car.model);float a=car.yaw/5215.189175f,si=std::sin(a),co=std::cos(a);auto seat=model.seat;
                slice::Point position={car.position.x+seat.x*co-seat.z*si,car.position.y+seat.y,car.position.z+seat.x*si+seat.z*co};
                pedestrianTexture.bind();mesh(position,trafficDriverAnimation.meshAt(slice::Animation::Idle,0),trafficDriverAnimation.count(),car.yaw,1,car.tilt,trafficDriverAnimation.uv());
            }
        }
        const auto* peds=population.pedestrians();
        for(int i=0;i<slice::Population::PedCount;++i)if(drawPeds[i]) {
            const auto clip=peds[i].moving ? (peds[i].flee ? slice::Animation::Run : slice::Animation::Walk) : slice::Animation::Idle;
            pedestrianTexture.bind();mesh(peds[i].position,civilianAnimation[i%2].meshAt(clip,peds[i].clock),civilianAnimation[i%2].count(),peds[i].yaw,1,{},civilianAnimation[i%2].uv());
        }
        glPolyFmt(POLY_ALPHA(31)|POLY_CULL_NONE|ClipFarPolygons);

        if(job.armed && job.flash && !job.inCar) {
            slice::Point muzzle={physics.player.x+sinLerp(playerMotion.heading())/4096.f*.6f,physics.player.y+1.2f,
                                 physics.player.z-cosLerp(playerMotion.heading())/4096.f*.6f};
            glColor(RGB15(31,28,4));glBegin(GL_TRIANGLES);
            int x=static_cast<int>(muzzle.x*64),y=static_cast<int>(muzzle.y*64),z=static_cast<int>(muzzle.z*64);
            glVertex3v16(x-8,y,z);glVertex3v16(x+8,y,z);glVertex3v16(x,y+12,z);glEnd();
        }
    }
    renderProfile[4]=(tickGetCount()-section)*1000/TICK_FREQ;
    glGetInt(GL_GET_VERTEX_RAM_COUNT,&gpuVertices);glGetInt(GL_GET_POLYGON_RAM_COUNT,&gpuPolygons);
    if(gpuVertices>5600 || gpuPolygons>1880){sceneBudget=std::max(1536,sceneBudget-64);safeGpuFrames=0;}
    else if(++safeGpuFrames>=120 && gpuVertices<4600){sceneBudget=std::min(1760,sceneBudget+32);safeGpuFrames=0;}
    glFlush(0);
}
}
int main(int argc,char** argv) {
    if(isDSiMode())setCpuClock(true);
    // tickGetCount only reads the timer; start it before using elapsed time.
    tickInit();
    videoSetMode(MODE_0_3D);videoSetModeSub(MODE_5_2D);vramSetBankC(VRAM_C_SUB_BG);
    consoleInit(&hud,0,BgType_Text4bpp,BgSize_T_256x256,31,0,false,true);lcdMainOnTop();
    glInit();vramSetBankA(VRAM_A_TEXTURE);vramSetBankB(VRAM_B_TEXTURE_SLOT1);vramSetBankE(VRAM_E_TEX_PALETTE);
    glEnable(GL_ANTIALIAS|GL_FOG|GL_TEXTURE_2D);glClearColor(9,18,25,31);glClearPolyID(63);glClearDepth(0x7fff);
    int handles[TextureCacheSlots];glGenTextures(TextureCacheSlots,handles);for(int i=0;i<TextureCacheSlots;++i)textureCache[i].handle=handles[i];
    glViewport(0,0,255,191);glMatrixMode(GL_PROJECTION);glLoadIdentity();
    gluPerspective(65,256.0/192.0,slice::Projection::Near/64,slice::Projection::Far/64);
    std::printf("VICE CITY DSi - STORY\nLoading city and actors...\n");
    int error=12;
    if(slice::mountGameAssets(argc,argv))error=0;
    hud.fontCurPal=0;BG_PALETTE_SUB[15]=RGB15(31,31,31);
#if !defined(REVC_INSPECT_OCEAN_VIEW) || defined(REVC_INSPECT_KEN_SPAWN)
    if(!error && (!slice::Frontend::movies() || slice::Frontend::run(false)==slice::Frontend::Error))error=30;
#endif
    consoleClear();std::printf("VICE CITY DSi - STORY\nLoading city and actors...\n");
    if(!error && !world.open("nitro:"))error=14;
    if(!error && world.textured() && (!textureFile.open("nitro:/world/textures.vct") || !textureFile.loadAverages("nitro:/world/averages.vcc")))error=19;
    if(!error)detailTextureFile.open("nitro:/world/detail.vct");
    if(!error && (!water.open("nitro:/world/water.vcw") || water.texture()>textureFile.count()))error=20;
    if(!error && !lighting.open("nitro:/world/lighting.vcl"))error=22;
    if(!error) {
        unsigned color=lighting.sky();glClearColor(color&31,(color>>5)&31,(color>>10)&31,31);
    }
    slice::Point preferred=slice::Campaign::spawn(),shift;
#if defined(REVC_INSPECT_OCEAN_VIEW) && !defined(REVC_INSPECT_KEN_SPAWN)
    preferred={slice::missionData::Hotel.x-32,9.5f,slice::missionData::Hotel.z+3};
#endif
    campaign.reset();
    if(!error && !world.update(preferred,shift,true)) error=15;
    preferred.x-=shift.x;preferred.y-=shift.y;preferred.z-=shift.z;
    if(!error && !physics.startExploration(world.triangles(),world.count(),preferred)) error=16;
    if(!error && !job.start(physics,true)) {error=17;physics.state=slice::Game::LoadingFailed;}
    if(!error && (!playerAnimation.open("nitro:/world/player.vca",520) ||
                  !guardAnimation.open("nitro:/world/mercedes.vca",120))) {error=18;physics.state=slice::Game::LoadingFailed;}
    if(!error && (!civilianAnimation[0].open("nitro:/world/male01.vca",80) ||
                  !civilianAnimation[1].open("nitro:/world/hfyst.vca",80) || !population.open("nitro:") ||
                  !population.refresh(physics,world.anchor(),job.car,true,world.lastReadBytes()==0 && distant.readBytes()==0))) {error=23;physics.state=slice::Game::LoadingFailed;}
    if(!error && (!playerTexture.open("nitro:/world/player.atx") || !vehicleModels.open("nitro:") || !driverAnimation.open("nitro:/world/driver.vca",120) || !trafficDriverAnimation.open("nitro:/world/fleet-driver.vca",80))) {error=31;physics.state=slice::Game::LoadingFailed;}
    if(!error && (!fleetTexture.open("nitro:/world/fleet-shared.atx") || !pedestrianTexture.open("nitro:/world/pedestrians.atx"))){error=28;physics.state=slice::Game::LoadingFailed;}
    if(!error){const auto& model=vehicleModels.model(0);job.vehicleGeometry(model.seat,model.radius,model.wheelbase);}
    if(!error)population.gaits(civilianAnimation[0].speed(slice::Animation::Walk),civilianAnimation[1].speed(slice::Animation::Walk),
        civilianAnimation[0].speed(slice::Animation::Run),civilianAnimation[1].speed(slice::Animation::Run));
    if(!error && guardAnimation.speed(slice::Animation::Walk)>0)job.guardSpeed=guardAnimation.speed(slice::Animation::Walk)/60;
    if(!error && !citymap.open("nitro:/world/citymap.vcm")){error=25;physics.state=slice::Game::LoadingFailed;}
    if(!error && (!distant.open("nitro:/world/distant.vcd") || !chefAnimation.open("nitro:/world/chef.vca",80) || !buddyAnimation.open("nitro:/world/igbuddy.vca",80) || !workerAnimation.open("nitro:/world/spandxa.vca",80))) {error=29;physics.state=slice::Game::LoadingFailed;}
    if(!error)yaw=int(std::atan2(job.car.x-physics.player.x,physics.player.z-job.car.z)*5215.189175f)&32767;
#if defined(REVC_INSPECT_OCEAN_VIEW) && !defined(REVC_INSPECT_KEN_SPAWN)
    if(!error){auto a=world.anchor();yaw=int(std::atan2(slice::missionData::Hotel.x-physics.player.x-a.x,physics.player.z+a.z-slice::missionData::Hotel.z)*5215.189175f)&32767;}
#endif
    playerMotion.reset(yaw);
    boundRevision=world.collisionRevision();
    if(error) std::printf("Load failed: %d\n",error);
    hud.fontCurPal=0;BG_PALETTE_SUB[15]=RGB15(31,31,31);
    bgSetPriority(hud.bgId,0);bgShow(hud.bgId);consoleSelect(&hud);
    consoleClear();
#if !defined(REVC_INSPECT_OCEAN_VIEW) || defined(REVC_INSPECT_KEN_SPAWN)
    if(!error) {
        if(!slice::Frontend::story()){error=27;physics.state=slice::Game::LoadingFailed;}
    }
#endif
    if(!error && isDSiMode())world.enableExtendedCache();
    slice::GameplayDisplay::init();
    uint64_t lastTick=tickGetCount(),remainder=0,fpsStart=lastTick;
    unsigned fpsFrames=0,fps=0,frame=0;
    unsigned playerFire=0,guardFire=0;
    uint64_t streamTime=0,drawTime=0,workTime=0,simulationTime=0,cameraTime=0,mapTime=0;
    slice::Point previousFocus{},previousDesired{},cameraPosition{},worldCameraPosition{};
    unsigned cameraRevision=0;
    slice::Point previousStreamPlayer{};int previousMotionTicks=0;bool streamHistory=false;
    while(pmMainLoop()) {
        const uint64_t workStart=tickGetCount();
        scanKeys();const unsigned down=keysDown(),held=keysHeld();
        const uint64_t now=tickGetCount();remainder+=(now-lastTick)*60;lastTick=now;++fpsFrames;
        if(now-fpsStart>=TICK_FREQ) {fps=static_cast<unsigned>(fpsFrames*TICK_FREQ/(now-fpsStart));fpsFrames=0;fpsStart=now;}
        const int elapsed=static_cast<int>(std::min<uint64_t>(remainder/TICK_FREQ,7200));remainder%=TICK_FREQ;
        bool menuNewGame=false;
        if((down&KEY_START) && physics.state!=slice::Game::LoadingFailed) {
            if(physics.state==slice::Game::Paused){physics.pause();citymap.pause(false);}
            else {
                physics.pause();world.cancelStreaming();auto choice=slice::GameplayDisplay::framebuffer([](){return slice::Frontend::run(true);});
                if(choice==slice::Frontend::Error){error=30;physics.state=slice::Game::LoadingFailed;}
                else if(choice==slice::Frontend::Map)citymap.pause(true);
                else {physics.pause();menuNewGame=choice==slice::Frontend::NewGame;}
            }
            lastTick=tickGetCount();remainder=0;streamHistory=false;
        }
        if((down&KEY_B) && physics.state==slice::Game::LoadingFailed)break;
        if((down&KEY_SELECT) || menuNewGame) {
            if((held&KEY_L) && physics.state==slice::Game::Playing) {
                job.reload();
            } else if(physics.state!=slice::Game::LoadingFailed) {
                world.cancelStreaming();
                cameraPitch=2300;
                job.reset();campaign.reset();citymap.pause(false);yaw=int(std::atan2(job.car.x-physics.player.x,physics.player.z-job.car.z)*5215.189175f)&32767;playerAnimation.reset();guardAnimation.reset();playerMotion.reset(yaw);playerAction=false;playerFire=guardFire=0;
                population.reset();
                if(menuNewGame && !slice::GameplayDisplay::framebuffer([](){return slice::Frontend::story();})){error=27;physics.state=slice::Game::LoadingFailed;}
                if(menuNewGame){lastTick=tickGetCount();remainder=0;streamHistory=false;}
            }
        }
        const auto origin=world.anchor();
        const slice::Point globalPlayer={physics.player.x+origin.x,physics.player.y+origin.y,physics.player.z+origin.z};
        touchPosition touch{};if(down&KEY_TOUCH)touchRead(&touch);
        if((down&KEY_TOUCH) && slice::WeaponWheel::button(touch.px,touch.py) && physics.state==slice::Game::Playing && !campaign.talking() && !campaign.briefing()) {
            world.cancelStreaming();slice::WeaponWheel::show(job);citymap.invalidate();
            lastTick=fpsStart=tickGetCount();remainder=0;fpsFrames=0;streamHistory=false;frame=3;continue;
        }
        citymap.input(touch.px,touch.py,globalPlayer,down&KEY_TOUCH,
            physics.state==slice::Game::Paused && (down&KEY_X),physics.state==slice::Game::Paused && (down&KEY_Y));
        const uint64_t streamStart=tickGetCount();
        if(physics.state!=slice::Game::LoadingFailed) {
            slice::Point shift;
            slice::Point streamPlayer=physics.player;
            // Centre the next snapshot ahead of actual travel, balancing the
            // scenery on both sides of a moving player while IO is in flight.
            // Use displacement rather than requested input so walls do not
            // push streaming into geometry the player cannot reach.
            if(streamHistory && previousMotionTicks>0 && physics.state==slice::Game::Playing) {
                float dx=globalPlayer.x-previousStreamPlayer.x,dz=globalPlayer.z-previousStreamPlayer.z;
                float factor=36.f/previousMotionTicks;
                float length=std::sqrt(dx*dx+dz*dz);
                if(length<3) {
                    const float limit=job.inCar ? 24.f : 6.f;
                    if(length*factor>limit)factor=limit/length;
                    streamPlayer.x+=dx*factor;streamPlayer.z+=dz*factor;
                }
            }
            previousStreamPlayer=globalPlayer;streamHistory=true;previousMotionTicks=0;
            if(!world.updateCollision(physics.player,streamPlayer) || !world.update(streamPlayer,shift,false,true,job.inCar)) {error=21;physics.state=slice::Game::LoadingFailed;}
            else {
                physics.rebase(shift);job.rebase(shift);population.rebase(shift);
                if(boundRevision!=world.collisionRevision()) {physics.bindGeometry(world.triangles(),world.count());boundRevision=world.collisionRevision();}
                auto a=world.anchor();
                if(!distant.update({streamPlayer.x+origin.x,streamPlayer.y+origin.y,streamPlayer.z+origin.z},a,true,!world.urgentCollisionRefresh())){error=30;physics.state=slice::Game::LoadingFailed;}
                if(!population.refresh(physics,world.anchor(),job.car,true,world.lastReadBytes()==0 && distant.readBytes()==0)){error=24;physics.state=slice::Game::LoadingFailed;}
            }
        }
        streamTime=tickGetCount()-streamStart;
        const uint64_t simulationStart=tickGetCount();
        if(physics.state==slice::Game::Playing) {
            const slice::Point oldGuard=job.guard;
            const int previousHealth=job.health;
            bool fired=false;

            if(const char* cue=campaign.pendingCutscene()) {
                world.cancelStreaming();static char scenePath[96];std::snprintf(scenePath,sizeof(scenePath),"nitro:/cutscenes/%s.vcv",cue);
                auto result=slice::GameplayDisplay::framebuffer([&](){return slice::Cinematic::play(scenePath,campaign.title());});
                // Optional mission movies fall back to the original text briefing
                // on storage errors; a broken movie must not strand the mission.
                campaign.finishCutscene(result!=slice::Cinematic::Error);
                citymap.invalidate();lastTick=fpsStart=tickGetCount();remainder=0;fpsFrames=0;streamHistory=false;continue;
            }
            if(campaign.talking()) {
                world.cancelStreaming();
                auto result=slice::GameplayDisplay::framebuffer([](){return slice::Cinematic::play("nitro:/cutscenes/int_b.vcv","An Old Friend");});
                if(result==slice::Cinematic::Error) {error=27;physics.state=slice::Game::LoadingFailed;}
                else campaign.finishCall();
                lastTick=fpsStart=tickGetCount();remainder=0;fpsFrames=0;
                continue;
            }
            if(campaign.state()==slice::Campaign::Failed && (down&KEY_R))campaign.retry(job);
            const bool briefing=campaign.briefing();
            if(briefing && (down&(KEY_L|KEY_R)))campaign.advanceBriefing((down&KEY_L)!=0);
            const bool talking=campaign.talking();
            const unsigned shoulders=KEY_L|KEY_R,directions=KEY_UP|KEY_DOWN|KEY_LEFT|KEY_RIGHT;
            const bool interact=(held&shoulders)==shoulders && (down&shoulders) && !(held&directions);
            if(!talking && !briefing && interact && physics.playerGrounded()) {
                bool wasInCar=job.inCar;slice::Garage::interact(physics,job,population,campaign,world.anchor(),&vehicleModels);
                if(wasInCar!=job.inCar){physics.resetPlayerPhysics();playerMotion.settle(job.carYaw);}
            }

            // Bound catch-up after a slow storage frame. Replaying half a second
            // of collision sweeps caused another stall and a large position jump.
            const int motion=std::min(elapsed,4),footMotion=std::min(elapsed,6),turn=((held&KEY_RIGHT)!=0)-((held&KEY_LEFT)!=0);
            previousMotionTicks=job.inCar ? motion : footMotion;
            const int forward=((held&KEY_UP)!=0)-((held&KEY_DOWN)!=0);
            // The face buttons exclusively orbit the gameplay camera. D-pad
            // movement and vehicle steering remain independent of these inputs.
            if(!talking) {
                const int horizontal=((held&KEY_A)!=0)-((held&KEY_Y)!=0);
                const int vertical=slice::cameraVerticalInput(held&KEY_X,held&KEY_B);
                yaw=(yaw+horizontal*160*footMotion)&32767;
                cameraPitch=std::max(-455,std::min(5916,cameraPitch+vertical*100*footMotion));
            }
            if(talking || briefing || job.transitioning()) { /* Dialogue and car entry suspend player actions. */ }
            else if(job.inCar) {
                playerMotion.rest(footMotion);
                job.steer(turn,motion);
                auto beforeCar=job.car;
                job.drive(forward,(held&KEY_L)!=0,sinLerp(job.carYaw)/4096.f,cosLerp(job.carYaw)/4096.f,motion);
                slice::Garage::guardDrive(job,population,campaign,world.anchor(),beforeCar);
            } else {
                const float si=sinLerp(yaw)/4096.f,co=cosLerp(yaw)/4096.f;
                // D-pad movement is relative to the camera; body heading follows
                // the source pedstats heading limit, independently of the camera.
                const int side=turn;
                const float normalization=(forward && side) ? .70710678f : 1.f;
                slice::PlayerMotion::Input input;
                input.x=(forward*si+side*co)*normalization;input.z=(-forward*co+side*si)*normalization;
                input.sprint=!job.armed && (held&KEY_R) && !(held&KEY_L);
                input.walk=(held&(KEY_L|KEY_R))==(KEY_L|KEY_R);
                input.jump=(down&KEY_L) && !(held&(KEY_R|KEY_SELECT));
                for(int tick=0;tick<footMotion;++tick) {
                    auto step=playerMotion.tick(input,physics.playerGrounded());input.jump=false;
                    auto before=physics.player;
                    if(step.x!=0 || step.z!=0 || step.impulse!=0 || !physics.playerGrounded())
                        job.walkPlayer(step.x,step.z,1.f/60,step.impulse);
                    if(campaign.blocks(physics.player,world.anchor()) || population.blocks(physics.player)){physics.player.x=before.x;physics.player.z=before.z;}
                }
                if((held&KEY_R) && !(held&KEY_L)) {
                    float si=sinLerp(playerMotion.heading())/4096.f,co=cosLerp(playerMotion.heading())/4096.f;
                    if(job.armed){fired=job.shoot(si,co);if(fired)campaign.strike(physics,job,world.anchor(),si,co,true);}
                    else fired=campaign.strike(physics,job,world.anchor(),si,co,false);
                }
            }
            job.tick(elapsed);
            campaign.tick(physics,job,world.anchor(),footMotion,false);
            {auto a=world.anchor();missionNear=campaign.nearby({physics.player.x+a.x,physics.player.y+a.y,physics.player.z+a.z});}
            if(campaign.outfit()!=playerOutfit) {
                playerOutfit=campaign.outfit();
                const char* animations[]={"nitro:/world/player.vca","nitro:/world/player2.vca","nitro:/world/player3.vca"};
                const char* textures[]={"nitro:/world/player.atx","nitro:/world/player2.atx","nitro:/world/player3.atx"};
                static const char* drivers[]={"nitro:/world/driver.vca","nitro:/world/driver2.vca","nitro:/world/driver3.vca"};
                if(!playerAnimation.open(animations[playerOutfit-1],520) || !playerTexture.open(textures[playerOutfit-1]) || !driverAnimation.open(drivers[playerOutfit-1],120)) {
                    error=26;physics.state=slice::Game::LoadingFailed;
                }
            }
            animationClock+=motion;
            population.trafficBudget(slice::Population::TrafficCount-campaign.vehicleCount());
            slice::Point obstacles[3];int obstacleCount=0;auto obstacleAnchor=world.anchor();
            for(int i=0;i<campaign.vehicleCount();++i){const auto& c=campaign.vehicles()[i];if(c.placed && !c.borrowed && c.health>0)obstacles[obstacleCount++]={c.global.x-obstacleAnchor.x,c.global.y-obstacleAnchor.y,c.global.z-obstacleAnchor.z};}
            population.obstacles(obstacles,obstacleCount);population.tick(physics,job.car,job.alert,motion);
            slice::Garage::sync(job,population,campaign,world.anchor());
            const unsigned ticks=unsigned(footMotion);
            if(fired)playerFire=job.armed ? 54 : 24;
            if(job.health<previousHealth)guardFire=54;

            const bool guardMoving=std::fabs(job.guard.x-oldGuard.x)+std::fabs(job.guard.z-oldGuard.z)>.001f;
            auto clip=job.reloading() ? slice::Animation::Reload : playerFire ? (job.armed ? slice::Animation::Fire : slice::Animation::Punch) : slice::Animation::Idle;
            playerAction=clip!=slice::Animation::Idle;
            playerAnimation.advance(clip,ticks,fired);
            guardAnimation.advance(guardFire ? slice::Animation::Fire : guardMoving ? slice::Animation::Walk : slice::Animation::Idle,
                                   ticks,job.health<previousHealth);
            playerFire=playerFire>ticks ? playerFire-ticks : 0;
            guardFire=guardFire>ticks ? guardFire-ticks : 0;
            if(job.alert)guardYaw=int(std::atan2(physics.player.x-job.guard.x,job.guard.z-physics.player.z)*5215.189175f)&32767;
        }
        simulationTime=tickGetCount()-simulationStart;
        const uint64_t cameraStart=tickGetCount();
        const bool vehicleCamera=job.inCar || job.transitioning();
        const float distance=vehicleCamera ? 4.72f : 3.05f;
        const float back=distance*cosLerp(cameraPitch)/4096.f;
        const auto subject=job.transitioning() ? job.car : physics.player;
        const slice::Point focus={subject.x,subject.y+1.1f,subject.z};
        const slice::Point desired={subject.x-back*sinLerp(yaw)/4096.f,
                                   focus.y+distance*sinLerp(cameraPitch)/4096.f,subject.z+back*cosLerp(yaw)/4096.f};
        if(cameraRevision!=world.collisionRevision() || focus.x!=previousFocus.x || focus.y!=previousFocus.y || focus.z!=previousFocus.z ||
           desired.x!=previousDesired.x || desired.y!=previousDesired.y || desired.z!=previousDesired.z) {
            worldCameraPosition=physics.cameraPosition(focus,desired);
            previousFocus=focus;previousDesired=desired;cameraRevision=world.collisionRevision();
        }
        cameraPosition=worldCameraPosition;
        if(!vehicleCamera)cameraPosition=slice::cameraOutsideVehicle(focus,cameraPosition,job.car,sinLerp(job.carYaw)/4096.f,cosLerp(job.carYaw)/4096.f);
        for(int i=0;i<slice::Population::VehicleCapacity;++i){const auto& car=population.vehicles()[i];if(car.active)cameraPosition=slice::cameraOutsideVehicle(focus,cameraPosition,car.position,sinLerp(car.yaw)/4096.f,cosLerp(car.yaw)/4096.f);}
        auto cameraAnchor=world.anchor();
        for(int i=0;i<campaign.vehicleCount();++i){const auto& car=campaign.vehicles()[i];if(car.placed && !car.borrowed && car.health>0)cameraPosition=slice::cameraOutsideVehicle(focus,cameraPosition,{car.global.x-cameraAnchor.x,car.global.y-cameraAnchor.y,car.global.z-cameraAnchor.z},0,1,car.model==2 ? 2.8f : 1.9f);}
        Camera camera{static_cast<int>(cameraPosition.x*64),static_cast<int>(cameraPosition.y*64),static_cast<int>(cameraPosition.z*64),cameraPitch};
        cameraTime=tickGetCount()-cameraStart;
        const bool viewMoving=(job.inCar && std::fabs(job.speed)>.003f) || (held&(KEY_UP|KEY_DOWN|KEY_LEFT|KEY_RIGHT|KEY_A|KEY_B|KEY_X|KEY_Y))!=0 ||
            (!job.inCar && (playerMotion.speed()>.05f || !physics.playerGrounded()));
        settledTextureFrames=(!viewMoving && world.lastReadBytes()==0 && distant.readBytes()==0) ? std::min(60u,settledTextureFrames+1) : 0;
        detailTextureUploadAllowed=settledTextureFrames>=20;
        textureUploadAllowed=physics.state!=slice::Game::LoadingFailed;
        textureUploadLimit=(world.lastReadBytes() || distant.readBytes()) ? 1 : viewMoving ? 2 : 4;
        const uint64_t drawStart=tickGetCount();draw(camera);drawTime=tickGetCount()-drawStart;
        workTime=tickGetCount()-workStart;
        // Geometry submission is timed separately from storage. Adjust face count
        // even while streaming, keeping the same fog and distance limits.
        if(frame%30==0 && textureUploads==0 && workTime>TICK_FREQ*16/1000 && drawTime>TICK_FREQ*12/1000)
            world.renderBudget(slice::RenderBudget::slower(world.renderBudget()));
        else if(frame%120==0 && textureUploads==0 && workTime<TICK_FREQ*14/1000 && drawTime<TICK_FREQ*12/1000)
            world.renderBudget(world.renderBudget()+16);
        ++frame;
        if(frame%4==0 || down&(KEY_START|KEY_TOUCH|KEY_X|KEY_Y|KEY_A|KEY_B|KEY_L|KEY_R|KEY_SELECT)) {
            auto anchor=world.anchor();
            slice::Point player={physics.player.x+anchor.x,physics.player.y+anchor.y,physics.player.z+anchor.z};
            auto target=campaign.target(anchor);
            slice::Point objective={target.x+anchor.x,target.y+anchor.y,target.z+anchor.z};
            citymap.arrived(player,job.inCar);
            const uint64_t mapStart=tickGetCount();
            citymap.draw(player,job.inCar ? yaw : playerMotion.heading(),campaign.hasTarget() ? &objective : nullptr,physics.state==slice::Game::Paused);
            if(physics.state==slice::Game::Playing) slice::WeaponWheel::drawButton();
            mapTime=tickGetCount()-mapStart;
            const char* title=physics.state==slice::Game::Paused ? "PAUSED - CITY MAP" : campaign.title();
            std::printf("\x1b[0;0H%-31.31s\x1b[1;0HHP:%3d  $%5d  %3dkmh  FPS:%2u   ",title,job.health,job.cash,int(job.inCar ? std::fabs(job.speed)*216 : playerMotion.speed()*3.6f),fps);
#ifdef REVC_INSPECT_OCEAN_VIEW
            std::printf("\x1b[17;0HTEX %2uKB FAIL:%u     ",worldTextureBytes/1024,textureFailures);
            std::printf("\x1b[18;0HGPU %4dP %4dV O:%u B:%4d",gpuPolygons,gpuVertices,unsigned(gpuOverflow),sceneBudget);
            std::printf("\x1b[19;0HC%3u S%3u K%2u M%2u U%u    ",unsigned(workTime*1000/TICK_FREQ),unsigned(simulationTime*1000/TICK_FREQ),unsigned(cameraTime*1000/TICK_FREQ),unsigned(mapTime*1000/TICK_FREQ),world.lastStreamUnits());
            std::printf("\x1b[0;0HV%2u N%2u F%2u W%u A%2u I%3u  ",renderProfile[0],renderProfile[1],renderProfile[2],renderProfile[3],renderProfile[4],unsigned(streamTime*1000/TICK_FREQ));
#endif
            if(job.armed && !job.inCar)std::printf("\x1b[1;0HHP:%3d $%4d AM:%02d/%02d FPS:%2u   ",job.health,job.cash,job.ammo,job.reserve,fps);
            for(int row=20;row<24;++row)std::printf("\x1b[%d;0H%*s",row,row==23 ? 31 : 32,"");
            if(campaign.talking() || campaign.briefing()) {
                const char* text=campaign.dialogue();const char* newline=std::strchr(text,'\n');
                std::printf("\x1b[20;0H%.*s",newline ? int(newline-text) : int(std::strlen(text)),text);
                if(newline)std::printf("\x1b[21;0H%.32s",newline+1);
                std::printf("\x1b[22;0HR:next %u/%u  L:skip",campaign.page()+1,campaign.pages());
            } else {
                if(citymap.waypoint())objective=citymap.destination();
                const float dx=objective.x-player.x,dz=objective.z-player.z;
                const float ahead=dx*sinLerp(yaw)/4096.f-dz*cosLerp(yaw)/4096.f;
                const float side=dx*cosLerp(yaw)/4096.f+dz*sinLerp(yaw)/4096.f;
                const char* bearing=ahead<0 ? "BEHIND" : std::fabs(side)<2 ? "AHEAD" : side>0 ? "RIGHT" : "LEFT";
                const bool paused=physics.state==slice::Game::Paused;
                const char* instruction=campaign.objective();
                unsigned length=std::strlen(instruction),cut=std::min(32u,length);
                if(!paused && length>32) {
                    while(cut && instruction[cut]!=' ')--cut;
                    if(!cut)cut=32;
                }
                std::printf("\x1b[20;0H%.*s",int(paused ? std::min(31u,length) : cut),instruction);
                const bool twoLines=!paused && length>cut;
                if(twoLines)std::printf("\x1b[21;0H%.32s",instruction+cut+(instruction[cut]==' '));
                if(citymap.waypoint() || campaign.hasTarget())
                    std::printf("\x1b[%d;0H%s %4dm %s",twoLines ? 22 : 21,citymap.waypoint() ? "Waypoint" : "Mission",int(std::sqrt(dx*dx+dz*dz)),bearing);
                if(!twoLines)std::printf("\x1b[22;0H%s",paused ? "X: zoom  Y: clear  Start: resume" : "L:jump R:sprint L+R:car");
            }
            std::printf("\x1b[23;0H%-31.31s",physics.state==slice::Game::Paused ? "Touch:waypoint Select:restart" : "X/B:cam up/down Y/A:left/right");
            if(physics.state==slice::Game::LoadingFailed)std::printf("\x1b[20;0HLOAD FAILED: %d\x1b[21;0HWorld asset stage: %d",error,world.openError());
            if(error==27 && physics.state==slice::Game::LoadingFailed)std::printf("\x1b[21;0HScene error: %d\x1b[22;0H%.31s",slice::Cinematic::lastError,slice::Cinematic::lastPath);
        }
        swiWaitForVBlank();
    }
    slice::GameplayDisplay::suspend();
    population.close();detailTextureFile.close();textureFile.close();world.close();for(auto& slot:textureCache)glDeleteTextures(1,&slot.handle);return 0;
}
