#pragma once
#include "animation.h"
namespace slice {
// A bounded controller using the original player's root-motion and timing rules.
class PlayerMotion {
public:
    struct Input {float x=0,z=0;bool sprint=false,walk=false,jump=false;};
    struct Step {float x=0,z=0,impulse=0;};
    Step tick(const Input& input,bool grounded);
    void reset(int yaw=0);
    void settle(int yaw);
    void rest(unsigned ticks);
    int heading() const {return int(angle*5215.189175f)&32767;}
    Animation::Clip first() const {return primary;}
    Animation::Clip second() const {return secondary;}
    float firstPhase() const {return phaseA;}
    float secondPhase() const {return phaseB;}
    float blend() const {return blendAmount;}
    float stamina() const {return energy;}
    float speed() const {return lastSpeed;}
private:
    enum State {Standing,Starting,Moving,Stopping,Launching,Flying,Landing};
    State state=Standing;
    Animation::Clip primary=Animation::Idle,secondary=Animation::Idle,stopClip=Animation::RunStop;
    float angle=0,phaseA=0,phaseB=0,blendAmount=0,moveAmount=0,sprintBlend=0;
    float time=0,cycle=0,idleTime=0,energy=150,maximum=150,progress=0;
    float airX=0,airZ=0,lastSpeed=0;
    bool sprinting=false,exhausted=false;
    void pose(Animation::Clip a,float pa,Animation::Clip b,float pb,float blend);
};
}
