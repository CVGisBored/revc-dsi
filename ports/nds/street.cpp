#include "street.h"
#include <algorithm>
#include <cmath>
#include "vehicle_handling.h"

namespace slice {
constexpr float Street::MaximumSpeed;
constexpr float Street::Acceleration;
constexpr float Street::BrakeDeceleration;
float Street::distance2(Point a,Point b) {return (a.x-b.x)*(a.x-b.x)+(a.z-b.z)*(a.z-b.z);}
bool Street::safeSite(Point& p,bool vehicle) const {
    auto check=[this,vehicle](Point& q){return vehicle ? physics->vehiclePose(q,0,1) : physics->actorMove(q,0,0);};
    if(check(p)) return true;
    const Point seed=p;
    for(int ring=1;ring<=6;++ring) for(int z=-ring;z<=ring;++z) for(int x=-ring;x<=ring;++x) {
        if(std::abs(x)!=ring && std::abs(z)!=ring) continue;
        Point q={seed.x+x*.5f,seed.y,seed.z+z*.5f};
        if(check(q)) {p=q;return true;}
    }
    return false;
}
bool Street::start(Game& game,bool story) {
    storyMode=story;physics=&game;initial=game.player;
    initialCar={initial.x,initial.y,initial.z+6};
    if(!safeSite(initialCar,true) || distance2(initialCar,initial)<16) return false;
    bool route=false;
    const float directions[4][2]={{0,-1},{1,0},{0,1},{-1,0}};
    // Only one short vehicle corridor is checked, no city-wide route search.
    for(auto& d:directions) {
        Point p=initialCar;bool valid=true;
        if(!physics->vehiclePose(p,d[0],-d[1])) continue;
        for(int i=0;i<24;++i) {
            p.x+=d[0]*.25f;p.z+=d[1]*.25f;
            if(!physics->vehiclePose(p,d[0],-d[1])) {valid=false;break;}
        }
        if(valid) {
            initialDestination=p;route=true;
            initialYaw=d[0]>0 ? 8192 : d[0]<0 ? 24576 : d[1]>0 ? 16384 : 0;
            initialGuard={initialCar.x-d[1]*3.5f,initialCar.y,initialCar.z+d[0]*3.5f};
            break;
        }
    }
    if(!route) return false;
    car=initialCar;
    carSine=initialYaw==8192 ? 1 : initialYaw==24576 ? -1 : 0;
    carCosine=initialYaw==0 ? 1 : initialYaw==16384 ? -1 : 0;
    bool spawnFound=false;
    for(float side : {-1.9f,1.9f}) {
        Point p={car.x+side*carCosine-carSine,car.y,car.z+side*carSine+carCosine};
        if(!safeSite(p,false)) continue;
        float dx=p.x-car.x,dz=p.z-car.z;
        if(std::fabs(dx*carCosine+dz*carSine)<1.6f && std::fabs(dx*carSine-dz*carCosine)<3.3f) continue;
        initial=p;spawnFound=true;break;
    }
    if(!spawnFound) return false;
    if(storyMode) {initialWeapon=initial;initialGuard=initial;reset();return true;}
    initialWeapon={initial.x,initial.y,initial.z-1};
    if(!safeSite(initialWeapon,false)) return false;
    bool guardFound=false;
    for(float side : {-3.5f,3.5f}) {
        for(float length : {-4.f,0.f,4.f}) {
            Point candidate={car.x+side*carCosine+length*carSine,car.y,car.z+side*carSine-length*carCosine};
            if(!safeSite(candidate,false) || distance2(candidate,initial)>64) continue;
            Point from=initial,to=candidate;from.y+=1.2f;to.y+=1.1f;
            if(clearShot(from,to)) {initialGuard=candidate;guardFound=true;break;}
        }
        if(guardFound) break;
    }
    for(int ring=3;!guardFound && ring<=8;++ring) {
        for(int z=-ring;!guardFound && z<=ring;++z) for(int x=-ring;!guardFound && x<=ring;++x) {
            if(std::abs(x)!=ring && std::abs(z)!=ring) continue;
            Point candidate={initial.x+x*.5f,initial.y,initial.z+z*.5f};
            if(!physics->actorMove(candidate,0,0)) continue;
            Point from=initial,to=candidate;from.y+=1.2f;to.y+=1.1f;
            if(clearShot(from,to)) {initialGuard=candidate;guardFound=true;}
        }
    }
    if(!guardFound) return false;
    reset();return true;
}
void Street::reset() {
    if(physics)physics->resetPlayerPhysics();
    if(!physics) return;
    physics->player=initial;physics->state=Game::Playing;physics->explore(true);
    car=initialCar;guard=initialGuard;weapon=initialWeapon;destination=initialDestination;
    health=carHealth=100;guardHealth=60;ammo=reserve=cash=alert=0;carYaw=initialYaw;flash=0;
    inCar=armed=pistolOwned=false;phase=Pickup;speed=0;
    wheel=velocityX=velocityZ=0;gear=1;shiftFrames=0;
    carTilt={};bodyTilt={};wheelRoll=pitchVelocity=rollVelocity=0;transitionFrames=0;
    carSine=initialYaw==8192 ? 1 : initialYaw==24576 ? -1 : 0;
    carCosine=initialYaw==0 ? 1 : initialYaw==16384 ? -1 : 0;
    safeYaw=initialYaw;
    carModel=0;missionVehicle=-1;missionToken=0;seatLocal={-.402f,.545f,-.03f};wheelRadius=.332f;axleDistance=3.289f;
    shotCooldown=reloadFrames=enemyCooldown=0;
    if(storyMode) {guardHealth=0;phase=Vehicle;}
}
void Street::rebase(Point shift) {
    auto apply=[shift](Point& p){p.x-=shift.x;p.y-=shift.y;p.z-=shift.z;};
    apply(car);apply(guard);apply(weapon);apply(destination);apply(initial);
    apply(initialCar);apply(initialGuard);apply(initialWeapon);apply(initialDestination);
    apply(entryPosition);apply(exitPosition);
}
void Street::interact() {
    if(!physics || physics->state!=Game::Playing || phase==Failed || transitioning()) return;
    if(inCar) {
        if(std::fabs(speed)>.02f) return;
        for(float side : {-1.f,1.f}) {
            Point exit={car.x+side*1.9f*carCosine,car.y,car.z+side*1.9f*carSine};
            if(physics->actorMove(exit,0,0)) {
                entrySide=int(side);entryPosition=car;exitPosition=exit;
                physics->player=exit;inCar=false;speed=0;velocityX=velocityZ=0;
                enteringCar=false;transitionFrames=90;return;
            }
        }
        return;
    }
    if(!storyMode && !armed && distance2(physics->player,weapon)<2.25f) {
        grantPistol(12,36);phase=Guard;return;
    }
    Point from=physics->player,to=car;from.y+=1;to.y+=1;
    if(carHealth>0 && guardHealth<=0 && distance2(physics->player,car)<12.25f && physics->lineClear(from,to)) {
        entryPosition=physics->player;
        entrySide=((from.x-car.x)*carCosine+(from.z-car.z)*carSine)<0 ? -1 : 1;
        physics->player=car;inCar=true;enteringCar=true;transitionFrames=90;
        if(phase!=Complete) phase=Delivery;
    }
}
void Street::reload() {
    if(physics && physics->state==Game::Playing && armed && !inCar && reserve>0 && ammo<12 && !reloadFrames)
        reloadFrames=70;
}
bool Street::takeVehicle(Point position,int heading,int hitpoints,int model) {
    if(!physics || physics->state!=Game::Playing || inCar || transitioning() || phase==Failed || hitpoints<=0 || distance2(physics->player,position)>12.25f || std::fabs(physics->player.y-position.y)>2)return false;
    Point from=physics->player,to=position;from.y+=1;to.y+=1;
    if(!physics->lineClear(from,to))return false;
    float a=heading/5215.189175f,si=std::sin(a),co=std::cos(a);Point p=position;
    if(!physics->vehiclePose(p,si,co))return false;
    car=position;carYaw=safeYaw=heading;carSine=si;carCosine=co;carHealth=hitpoints;carModel=model;
    speed=velocityX=velocityZ=wheel=wheelRoll=pitchVelocity=rollVelocity=0;gear=1;bodyTilt={};
    entryPosition=physics->player;float dx=entryPosition.x-car.x,dz=entryPosition.z-car.z;
    entrySide=dx*co+dz*si<0 ? -1 : 1;
    physics->player=car;inCar=true;enteringCar=true;transitionFrames=90;if(phase!=Complete)phase=Delivery;
    return true;
}
bool Street::clearShot(Point a,Point b) const {
    if(!physics->lineClear(a,b)) return false;
    auto local=[this](Point p) {float x=p.x-car.x,z=p.z-car.z;
        return Point{x*carCosine+z*carSine,p.y-car.y,x*carSine-z*carCosine};};
    a=local(a);b=local(b);
    const float from[3]={a.x,a.y,a.z},to[3]={b.x,b.y,b.z};
    const float low[3]={-1.15f,.15f,-2.8f},high[3]={1.15f,1.5f,2.8f};
    float enter=.001f,leave=.999f;
    for(int k=0;k<3;++k) {
        float delta=to[k]-from[k];
        if(std::fabs(delta)<1e-6f) {if(from[k]<low[k] || from[k]>high[k]) return true;}
        else {
            float u=(low[k]-from[k])/delta,v=(high[k]-from[k])/delta;
            if(u>v) std::swap(u,v);
            enter=std::max(enter,u);leave=std::min(leave,v);
            if(enter>leave) return true;
        }
    }
    return false;
}
bool Street::shoot(float sine,float cosine) {
    if(!physics || physics->state!=Game::Playing || phase==Failed || !armed || inCar ||
       shotCooldown || reloadFrames || ammo<=0 || !std::isfinite(sine) || !std::isfinite(cosine)) return false;
    --ammo;shotCooldown=15;flash=4;alert=1;
    Point a=physics->player;a.y+=1.2f;
    Point b=guard;b.y+=1.1f;
    const float dx=b.x-a.x,dz=b.z-a.z,range2=dx*dx+dz*dz;
    const float forward=dx*sine-dz*cosine,side=dx*cosine+dz*sine;
    if(guardHealth>0 && range2<=100 && forward>0 && std::fabs(side)<.8f && clearShot(a,b)) {
        guardHealth=std::max(0,guardHealth-25);
        if(guardHealth==0) {phase=Vehicle;alert=0;}
    }
    return true;
}
bool Street::walk(float dx,float dz) {
    if(!physics || inCar || phase==Failed || transitioning()) return false;
    Point old=physics->player;
    bool moved=physics->move(dx,dz);
    const float x=physics->player.x-car.x,z=physics->player.z-car.z;
    if(std::fabs(x*carCosine+z*carSine)<1.4f && std::fabs(x*carSine-z*carCosine)<2.85f) {
        physics->player=old;return false;
    }
    return moved;
}
bool Street::walkPlayer(float dx,float dz,float seconds,float impulse) {
    if(!physics || inCar || phase==Failed || transitioning())return false;
    Point old=physics->player;bool moved=physics->movePlayer(dx,dz,seconds,impulse);
    const float x=physics->player.x-car.x,z=physics->player.z-car.z;
    if(std::fabs(x*carCosine+z*carSine)<1.4f && std::fabs(x*carSine-z*carCosine)<2.85f) {
        physics->player.x=old.x;physics->player.z=old.z;return false;
    }
    return moved;
}

void Street::steer(int direction,int frames) {
    if(!physics || physics->state!=Game::Playing || !inCar || phase==Failed || transitioning())return;
    frames=std::min(4,std::max(0,frames));
    float target=std::max(-1,std::min(1,direction))*.5235988f/(1+std::fabs(speed)*4);
    for(int i=0;i<frames;++i)wheel+=(target-wheel)*.15f;
    if(std::fabs(speed)>.001f) {
        const float maximumTurn=9.81f*.91f/(3600*std::fabs(speed));
        const float turn=std::max(-maximumTurn,std::min(maximumTurn,speed*std::tan(wheel)/axleDistance));
        carYaw=(carYaw+int(turn*5215.189175f*frames))&32767;
    }
}
void Street::drive(int throttle,bool brake,float sine,float cosine,int frames) {
    if(!physics || physics->state!=Game::Playing || !inCar || phase==Failed || transitioning()) return;
    static const SentinelHandling handling;
    frames=std::min(4,std::max(0,frames));throttle=std::max(-1,std::min(1,throttle));
    for(int i=0;i<frames;++i) {
        const float previous=speed;
        const bool braking=brake || (throttle && speed*throttle<0);
        if(braking) speed=std::fabs(speed)<BrakeDeceleration ? 0 : speed-(speed>0 ? BrakeDeceleration : -BrakeDeceleration);
        else if(throttle) speed+=handling.acceleration(speed,throttle,gear);
        else {const float rolling=.00012f;speed=std::fabs(speed)<rolling ? 0 : speed-std::copysign(rolling,speed);}
        speed=handling.drag(speed);
        speed=std::max(-.2f*50/60,std::min(handling.maximum*50/60,speed));
        const float grip=braking ? .3f : .91f*.2f;
        velocityX+=(speed*sine-velocityX)*grip;velocityZ+=(-speed*cosine-velocityZ)*grip;
        if(std::fabs(speed)<.0001f && velocityX*velocityX+velocityZ*velocityZ<.000001f)velocityX=velocityZ=0;
        const bool sameHeading=sine==carSine && cosine==carCosine;
        if(sameHeading && std::fabs(speed)<=.0001f && !velocityX && !velocityZ) {speed=0;car=physics->player;settleBody(0);continue;}
        Point pose=physics->player;
        const Point before=physics->player;
        // Movement validates the new footprint; only test rotation separately
        // when the heading changes. A stopped car needs no collision query.
        const bool rotated=sameHeading || physics->vehiclePose(pose,sine,cosine,&carTilt);
        bool moved=rotated;
        if(rotated && (velocityX || velocityZ)) {
            moved=physics->vehicleMove(velocityX,velocityZ,sine,cosine,&carTilt);
            if(distance2(before,physics->player)<(velocityX*velocityX+velocityZ*velocityZ)*.9f) moved=false;
        }
        if(!moved) {
            carHealth=std::max(0,carHealth-(std::fabs(previous)>.09f ? 10 : 0));speed=0;
            velocityX=velocityZ=0;
        }
        if(rotated) {carSine=sine;carCosine=cosine;safeYaw=carYaw;}
        else carYaw=safeYaw;
        car=physics->player;
        const float travel=std::sqrt(distance2(before,car));
        wheelRoll=std::fmod(wheelRoll-std::copysign(travel/wheelRadius,speed ? speed : previous),6.2831853f);
        settleBody((speed-previous)*3600);
    }
}
void Street::settleBody(float acceleration) {
    const float pitchTarget=std::max(-.07f,std::min(.07f,acceleration*.008f));
    const float rollTarget=std::max(-.09f,std::min(.09f,speed*speed*std::tan(wheel)*.3f));
    pitchVelocity+=(pitchTarget-bodyTilt.x)*.06f-pitchVelocity*.26f;
    rollVelocity+=(rollTarget-bodyTilt.z)*.06f-rollVelocity*.26f;
    bodyTilt.x+=pitchVelocity;bodyTilt.z+=rollVelocity;
    if(std::fabs(bodyTilt.x)+std::fabs(pitchVelocity)<.00001f)bodyTilt.x=pitchVelocity=0;
    if(std::fabs(bodyTilt.z)+std::fabs(rollVelocity)<.00001f)bodyTilt.z=rollVelocity=0;
}
void Street::tick(int frames) {
    if(!physics || physics->state!=Game::Playing || phase==Failed) return;
    frames=std::max(0,std::min(frames,7200));
    if(!inCar || transitioning())for(int i=0;i<std::min(frames,4);++i)settleBody(0);
    transitionFrames=std::max(0,transitionFrames-frames);
    shotCooldown=std::max(0,shotCooldown-frames);flash=std::max(0,flash-frames);
    if(reloadFrames) {
        reloadFrames=std::max(0,reloadFrames-frames);
        if(!reloadFrames) {int count=std::min(12-ammo,reserve);ammo+=count;reserve-=count;}
    }
    enemyCooldown=std::max(0,enemyCooldown-frames);
    if(guardHealth>0 && alert && phase!=Complete) {
        Point target=physics->player;float dx=target.x-guard.x,dz=target.z-guard.z;
        const float range=std::sqrt(dx*dx+dz*dz);
        if(range>3 && range<12) {
            const float speed=guardSpeed*std::min(frames,4)/range;
            physics->actorMove(guard,dx*speed,dz*speed);
        }
        Point a=guard,b=target;a.y+=1.1f;b.y+=1.1f;
        if(!enemyCooldown && range<=8 && clearShot(a,b)) {
            health=std::max(0,health-10);enemyCooldown=90;
        }
    }
    if(health<=0 || (!storyMode && carHealth<=0)) {phase=Failed;speed=0;return;}
    // A wreck in free roaming must still permit exiting and taking another car.
    if(carHealth<=0) {speed=velocityX=velocityZ=0;if(inCar && !transitioning())interact();}
    if(!storyMode && phase==Delivery && inCar && distance2(car,destination)<2.25f && std::fabs(speed)<.02f) {
        phase=Complete;cash+=250;alert=0;
    }
}
float Street::doorAngle() const {
    if(!transitioning())return 0;
    const float t=transitionPhase();
    return 1.256637f*std::min(1.f,std::min(t/.22f,(1-t)/.22f));
}
Point Street::driverPosition() const {
    Point seat={car.x+seatLocal.x*carCosine-seatLocal.z*carSine,car.y+seatLocal.y,car.z+seatLocal.x*carSine+seatLocal.z*carCosine};
    if(!transitioning())return seat;
    float t=std::max(0.f,std::min(1.f,(transitionPhase()-.2f)/.6f));
    t=t*t*(3-2*t);
    Point standing=enteringCar ? entryPosition : exitPosition;standing.y+=1.0f;
    Point a=enteringCar ? standing : seat,b=enteringCar ? seat : standing;
    return {a.x+(b.x-a.x)*t,a.y+(b.y-a.y)*t,a.z+(b.z-a.z)*t};
}
const char* Street::objective() const {
    switch(phase) {
    case Pickup:return "Find pistol; A to pick up";
    case Guard:return "Defeat the armed guard";
    case Vehicle:return "Enter Sentinel with A";
    case Delivery:return "Drive to drop-off; park";
    case Complete:return "Job done! Earned $250";
    case Failed:return "Mission failed: Select retry";
    }
    return "";
}
}
