#pragma once
#include "street.h"
namespace slice {
// Reviewed, bounded mission adaptations; this does not execute the full SCM VM.
class Campaign {
public:
    enum Stage {Hotel,Phone,Office,Clothes,Yacht,Escort,Passed,FirstComplete,Failed,
        Briefing,Malibu,ChefFight,PhonePickup,Escape,AmmoShop,ReturnHotel,JuryOne,JuryTwo,RiotClothes,RiotWorkers,RiotVans,SwineEntrance,SwineFight,SwineEscape,MallMeet,MallFight,ChipPickup,ChipDelivery,GolfClothes,GolfEntrance,GolfFight,CampaignComplete};
    enum Model {Civilian,ChefModel,WorkerModel};
    struct Npc {Point global{};int health=0,yaw=0,cooldown=0;Model model=Civilian;bool placed=false,hostile=false,hit=false,moving=false,fleeing=false;};
    struct Vehicle {Point global{};int health=0,model=0,yaw=0;bool placed=false,borrowed=false;};
    static Point spawn();
    void reset();void finishCall();
    void tick(Game& game,Street& actors,Point anchor,int frames,bool action);
    bool strike(Game& game,Street& actors,Point anchor,float sine,float cosine,bool bullet);
    void advanceBriefing(bool skip=false);
    void retry(Street& actors);
    Point target(Point anchor) const;Point passenger(Point anchor) const;
    bool hasTarget() const;
    bool hasPassenger() const;
    bool passengerMale() const {return stage!=Escort;}
    bool passengerInCar() const {return aboard;}
    bool passengerMoving() const {return followerMoving;}
    bool passengerRunning() const {return followerRunning;}
    bool suit() const {return dressed;}
    unsigned outfit() const {return dressed ? (missionNumber==4 ? 3u : 2u) : 1u;}
    static constexpr unsigned MissionCount=7;
    const char* pendingCutscene() const {return sceneCue;}
    void finishCutscene(bool played=true) {sceneCue=nullptr;if(played && briefing())advanceBriefing(true);}
    bool talking() const {return stage==Phone;}
    bool briefing() const {return stage==Briefing;}
    bool nearby(Point global) const;
    bool blocks(Point local,Point anchor) const;
    bool setpiece() const {return npcCountValue || vehicleCountValue;}
    const Npc* npcs() const {return people;}
    const Vehicle* vehicles() const {return cars;}
    int npcCount() const {return npcCountValue;}
    int vehicleCount() const {return vehicleCountValue;}
    unsigned vehicleToken() const {return vehicleEpoch;}
    void borrowVehicle(int i) {if(i>=0 && i<vehicleCountValue)cars[i].borrowed=true;}
    bool syncVehicle(int i,unsigned token,Point local,Point anchor,int yaw,int& health);
    unsigned mission() const {return missionNumber;}
    unsigned completed() const {return completedCount;}
    const char* title() const;const char* objective() const;const char* dialogue() const;
    unsigned page() const {return phonePage;}
    unsigned pages() const;
    Stage state() const {return stage;}
    int passengerYaw() const {return passengerYaw_;}
private:
    const char* sceneCue=nullptr;
    Point giver() const;
    Stage stage=Hotel;unsigned missionNumber=1,completedCount=0,phonePage=0;
    bool dressed=false,aboard=false,passengerPlaced=false,followerMoving=false,followerRunning=false;
    Point passengerGlobal{};int passengerYaw_=0,stageClock=0,attackCooldown=0;
    mutable char statusText[64]{};
    Npc people[4]{};Vehicle cars[3]{};int npcCountValue=0,vehicleCountValue=0;
    unsigned vehicleEpoch=0;
    void clearSetpiece();void startObjectives();void pass(Street& actors,int reward);
    void beginFight(Point p,Model model,bool hostile);
    void follow(Game& game,Street& actors,Point anchor,int frames);
};
}
