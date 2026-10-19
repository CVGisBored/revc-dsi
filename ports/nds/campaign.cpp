#include "campaign.h"
#include "campaign_data.h"
#include "ken_missions_data.h"
#include "expanded_missions_data.h"
#include <cmath>
#include <algorithm>
#include <cstdio>
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
Point Campaign::giver() const {return missionNumber==7 ? extraData::Avery : missionNumber>=5 ? extraData::Cortez : missionData::Office;}
Point Campaign::spawn(){return missionData::Spawn;}
void Campaign::clearSetpiece(){++vehicleEpoch;npcCountValue=vehicleCountValue=0;for(auto& p:people)p=Npc{};for(auto& p:cars)p=Vehicle{};}
void Campaign::reset(){*this=Campaign{};passengerGlobal=missionData::Passenger;}
void Campaign::finishCall(){if(stage==Phone){stage=FirstComplete;completedCount=1;stageClock=0;}}
bool Campaign::hasTarget() const {return stage!=Phone && stage!=Briefing && stage!=Passed && stage!=FirstComplete && stage!=CampaignComplete && stage!=Failed;}
bool Campaign::hasPassenger() const {return (stage==Escort || stage==Escape || stage==AmmoShop || stage==ReturnHotel) && passengerPlaced;}
Point Campaign::target(Point anchor) const {
    Point p=giver();
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
void Campaign::advanceBriefing(bool skip){if(stage==Briefing && (skip || ++phonePage>=pages()))startObjectives();}
void Campaign::pass(Street& actors,int reward){stage=Passed;stageClock=0;actors.cash+=reward;actors.alert=0;completedCount=missionNumber+1;clearSetpiece();aboard=passengerPlaced=false;}
void Campaign::retry(Street& actors){if(stage!=Failed)return;actors.health=actors.carHealth=100;actors.phase=Street::Vehicle;actors.alert=0;actors.guardHealth=0;clearSetpiece();aboard=passengerPlaced=false;stage=Office;stageClock=0;}
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
    if(game.state!=Game::Playing || stage==CampaignComplete || stage==Failed)return;
    frames=std::max(0,std::min(frames,30));attackCooldown=std::max(0,attackCooldown-frames);
    if(actors.health<=0 || (actors.inCar && actors.carHealth<=0)){stage=Failed;return;}
    if(stage==FirstComplete || stage==Passed){if((stageClock+=frames)>=180){if(stage==Passed)++missionNumber;stage=missionNumber>MissionCount ? CampaignComplete : Office;stageClock=0;}return;}
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
        if(game.placeActor(q,5)){v.global=add(q,anchor);v.placed=true;}}
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
    if(stage==Hotel)return "In the Beginning...";if(stage==Phone)return "An Old Friend";
    if(stage==FirstComplete)return "Opening mission complete";if(stage==Failed)return "Mission failed - R to retry";
    if(stage==CampaignComplete)return "Mission chain complete";
    static const char* names[]={"The Party","Back Alley Brawl","Jury Fury","Riot","Treacherous Swine","Mall Shootout","Four Iron"};return names[std::min(MissionCount-1,missionNumber-1)];
}
const char* Campaign::objective() const {
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
    case CampaignComplete:return "Eight missions complete. Free roam.";case Failed:return "R: retry mission / Select: restart";
    }
    return "";
}
const char* Campaign::dialogue() const {
    if(stage==Phone && phonePage<missionData::PhonePages)return missionData::Phone[phonePage];
    if(stage==Briefing && missionNumber>=1 && missionNumber<=4){const auto& b=kenData::Briefings[missionNumber-1];if(phonePage<b.count)return b.pages[phonePage];}
    if(stage==Briefing && missionNumber>=5 && missionNumber<=MissionCount){const auto& b=extraData::Briefings[missionNumber-5];if(phonePage<b.count)return b.pages[phonePage];}return "";
}
unsigned Campaign::pages() const {return stage==Briefing ? (missionNumber<=4 ? kenData::Briefings[missionNumber-1].count : extraData::Briefings[missionNumber-5].count) : missionData::PhonePages;}
}
