#include "player_motion.h"
#include "player_motion_data.h"
#include <algorithm>
#include <cmath>
namespace slice {
namespace {
constexpr float Dt=1.f/60.f,Pi=3.14159265358979323846f;
float duration(Animation::Clip clip){return playerData::tracks[clip].duration;}
float sample(Animation::Clip clip,float phase) {
    const auto& track=playerData::tracks[clip];float t=std::max(0.f,std::min(1.f,phase))*track.duration;
    unsigned hi=1;while(hi+1<track.count && track.keys[hi].time<t)++hi;
    const auto& a=track.keys[hi-1];const auto& b=track.keys[hi];
    float mix=b.time>a.time ? std::max(0.f,std::min(1.f,(t-a.time)/(b.time-a.time))) : 0;
    return a.forward+(b.forward-a.forward)*mix;
}
float travel(Animation::Clip clip,float from,float to) {
    // Support wrap without discarding the distance in the last part of a cycle.
    return sample(clip,to-std::floor(to))-sample(clip,from-std::floor(from))+
        (std::floor(to)-std::floor(from))*(sample(clip,1)-sample(clip,0));
}
float oneShot(Animation::Clip clip,float from,float to) {
    return sample(clip,std::min(1.f,to/duration(clip)))-sample(clip,std::min(1.f,from/duration(clip)));
}
float limitAngle(float a){while(a>Pi)a-=2*Pi;while(a<-Pi)a+=2*Pi;return a;}
}
void PlayerMotion::reset(int yaw) {*this=PlayerMotion{};angle=yaw/5215.189175f;}
void PlayerMotion::settle(int yaw) {
    float oldEnergy=energy,oldMaximum=maximum,oldProgress=progress;
    reset(yaw);energy=oldEnergy;maximum=oldMaximum;progress=oldProgress;
}
void PlayerMotion::rest(unsigned ticks) {energy=std::min(maximum,energy+25.f*ticks/60);}
void PlayerMotion::pose(Animation::Clip a,float pa,Animation::Clip b,float pb,float mix) {
    primary=a;secondary=b;phaseA=std::max(0.f,std::min(1.f,pa));phaseB=std::max(0.f,std::min(1.f,pb));blendAmount=mix;
}
PlayerMotion::Step PlayerMotion::tick(const Input& input,bool grounded) {
    Step out;float magnitude=std::sqrt(input.x*input.x+input.z*input.z);bool moving=magnitude>.001f;
    const float ts=Dt*playerData::TimeScale;
    if(moving && state!=Stopping && state!=Flying && state!=Launching && state!=Landing) {
        float wanted=std::atan2(input.x,-input.z),turn=limitAngle(wanted-angle);
        const float maxTurn=playerData::HeadingDegrees*Pi/180*ts;
        angle=limitAngle(angle+std::max(-maxTurn,std::min(maxTurn,turn)));
    }
    // Digital full travel is 128 / PAD_MOVE_TO_GAME_WORLD_MOVE (60).
    if(moving)moveAmount=std::min(input.walk ? 1.f : 128.f/60,moveAmount+playerData::MoveAcceleration*ts);
    else moveAmount=0;
    if(exhausted && energy>0)exhausted=false;
    if(!input.sprint || !moving || input.walk)sprinting=false;
    if(input.sprint && moving && !input.walk && state==Moving && !exhausted && (energy>0 || sprinting))sprinting=true;
    if(sprinting) {
        energy=std::max(playerData::StaminaFloor,energy-ts);progress+=ts;
        if(progress>=500){progress=0;maximum=std::min(1000.f,maximum+10);}
        if(energy<=playerData::StaminaFloor){sprinting=false;exhausted=true;}
    } else energy=std::min(maximum,energy+(moving && moveAmount>1 ? .3f : 1.f)*ts*.5f);
    sprintBlend=std::max(0.f,std::min(1.f,sprintBlend+(sprinting ? 2.f : -1.f)*Dt));
    float distance=0;
    if(state==Standing && moving){state=Starting;time=0;cycle=0;}
    if((state==Standing || state==Starting || state==Moving) && input.jump && grounded) {state=Launching;time=0;}
    if(!grounded && state!=Flying && state!=Launching) {
        state=Flying;time=0;airX=std::sin(angle)*lastSpeed;airZ=-std::cos(angle)*lastSpeed;
    }
    if(state==Flying && grounded){state=Landing;time=0;airX=airZ=0;}
    switch(state) {
    case Standing:
        idleTime+=Dt;pose(Animation::Idle,std::fmod(idleTime/duration(Animation::Idle),1.f),Animation::Idle,0,0);break;
    case Starting:
        if(!moving){state=Standing;time=0;break;}
        distance=oneShot(Animation::StartWalk,time,time+Dt);time+=Dt;
        pose(Animation::StartWalk,time/duration(Animation::StartWalk),Animation::StartWalk,0,0);
        if(time>=duration(Animation::StartWalk)){state=Moving;time=0;cycle=0;}break;
    case Moving: {
        if(!moving) {
            if(sprintBlend>.8f){state=Stopping;time=0;stopClip=cycle<.5f ? Animation::RunStop : Animation::RunStopR;}
            else {state=Standing;sprintBlend=0;}
            sprinting=false;break;
        }
        float runWeight=std::max(0.f,std::min(1.f,moveAmount-1));
        // RpAnimBlendClumpUpdateAnimations synchronizes locomotion periods.
        float period=(duration(Animation::Walk)*(1-runWeight)+duration(Animation::Run)*runWeight)*(1-sprintBlend)+duration(Animation::Sprint)*sprintBlend;
        float next=cycle+Dt/period;
        distance=(travel(Animation::Walk,cycle,next)*(1-runWeight)+travel(Animation::Run,cycle,next)*runWeight)*(1-sprintBlend)+travel(Animation::Sprint,cycle,next)*sprintBlend;
        cycle=next-std::floor(next);
        if(sprintBlend>0)pose(Animation::Run,cycle,Animation::Sprint,cycle,sprintBlend);
        else pose(Animation::Walk,cycle,Animation::Run,cycle,runWeight);
        break;
    }
    case Stopping:
        distance=oneShot(stopClip,time,time+Dt);time+=Dt;pose(stopClip,time/duration(stopClip),stopClip,0,0);
        if(time>=duration(stopClip)){state=Standing;sprintBlend=0;time=0;}break;
    case Launching:
        // Launch/glide are partial animations without translation extraction.
        // The underlying gait continues until FinishLaunchCB applies velocity.
        if(moving) {
            float runWeight=std::max(0.f,std::min(1.f,moveAmount-1));
            float period=(duration(Animation::Walk)*(1-runWeight)+duration(Animation::Run)*runWeight)*(1-sprintBlend)+duration(Animation::Sprint)*sprintBlend;
            float next=cycle+Dt/period;
            distance=(travel(Animation::Walk,cycle,next)*(1-runWeight)+travel(Animation::Run,cycle,next)*runWeight)*(1-sprintBlend)+travel(Animation::Sprint,cycle,next)*sprintBlend;
            cycle=next-std::floor(next);
        }
        time+=Dt;
        pose(Animation::JumpLaunch,time/duration(Animation::JumpLaunch),Animation::JumpLaunch,0,0);
        if(time>=duration(Animation::JumpLaunch)) {
            out.impulse=playerData::JumpImpulse/playerData::Mass*playerData::TimeScale;
            float minAir=(sprintBlend>0 ? .17f+.05f*sprintBlend : .1f+.07f*std::max(0.f,std::min(1.f,moveAmount-1)))*playerData::TimeScale;
            float airSpeed=std::max(lastSpeed,minAir);airX=std::sin(angle)*airSpeed;airZ=-std::cos(angle)*airSpeed;
            state=Flying;time=0;
        }break;
    case Flying:
        time+=Dt;out.x=airX*Dt;out.z=airZ*Dt;
        pose(Animation::JumpGlide,std::min(1.f,time/duration(Animation::JumpGlide)),Animation::JumpGlide,0,0);break;
    case Landing:
        distance=oneShot(Animation::JumpLand,time,time+Dt);time+=Dt;
        pose(Animation::JumpLand,time/duration(Animation::JumpLand),Animation::JumpLand,0,0);
        if(time>=duration(Animation::JumpLand)){state=Standing;time=0;sprintBlend=0;}break;
    }
    if(state!=Flying || out.impulse) {out.x=std::sin(angle)*distance;out.z=-std::cos(angle)*distance;}
    lastSpeed=std::sqrt(out.x*out.x+out.z*out.z)/Dt;
    return out;
}
}
