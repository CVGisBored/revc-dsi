#pragma once
#include "gameplay.h"

namespace slice {
// First standalone mission systems; the original GTA scripts are not running.
class Street {
public:
    enum Phase { Pickup, Guard, Vehicle, Delivery, Complete, Failed };
    bool start(Game& game,bool story=false);
    void reset();
    void rebase(Point shift);
    void interact();
    bool takeVehicle(Point position,int heading,int health,int model);
    void vehicleGeometry(Point seat,float radius,float wheelbase) {seatLocal=seat;wheelRadius=radius;axleDistance=wheelbase;}
    void stopAt(Point position) {car=position;if(physics)physics->player=position;speed=velocityX=velocityZ=0;}
    void reload();
    bool shoot(float sine,float cosine);
    bool walk(float dx,float dz);
    bool walkPlayer(float dx,float dz,float seconds,float impulse=0);
    void steer(int direction,int frames);
    void drive(int throttle,bool brake,float sine,float cosine,int frames);
    void tick(int frames);
    const char* objective() const;
    Point car{},guard{},weapon{},destination{};
    Point carTilt{};
    Point bodyTilt{};
    float wheelRoll=0;
    float steering() const {return wheel;}
    float doorAngle() const;
    bool transitioning() const {return transitionFrames>0;}
    bool entering() const {return enteringCar;}
    int doorSide() const {return entrySide;}
    float transitionPhase() const {return 1-transitionFrames/90.f;}
    Point driverPosition() const;
    int health=0,carHealth=0,guardHealth=0,ammo=0,reserve=0,cash=0,alert=0;
    int carYaw=0,flash=0;
    int carModel=0,missionVehicle=-1;unsigned missionToken=0;
    bool inCar=false,armed=false;
    bool pistolOwned=false;
    bool hasPistol() const {return pistolOwned || armed;}
    void grantPistol(int clip,int spare) {pistolOwned=armed=true;ammo=clip;reserve=spare;}
    bool selectWeapon(bool pistol) {
        if(pistol && !hasPistol())return false;
        pistolOwned=hasPistol();armed=pistol;reloadFrames=0;return true;
    }
    bool storyMode=false;
    Phase phase=Pickup;
    float speed=0;
    // SENTINEL handling.cfg: 165 km/h, 24 engine acceleration, RWD, 10.17 braking, 30 degree steering.
    static constexpr float MaximumSpeed=165.f/3.6f/60.f;
    static constexpr float Acceleration=24.f*.4f*.5f/(60.f*60.f);
    static constexpr float BrakeDeceleration=10.17f/(60.f*60.f);
    int gear=1;
    float guardSpeed=1.1324831f/60;
    bool reloading() const {return reloadFrames>0;}
private:
    Game* physics=nullptr;
    Point initial{},initialCar{},initialGuard{},initialWeapon{},initialDestination{};
    float carSine=0,carCosine=1;
    int shotCooldown=0,reloadFrames=0,enemyCooldown=0;
    int initialYaw=0;
    int safeYaw=0;
    float wheel=0,velocityX=0,velocityZ=0;
    int shiftFrames=0;
    int transitionFrames=0,entrySide=-1;
    bool enteringCar=false;
    Point entryPosition{},exitPosition{};
    float pitchVelocity=0,rollVelocity=0;
    Point seatLocal={-.402f,.545f,-.03f};float wheelRadius=.332f,axleDistance=3.289f;
    bool safeSite(Point& p,bool vehicle) const;
    bool clearShot(Point a,Point b) const;
    void settleBody(float acceleration);
    static float distance2(Point a,Point b);
};
}
