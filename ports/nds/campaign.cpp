#include "campaign.h"
#include "vehicle_models.h"
#include "campaign_data.h"
#include "ken_missions_data.h"
#include "expanded_missions_data.h"
#include <cmath>
#include <algorithm>
#include <cstdio>
#include <cstring>
namespace slice {
namespace {
Point subtract(Point a,Point b){return {a.x-b.x,a.y-b.y,a.z-b.z};}
Point add(Point a,Point b){return {a.x+b.x,a.y+b.y,a.z+b.z};}
float distance2(Point a,Point b){return (a.x-b.x)*(a.x-b.x)+(a.z-b.z)*(a.z-b.z);}
bool placeNear(Game& game,Point& p,float tolerance) {
    if(game.placeActor(p,tolerance))return true;const Point original=p;
    for(int ring=1;ring<=6;++ring)for(int dir=0;dir<8;++dir){float angle=dir*.785398163f;Point q={original.x+std::sin(angle)*ring*.5f,original.y,original.z+std::cos(angle)*ring*.5f};if(game.placeActor(q,tolerance)){p=q;return true;}}
    return false;
}
bool within(Point a,Point b,float x,float z,float h){return std::fabs(a.x-b.x)<=x && std::fabs(a.z-b.z)<=z && std::fabs(a.y-b.y)<=h;}
}

void Campaign::finishCutscene(bool played){
    sceneCue=nullptr;
    if(libraryIndex>=0){
        // Stay at the briefing when a movie fails. It always has a manual exit.
        if(stage==LibraryBriefing && played)advanceBriefing(true);
    }else if(played && briefing())advanceBriefing(true);
}
bool Campaign::selectMission(unsigned index){
    if(index>=MissionLibrary::count())return false;
    const auto& m=MissionLibrary::mission(index);if(m.kind==MissionLibrary::Catalog)return false;
    clearSetpiece();sceneCue=nullptr;aboard=passengerPlaced=dressed=false;stageClock=phonePage=0;
    activityStarted=shortFunds=airborne=false;workTicks=flightTicks=0;
    if(m.kind==MissionLibrary::Adapted){libraryIndex=-1;missionNumber=std::max(1,m.adapted);stage=m.adapted ? Office : Hotel;return true;}
    if(!m.routeCount)return false;
    libraryIndex=index;libraryStep=libraryScene=0;stage=LibraryBriefing;if(!activity())cueLibraryScene();return true;
}
void Campaign::cueLibraryScene(){
    if(libraryIndex<0 || sceneCue)return;
    const auto& m=MissionLibrary::mission(libraryIndex);
    while(libraryScene<m.movieCount){
        const auto& candidate=MissionLibrary::movie(m.movies[libraryScene]);
        const bool ending=!std::strcmp(candidate.name,"gspot") || !std::strcmp(candidate.name,"fist") || !std::strcmp(candidate.name,"finale") || !std::strcmp(candidate.name,"ass_2") || !std::strcmp(candidate.name,"col_5b") || !std::strcmp(candidate.name,"bank_3b");
        if(ending && libraryStep<m.routeCount+(m.action ? 1u : 0u))return;
        if(!ending && libraryScene && libraryStep*m.movieCount<m.routeCount*libraryScene)return;
        const auto& movie=MissionLibrary::movie(m.movies[libraryScene++]);
        if(movie.available){sceneCue=movie.name;return;}
    }
}
void Campaign::tickLibrary(Game& game,Street& actors,Point anchor,int frames,bool action){
    const auto& m=MissionLibrary::mission(libraryIndex);
    if(sceneCue)return;
    if(stage==LibraryOffer){if(!actors.inCar && within(add(game.player,anchor),m.start,6,6,8)){stage=LibraryBriefing;cueLibraryScene();}return;}
    if(stage==LibraryBriefing){if(action)advanceBriefing();return;}
    if(stage==LibraryPassed){
        if((stageClock+=frames)<180)return;
        if(activity()){libraryIndex=-1;stage=FreeRoam;return;}
        const int next=MissionLibrary::nextStory(libraryIndex);
        if(next<0){libraryIndex=-1;stage=CampaignComplete;}else{selectMission(next);stage=LibraryOffer;sceneCue=nullptr;libraryScene=0;}
        return;
    }
    if(activity()){tickActivity(game,actors,anchor,frames,action);return;}
    if(stage==LibraryCombat){
        bool alive=false;for(int i=0;i<npcCountValue;++i)alive|=people[i].health>0;
        for(int i=0;i<vehicleCountValue;++i)alive|=cars[i].health>0;
        if(alive)return;clearSetpiece();stage=LibraryRoute;++libraryStep;
    }
    if(stage!=LibraryRoute)return;
    if(libraryStep<m.routeCount && within(add(game.player,anchor),m.route[libraryStep],6,6,8))++libraryStep;
    if(libraryStep==m.routeCount && m.action){
        // Source mission locations, bounded stand-in combat. Reuse the existing
        // actor/car pools so content breadth does not increase rendering load.
        clearSetpiece();const Point p=m.route[m.routeCount-1];
        if(m.action==1){npcCountValue=std::min(4u,m.targets);for(int i=0;i<npcCountValue;++i){people[i].global={p.x+(i%2 ? 1.25f : -1.25f),p.y,p.z+(i/2 ? 1.25f : -1.25f)};people[i].health=75;people[i].hostile=true;}}
        else {vehicleCountValue=std::min(3u,m.targets);for(int i=0;i<vehicleCountValue;++i){cars[i].global={p.x+(int(i)-1)*3.5f,p.y,p.z+3};cars[i].health=100;cars[i].model=i%2;}}
        actors.grantPistol(12,std::max(actors.reserve,180));stage=LibraryCombat;return;
    }
    cueLibraryScene();if(sceneCue || libraryStep<m.routeCount)return;
    // Early routes have their own completion label; they do not claim SCM completion.
    if(!finished[m.id]){finished[m.id]=true;++completedCount;actors.cash+=m.reward;}
    actors.alert=0;stage=LibraryPassed;stageClock=0;
}

bool Campaign::activity() const {
    return libraryIndex>=0 && MissionLibrary::mission(libraryIndex).kind==MissionLibrary::Activity;
}

bool Campaign::activityInput(Point global) const {
    if(!activity() || stage!=LibraryRoute)return false;
    const auto& m=MissionLibrary::mission(libraryIndex);
    if(!activityStarted || m.action==MissionLibrary::Purchase)return within(global,m.start,8,8,8);
    return (m.action==MissionLibrary::Fire || m.action==MissionLibrary::Sales) &&
        libraryStep<m.routeCount && within(global,m.route[libraryStep],11,11,6);
}

void Campaign::leaveAircraft(Game& game,Point anchor) {
    if(!airborne)return;
    game.player=subtract(launchGround,anchor);
    game.resetPlayerPhysics();airborne=false;
}

void Campaign::fly(Game& game,Point anchor,float dx,float dz,int climb,int ticks,int heading) {
    if(!airborne || stage!=LibraryRoute)return;
    // World positions stay local to the streaming anchor, as for the player.
    // Sweep the cabin centre to stop at loaded structures, including rooftops.
    flightYaw=heading;
    Point next=game.player;
    next.x+=dx*.28f*ticks;next.z+=dz*.28f*ticks;
    next.y=std::max(2.f-anchor.y,std::min(190.f-anchor.y,next.y+climb*.18f*ticks));
    const Point lift={0,1.1f,0};
    if(game.lineClear(add(game.player,lift),add(next,lift)))game.player=next;
    Point floor=game.player;
    floor.y+=1.2f;
    if(game.placeActor(floor,5))game.player.y=std::max(game.player.y,floor.y+.18f);
}

void Campaign::completeActivity(Street& actors) {
    const auto& m=MissionLibrary::mission(libraryIndex);
    if(!finished[m.id]){finished[m.id]=true;++completedCount;}
    // Services and time trials pay for each completed run. A purchase has no payout.
    actors.cash+=m.reward;
    clearSetpiece();stage=LibraryPassed;stageClock=0;
}

void Campaign::tickActivity(Game& game,Street& actors,Point anchor,int frames,bool action) {
    const auto& m=MissionLibrary::mission(libraryIndex);
    const Point here=add(game.player,anchor);
    if(stage!=LibraryRoute && stage!=LibraryCombat)return;
    if(m.action==MissionLibrary::Purchase){
        if(!finished[m.id]){
            shortFunds=actors.cash<int(m.targets);
            if(!action || actors.inCar || actors.transitioning() || !within(here,m.start,8,8,8) || shortFunds)return;
            actors.cash-=int(m.targets);finished[m.id]=true;++completedCount;
            libraryStep=1;cueLibraryScene();
            if(sceneCue)return;
        }
        completeActivity(actors);return;
    }
    const int serviceModel=m.action==MissionLibrary::Fire ? VehicleModels::FireTruck :
        m.action==MissionLibrary::Patrol ? VehicleModels::Police : VehicleModels::MrWhoopee;
    if(!activityStarted){
        if(m.action!=MissionLibrary::Flight && !vehicleCountValue){
            vehicleCountValue=1;cars[0].global=m.start;cars[0].model=serviceModel;cars[0].health=100;
        }
        if(!within(here,m.start,8,8,8) || !action || actors.transitioning())return;
        if(m.action==MissionLibrary::Flight){
            if(actors.inCar)return;
            launchGround=here;airborne=true;game.player.y+=1.5f;
        }else{
            if(!actors.inCar || actors.carModel!=serviceModel)return;
            clearSetpiece();
            if(m.action==MissionLibrary::Patrol)actors.grantPistol(12,std::max(actors.reserve,72));
        }
        activityStarted=true;return;
    }
    if(m.action==MissionLibrary::Flight){
        flightTicks+=frames;flightAltitude=int(std::round(here.y));
        const auto& course=MissionLibrary::course(m.id);
        if(libraryStep<course.count){
            if(within(here,course.points[libraryStep],7,7,5))++libraryStep;
        }else if(within(here,launchGround,7,7,2)){
            leaveAircraft(game,anchor);completeActivity(actors);
        }
        return;
    }
    const Point location=m.route[libraryStep];
    if(m.action==MissionLibrary::Patrol){
        if(stage==LibraryCombat){
            if(people[0].health>0)return;
        }else{
            if(!within(here,location,24,24,8))return;
            beginFight(location,Civilian,true);stage=LibraryCombat;return;
        }
    }else if(m.action==MissionLibrary::Fire){
        if(!vehicleCountValue){vehicleCountValue=1;cars[0].global=location;cars[0].health=100;}
        if(!actors.inCar || actors.carModel!=serviceModel || actors.transitioning() || !action || std::fabs(actors.speed)>.025f ||
           !within(here,location,10,10,6))return;
        workTicks+=frames;if(workTicks<90)return;
    }else{
        if(!npcCountValue){npcCountValue=1;people[0].global={location.x+1.4f,location.y,location.z};people[0].health=100;}
        if(!actors.inCar || actors.carModel!=serviceModel || actors.transitioning() || std::fabs(actors.speed)>.025f || !action ||
           !within(here,location,7,7,5))return;
    }
    clearSetpiece();workTicks=0;stage=LibraryRoute;
    if(++libraryStep==m.routeCount)completeActivity(actors);
}

const char* Campaign::activityObjective() const {
    const auto& m=MissionLibrary::mission(libraryIndex);
    if(stage==LibraryPassed && m.action==MissionLibrary::Flight){
        std::snprintf(statusText,sizeof(statusText),"Course complete in %u:%02u. +$%u",flightTicks/3600,(flightTicks/60)%60,m.reward);
        return statusText;
    }
    if(stage==LibraryPassed)return m.action==MissionLibrary::Purchase ? "Property owned" : "Job complete. Payment received.";
    if(m.action==MissionLibrary::Purchase){
        std::snprintf(statusText,sizeof(statusText),shortFunds ? "Need $%u to buy this property." : "On foot at marker: R to buy $%u",m.targets);
    }else if(!activityStarted)return m.action==MissionLibrary::Flight ? "Go to launch site on foot. R: fly" : "Take marked vehicle (L+R). R at dispatch to start";
    else if(m.action==MissionLibrary::Flight){
        const auto& c=MissionLibrary::course(m.id);
        if(libraryStep>=c.count)return "Return to launch. L to land.";
        std::snprintf(statusText,sizeof(statusText),"Gate %u/%u. Height %d/%dm. Time %us",libraryStep+1,c.count,flightAltitude,int(std::round(c.points[libraryStep].y)),flightTicks/60);
    }else if(m.action==MissionLibrary::Fire)
        std::snprintf(statusText,sizeof(statusText),"Fire %u/%u. Stop truck. Hold R: hose %u%%",libraryStep+1,m.routeCount,100u*workTicks/90);
    else if(m.action==MissionLibrary::Patrol)
        std::snprintf(statusText,sizeof(statusText),"Suspect %u/%u. %s",libraryStep+1,m.routeCount,stage==LibraryCombat ? "On foot: R to attack" : "Go to dispatch.");
    else std::snprintf(statusText,sizeof(statusText),"Customer %u/%u. Stop car near buyer. R: sell",libraryStep+1,m.routeCount);
    return statusText;
}

Point Campaign::giver() const {return missionNumber==7 ? extraData::Avery : missionNumber>=5 ? extraData::Cortez : missionData::Office;}
Point Campaign::spawn(){return missionData::Spawn;}
void Campaign::clearSetpiece(){++vehicleEpoch;npcCountValue=vehicleCountValue=0;for(auto& p:people)p=Npc{};for(auto& p:cars)p=Vehicle{};}
void Campaign::reset(){*this=Campaign{};passengerGlobal=missionData::Passenger;}
void Campaign::finishCall(){if(stage==Phone){stage=FirstComplete;if(!finished[2]){finished[1]=finished[2]=true;++completedCount;}stageClock=0;}}
bool Campaign::hasTarget() const {return stage!=FreeRoam && stage!=Phone && stage!=Briefing && stage!=Passed && stage!=FirstComplete && stage!=CampaignComplete && stage!=Failed && stage!=LibraryBriefing && stage!=LibraryPassed;}
bool Campaign::hasPassenger() const {return (stage==Escort || stage==Escape || stage==AmmoShop || stage==ReturnHotel) && passengerPlaced;}
Point Campaign::target(Point anchor) const {
    Point p=giver();
    if(libraryIndex>=0){const auto& m=MissionLibrary::mission(libraryIndex);
        if(activity() && !activityStarted)return subtract(m.start,anchor);
        if(airborne){const auto& course=MissionLibrary::course(m.id);return subtract(libraryStep<course.count ? course.points[libraryStep] : launchGround,anchor);}
        if(stage==LibraryCombat){for(int i=0;i<npcCountValue;++i)if(people[i].health>0)return subtract(people[i].global,anchor);for(int i=0;i<vehicleCountValue;++i)if(cars[i].health>0)return subtract(cars[i].global,anchor);}
        return subtract(stage==LibraryOffer ? m.start : m.route[std::min(libraryStep,m.routeCount-1)],anchor);}
    switch(stage) {
    case Hotel:case Phone:case ReturnHotel:p=missionData::Hotel;break;
    case Clothes:case RiotClothes:p=missionData::Clothes;break;
    case Yacht:p=missionData::Yacht;break;
    case Escort:p=missionData::Club;break;
    case SwineEntrance:case SwineEscape:p=extraData::Penthouse;break;
    case SwineFight:case MallFight:case GolfFight:p=npcCountValue ? people[0].global : p;break;
    case MallMeet:case ChipPickup:p=extraData::Courier;break;
    case ChipDelivery:p=extraData::Cortez;break;
    case GolfClothes:p=extraData::GolfClothes;break;
    case GolfEntrance:p=extraData::GolfEntrance;break;
    case Malibu:p=kenData::Malibu;break;
    case ChefFight:case PhonePickup:case Escape:p=kenData::Chef;break;
    case AmmoShop:p=kenData::Ammo;break;
    case JuryOne:p=vehicleCountValue ? cars[0].global : kenData::JurorCars[0];break;
    case JuryTwo:p=vehicleCountValue>1 ? cars[1].global : kenData::JurorCars[1];break;
    case RiotWorkers:p=kenData::Depot;break;
    case RiotVans:
        p=kenData::Vans[0];for(int i=0;i<vehicleCountValue;++i)if(cars[i].health>0){p=cars[i].global;break;}break;
    default:break;
    }
    if((stage==Escort || stage==AmmoShop || stage==ReturnHotel) && !aboard && passengerPlaced && distance2(passengerGlobal,p)>1600)p=passengerGlobal;
    return subtract(p,anchor);
}
bool Campaign::nearby(Point p) const {for(int i=0;i<npcCountValue;++i)if(people[i].health>0 && distance2(p,people[i].global)<1600)return true;for(int i=0;i<vehicleCountValue;++i)if(cars[i].health>0 && distance2(p,cars[i].global)<1600)return true;return false;}
bool Campaign::blocks(Point local,Point anchor) const {
    Point p=add(local,anchor);
    for(int i=0;i<npcCountValue;++i)if(people[i].health>0 && people[i].placed && std::fabs(p.y-people[i].global.y)<1.6f && distance2(p,people[i].global)<.49f)return true;
    for(int i=0;i<vehicleCountValue;++i)if(!cars[i].borrowed && cars[i].health>0 && cars[i].placed && std::fabs(p.y-cars[i].global.y)<1.8f && std::fabs(p.x-cars[i].global.x)<1.45f && std::fabs(p.z-cars[i].global.z)<2.95f)return true;
    return hasPassenger() && !aboard && std::fabs(p.y-passengerGlobal.y)<1.6f && distance2(p,passengerGlobal)<.49f;
}
Point Campaign::passenger(Point anchor) const {return subtract(passengerGlobal,anchor);}
bool Campaign::syncVehicle(int i,unsigned token,Point local,Point anchor,int heading,int& health) {
    if(token!=vehicleEpoch || i<0 || i>=vehicleCountValue || !cars[i].borrowed)return false;
    auto& car=cars[i];health=std::min(health,car.health);car.health=health;car.global=add(local,anchor);car.yaw=heading;return true;
}
void Campaign::beginFight(Point p,Model model,bool hostile){clearSetpiece();npcCountValue=1;people[0].global=p;people[0].health=100;people[0].model=model;people[0].hostile=hostile;}
void Campaign::startObjectives(){
    phonePage=0;stageClock=0;clearSetpiece();
    if(missionNumber==1)stage=Clothes;
    else if(missionNumber==2){stage=Malibu;dressed=false;aboard=passengerPlaced=false;}
    else if(missionNumber==3){stage=JuryOne;vehicleCountValue=2;npcCountValue=2;for(int i=0;i<2;++i){cars[i].global=kenData::JurorCars[i];cars[i].health=100;cars[i].model=i==0 ? 0 : 1;people[i].global=kenData::Jurors[i];people[i].health=100;}}
    else if(missionNumber==4){stage=RiotClothes;dressed=false;}
    else if(missionNumber==5){stage=SwineEntrance;dressed=false;}
    else if(missionNumber==6){stage=MallMeet;dressed=false;}
    else {stage=GolfClothes;dressed=false;}
}
void Campaign::advanceBriefing(bool skip){if(stage==LibraryBriefing){stage=LibraryRoute;phonePage=0;return;}if(stage==Briefing && (skip || ++phonePage>=pages()))startObjectives();}
void Campaign::pass(Street& actors,int reward){stage=Passed;stageClock=0;static const unsigned ids[]={3,4,5,6,7,8,18};const auto id=ids[missionNumber-1];if(!finished[id]){actors.cash+=reward;finished[id]=true;++completedCount;}actors.alert=0;clearSetpiece();aboard=passengerPlaced=false;}
void Campaign::retry(Street& actors){if(stage!=Failed)return;actors.health=actors.carHealth=100;actors.phase=Street::Vehicle;actors.alert=0;actors.guardHealth=0;clearSetpiece();aboard=passengerPlaced=false;stage=libraryIndex>=0 ? LibraryBriefing : Office;libraryStep=libraryScene=0;sceneCue=nullptr;activityStarted=shortFunds=airborne=false;workTicks=flightTicks=0;if(libraryIndex>=0 && !activity())cueLibraryScene();stageClock=0;}
void Campaign::follow(Game& game,Street& actors,Point anchor,int frames){
    Point q=passenger(anchor);followerMoving=followerRunning=false;
    if(!passengerPlaced){if(!game.placeActor(q,4))return;passengerPlaced=true;}
    if(aboard){
        if(actors.inCar)q=game.player;
        else {Point exit={actors.car.x+2.2f,actors.car.y,actors.car.z};if(!game.placeActor(exit,3)){exit.x=actors.car.x-2.2f;if(!game.placeActor(exit,3))return;}q=exit;aboard=false;}
    } else {
        float d2=distance2(q,game.player);
        if(actors.inCar && d2<100 && std::fabs(actors.speed)<.02f){aboard=true;q=game.player;}
        else if(d2>2.25f){float dx=game.player.x-q.x,dz=game.player.z-q.z,d=std::sqrt(d2);passengerYaw_=int(std::atan2(dx,-dz)*5215.189175f)&32767;
            float step=std::min((d>4 ? 6.1f : 1.25f)/60*std::min(30,std::max(0,frames)),d-1.5f);Point before=q;game.actorMove(q,dx/d*step,dz/d*step);followerMoving=distance2(before,q)>.000001f;followerRunning=d>4;}
    }
    passengerGlobal=add(q,anchor);
}
void Campaign::tick(Game& game,Street& actors,Point anchor,int frames,bool action){
    if(game.state!=Game::Playing || stage==CampaignComplete || stage==FreeRoam || stage==Failed)return;
    frames=std::max(0,std::min(frames,30));attackCooldown=std::max(0,attackCooldown-frames);
    if(actors.health<=0 || (actors.inCar && actors.carHealth<=0)){leaveAircraft(game,anchor);stage=Failed;return;}
    if(libraryIndex>=0){tickLibrary(game,actors,anchor,frames,action);if(stage!=LibraryCombat && !(activity() && setpiece()))return;}
    if(stage==FirstComplete || stage==Passed){if((stageClock+=frames)>=180){if(stage==Passed)++missionNumber;stage=Office;if(missionNumber>MissionCount){int next=MissionLibrary::nextStory();if(next>=0){selectMission(next);stage=LibraryOffer;sceneCue=nullptr;libraryScene=0;}else stage=CampaignComplete;}stageClock=0;}return;}
    if(stage==Briefing){if(action)advanceBriefing();return;}
    const Point p=add(game.player,anchor);
    if(stage==Escort || stage==Escape || stage==AmmoShop || stage==ReturnHotel)follow(game,actors,anchor,frames);
    for(int i=0;i<npcCountValue;++i){auto& n=people[i];n.moving=false;if(n.health<=0)continue;Point q=subtract(n.global,anchor);
        if(distance2(q,game.player)>80*80)continue;
        if(!n.placed){if(!placeNear(game,q,5))continue;n.placed=true;}
        n.cooldown=std::max(0,n.cooldown-frames);
        if(n.fleeing && (stage==SwineFight || stage==GolfFight)) {
            float dx=q.x-game.player.x,dz=q.z-game.player.z,d=std::sqrt(dx*dx+dz*dz);
            const Point home=subtract(stage==SwineFight ? extraData::Gonzalez : extraData::GolfTarget,anchor);
            if(d>0.1f && d<8 && distance2(q,home)<100){auto before=q;game.actorMove(q,dx/d*1.9f/60*frames,dz/d*1.9f/60*frames);n.moving=distance2(before,q)>.00001f;n.yaw=int(std::atan2(dx,-dz)*5215.189175f)&32767;}
        }
        if(n.hostile){float dx=game.player.x-q.x,dz=game.player.z-q.z,d=std::sqrt(dx*dx+dz*dz);n.yaw=int(std::atan2(dx,-dz)*5215.189175f)&32767;
            if(d>1.4f && d<24){auto before=q;game.actorMove(q,dx/d*std::min(d-1.3f,2.7f/60*frames),dz/d*std::min(d-1.3f,2.7f/60*frames));n.moving=distance2(before,q)>.00001f;}
            if(d<1.9f && !n.cooldown && !actors.inCar){Point a=q,b=game.player;a.y+=1;b.y+=1;if(game.lineClear(a,b)){actors.health=std::max(0,actors.health-6);n.cooldown=60;}}
        }
        n.global=add(q,anchor);
    }
    for(int i=0;i<vehicleCountValue;++i){auto& v=cars[i];if(v.placed)continue;Point q=subtract(v.global,anchor);if(distance2(q,game.player)>70*70)continue;
        if(libraryIndex>=0 ? placeNear(game,q,5) : game.placeActor(q,5)){v.global=add(q,anchor);v.placed=true;}}
    switch(stage){
    case Hotel:if(!actors.inCar && within(p,missionData::Hotel,1.8f,1.8f,1.5f)){stage=Phone;phonePage=0;}break;
    case Phone:if(action && ++phonePage>=missionData::PhonePages)finishCall();break;
    case Office:if(!actors.inCar && within(p,giver(),2.5f,2.5f,3)){stage=Briefing;phonePage=0;
        static const char* const intros[]={"law_1a","law_2a","law_3","law_4","col_1","col_2","tex_1"};sceneCue=intros[missionNumber-1];
        if(missionNumber==3 || missionNumber==4)actors.selectWeapon(false);}break;
    case Clothes:if(!actors.inCar && within(p,missionData::Clothes,2,2,3)){stage=Yacht;dressed=true;}break;
    case Yacht:if((!actors.inCar || std::fabs(actors.speed)<.02f) && within(p,missionData::Yacht,4,4,4)){stage=Escort;sceneCue="law_1b";passengerGlobal=missionData::Passenger;aboard=passengerPlaced=false;}break;
    case Escort:if(within(p,missionData::Club,3,3,3) && std::fabs(actors.speed)<.02f && (aboard || within(passengerGlobal,missionData::Club,5,5,3)))pass(actors,100);break;
    case Malibu:if(within(p,kenData::Malibu,3,3,4)){stage=ChefFight;sceneCue="law_2b";beginFight(kenData::Chef,ChefModel,true);}break;
    case ChefFight:if(people[0].health<=0){stage=PhonePickup;clearSetpiece();}break;
    case PhonePickup:if(!actors.inCar && within(p,kenData::Chef,2.5f,2.5f,4)){
        stage=Escape;sceneCue="law_2c";clearSetpiece();npcCountValue=3;for(int i=0;i<3;++i){people[i].global=kenData::Thugs[i];people[i].health=75;people[i].hostile=true;}
        passengerGlobal=kenData::Lance;passengerPlaced=aboard=false;actors.grantPistol(12,48);
    }break;
    case Escape:{int alive=0;for(int i=0;i<npcCountValue;++i)alive+=people[i].health>0;if(!alive){stage=AmmoShop;clearSetpiece();}}break;
    case AmmoShop:if(within(p,kenData::Ammo,5,5,4) && (aboard || distance2(p,passengerGlobal)<100)){stage=ReturnHotel;actors.ammo=12;actors.reserve=std::max(actors.reserve,60);}break;
    case ReturnHotel:if(!actors.inCar && within(p,missionData::Hotel,1.8f,1.8f,2) && (aboard || distance2(p,passengerGlobal)<144))pass(actors,200);break;
    case JuryOne:case JuryTwo:
        if(people[0].health<=0 || people[1].health<=0){stage=Failed;break;}
        if(cars[0].health<=0 && cars[1].health<=0)pass(actors,400);
        else stage=cars[0].health<=0 ? JuryTwo : JuryOne;break;
    case RiotClothes:if(!actors.inCar && within(p,kenData::RiotClothes,2,2,3)){dressed=true;stage=RiotWorkers;clearSetpiece();npcCountValue=4;for(int i=0;i<4;++i){people[i].global=kenData::Workers[i];people[i].health=100;people[i].model=WorkerModel;}}break;
    case RiotWorkers:{int provoked=0;for(int i=0;i<npcCountValue;++i)provoked+=people[i].hit;if(provoked>=4){stage=RiotVans;clearSetpiece();vehicleCountValue=3;for(int i=0;i<3;++i){cars[i].global=kenData::Vans[i];cars[i].health=150;cars[i].model=2;}actors.grantPistol(12,std::max(actors.reserve,72));}}break;
    case SwineEntrance:
        if(!actors.inCar && within(p,extraData::Penthouse,3,3,4)){
            stage=SwineFight;beginFight(extraData::Gonzalez,Civilian,false);people[0].fleeing=true;
            npcCountValue=3;for(int i=1;i<3;++i){people[i].global=extraData::Gonzalez;people[i].global.x-=i*1.1f;people[i].global.z-=i;people[i].health=75;people[i].hostile=true;}
            game.player=subtract(extraData::RoofEntry,anchor);game.resetPlayerPhysics();
        }break;
    case SwineFight:if(people[0].health<=0){stage=SwineEscape;clearSetpiece();actors.alert=2;stageClock=0;game.player=subtract(extraData::Penthouse,anchor);game.resetPlayerPhysics();}break;
    case SwineEscape:if(distance2(p,extraData::Penthouse)>25*25){stageClock+=frames;if(stageClock>=900)pass(actors,250);}break;
    case MallMeet:if(!actors.inCar && within(p,extraData::Courier,6,6,4)){
        stage=MallFight;beginFight(extraData::Courier,Civilian,true);npcCountValue=3;
        for(int i=1;i<3;++i){people[i].global=extraData::Courier;people[i].global.x+=i*1.3f;people[i].global.z+=1.5f;people[i].health=75;people[i].hostile=true;}
    }break;
    case MallFight:{int alive=0;for(int i=0;i<npcCountValue;++i)alive+=people[i].health>0;if(!alive){stage=ChipPickup;clearSetpiece();}}break;
    case ChipPickup:if(!actors.inCar && within(p,extraData::Courier,3,3,4))stage=ChipDelivery;break;
    case ChipDelivery:if(!actors.inCar && within(p,extraData::Cortez,2.5f,2.5f,3))pass(actors,500);break;
    case GolfClothes:if(!actors.inCar && within(p,extraData::GolfClothes,2,2,3)){stage=GolfEntrance;actors.selectWeapon(false);}break;
    case GolfEntrance:if(!actors.inCar && within(p,extraData::GolfEntrance,3,3,4)){
        stage=GolfFight;beginFight(extraData::GolfTarget,Civilian,false);people[0].fleeing=true;npcCountValue=4;
        for(int i=1;i<4;++i){people[i].global=extraData::GolfGuards[i-1];people[i].health=75;people[i].hostile=true;}
    }break;
    case GolfFight:if(people[0].health<=0)pass(actors,500);break;
    case RiotVans:{int alive=0;for(int i=0;i<vehicleCountValue;++i)alive+=cars[i].health>0;if(!alive)pass(actors,1000);}break;
    default:break;
    }
}
bool Campaign::strike(Game& game,Street& actors,Point anchor,float sine,float cosine,bool bullet){
    if(!setpiece() || briefing() || stage==Failed || attackCooldown || actors.inCar)return false;
    attackCooldown=bullet ? 0 : 24;const Point source=add(game.player,anchor);float best=bullet ? 30.f : 3.6f;int hit=-1;bool vehicle=false;
    auto consider=[&](Point target,int index,bool car){float dx=target.x-source.x,dz=target.z-source.z;float forward=dx*sine-dz*cosine,side=dx*cosine+dz*sine;float range=std::sqrt(dx*dx+dz*dz);
        if(forward<-.2f || range>best || std::fabs(side)>(car ? 2.4f : bullet ? .9f : 1.35f))return;
        Point a=game.player,b=subtract(target,anchor);a.y+=1.1f;b.y+=1.1f;
        if(!game.lineClear(a,b))return;best=range;hit=index;vehicle=car;};
    for(int i=0;i<npcCountValue;++i)if(people[i].health>0 && people[i].placed)consider(people[i].global,i,false);
    for(int i=0;i<vehicleCountValue;++i)if(cars[i].health>0 && cars[i].placed)consider(cars[i].global,i,true);
    if(hit>=0){if(vehicle)cars[hit].health=std::max(0,cars[hit].health-(bullet ? 35 : 25));else{auto& n=people[hit];n.health=std::max(0,n.health-(bullet ? 35 : 25));n.hit=true;if(stage==RiotWorkers)n.hostile=true;}}
    if(hit<0 && !bullet)attackCooldown=0;
    return hit>=0;
}
const char* Campaign::title() const {
    if(libraryIndex>=0 && stage!=Failed)return MissionLibrary::mission(libraryIndex).title;
    if(stage==FreeRoam)return "Vice City";
    if(stage==Hotel)return "In the Beginning...";if(stage==Phone)return "An Old Friend";
    if(stage==FirstComplete)return "Opening mission complete";if(stage==Failed)return "Mission failed - R to retry";
    if(stage==CampaignComplete)return "Mission chain complete";
    static const char* names[]={"The Party","Back Alley Brawl","Jury Fury","Riot","Treacherous Swine","Mall Shootout","Four Iron"};return names[std::min(MissionCount-1,missionNumber-1)];
}
const char* Campaign::objective() const {
    if(libraryIndex>=0 && stage!=Failed){
        if(activity())return activityObjective();
        if(stage==LibraryOffer)return "Go to the contact. Early adaptation.";
        if(stage==LibraryBriefing)return "Early route. R: begin / L: begin";
        if(stage==LibraryPassed)return "ADAPTATION COMPLETE - next contact";
        if(stage==LibraryCombat)return npcCountValue ? "Defeat attackers. R: fire." : "Disable target vehicles. R: fire.";
        std::snprintf(statusText,sizeof(statusText),"Early route: checkpoint %u/%u",libraryStep+1,MissionLibrary::mission(libraryIndex).routeCount);return statusText;
    }
    switch(stage){
    case Hotel:return "Go to Ocean View. Enter on foot.";case Phone:return "Watch the hotel phone call.";
    case FirstComplete:return "Ocean View reached. Ken is next.";case Office:return missionNumber==7 ? "Visit Avery at the building site." : missionNumber>=5 ? "Visit Cortez at the yacht on foot." : "Visit Ken's office on foot.";
    case Briefing:return "R: next briefing / L: skip";case Clothes:return missionData::ClothesText;case Yacht:return missionData::YachtText;
    case Escort:return aboard ? missionData::ClubText : "Get Mercedes into your car.";
    case Malibu:return "Visit the Malibu Club entrance.";case ChefFight:std::snprintf(statusText,sizeof(statusText),"Chef: %d HP. R: punch nearby.",people[0].health);return statusText;
    case PhonePickup:return "Collect the chef's phone on foot.";case Escape:{int down=0;for(int i=0;i<npcCountValue;++i)down+=people[i].health<=0;std::snprintf(statusText,sizeof(statusText),"Attackers: %d/3 defeated. R: fire.",down);return statusText;}
    case AmmoShop:return "Take Lance to Ammu-Nation.";case ReturnHotel:return "Return to Ocean View with Lance.";
    case JuryOne:case JuryTwo:{int damaged=0;for(int i=0;i<vehicleCountValue;++i)damaged+=cars[i].health<=0;std::snprintf(statusText,sizeof(statusText),"Cars: %d/2. Do not kill jurors.",damaged);return statusText;}
    case RiotClothes:return "Change into work clothes at Rafael's.";case RiotWorkers:{int hit=0;for(int i=0;i<npcCountValue;++i)hit+=people[i].hit;std::snprintf(statusText,sizeof(statusText),"Workers provoked: %d/4. R: punch.",hit);return statusText;}
    case RiotVans:{int destroyed=0;for(int i=0;i<vehicleCountValue;++i)destroyed+=cars[i].health<=0;std::snprintf(statusText,sizeof(statusText),"Vans destroyed: %d/3. R: fire.",destroyed);return statusText;}case Passed:return "MISSION PASSED! Next contact soon.";
    case SwineEntrance:return "Enter Gonzalez's penthouse on foot.";
    case SwineFight:return "Eliminate Gonzalez and get out.";
    case SwineEscape:std::snprintf(statusText,sizeof(statusText),"Leave the area. Lay low: %ds",(900-stageClock+59)/60);return statusText;
    case MallMeet:return "Meet the courier upstairs in the mall.";
    case MallFight:return "Survive the ambush. Defeat attackers.";
    case ChipPickup:return "Collect the courier's chips on foot.";
    case ChipDelivery:return "Deliver the chips to Cortez's yacht.";
    case GolfClothes:return "Collect your country club membership.";
    case GolfEntrance:return "Enter Leaf Links country club on foot.";
    case GolfFight:return "Find and eliminate the businessman.";
    case LibraryOffer:case LibraryBriefing:case LibraryRoute:case LibraryCombat:case LibraryPassed:return "";
    case FreeRoam:return "Free roam. Start: missions";
    case CampaignComplete:return "Story routes complete. Free roam.";case Failed:return "R: retry mission / Select: restart";
    }
    return "";
}
const char* Campaign::dialogue() const {
    if(stage==LibraryBriefing && activity()){
        switch(MissionLibrary::mission(libraryIndex).action){
        case MissionLibrary::Purchase:return "Visit the marked property.\nBuy on foot with cash using R.";
        case MissionLibrary::Fire:return "Take the marked fire truck.\nStop by each fire. Hold R: hose.";
        case MissionLibrary::Patrol:return "Take the marked police cruiser.\nStop three suspects on foot.";
        case MissionLibrary::Sales:return "Take the marked Mr. Whoopee.\nStop near each buyer. Press R.";
        default:return "Fly the marked gates, then land.\nD-pad: fly. R: climb. L: descend.";
        }
    }
    if(stage==LibraryBriefing)return "Early mission adaptation. Full\noriginal scripting still pending.";
    if(stage==Phone && phonePage<missionData::PhonePages)return missionData::Phone[phonePage];
    if(stage==Briefing && missionNumber>=1 && missionNumber<=4){const auto& b=kenData::Briefings[missionNumber-1];if(phonePage<b.count)return b.pages[phonePage];}
    if(stage==Briefing && missionNumber>=5 && missionNumber<=MissionCount){const auto& b=extraData::Briefings[missionNumber-5];if(phonePage<b.count)return b.pages[phonePage];}return "";
}
unsigned Campaign::pages() const {if(stage==LibraryBriefing)return 1;return stage==Briefing ? (missionNumber<=4 ? kenData::Briefings[missionNumber-1].count : extraData::Briefings[missionNumber-5].count) : missionData::PhonePages;}
}
