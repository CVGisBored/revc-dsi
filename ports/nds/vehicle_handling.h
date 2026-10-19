#pragma once
#include <cmath>
#include <algorithm>
namespace slice {
// SENTINEL handling.cfg, cHandlingDataMgr::ConvertDataToGameUnits and
// cTransmission::CalculateDriveAcceleration. Source velocities use 50 Hz.
struct SentinelHandling {
    float maximum=0,cruise=0;
    SentinelHandling() {
        float velocity=165.f/3.6f/50.f;
        for(float drag=100;24.f*.4f/(2500*6)<drag && velocity>0;) {
            velocity-=.01f;float resistance=.5f*velocity*velocity*2.f*1.6f/1400.f;
            drag=velocity*(1-1/(1+resistance));
        }
        cruise=velocity;maximum=velocity*1.2f;
    }
    float acceleration(float speed,int throttle,int& gear) const {
        const float v=speed*60.f/50.f;
        if(v<-.2f || v>maximum)return 0;
        gear=std::max(0,std::min(5,gear));
        const float step=maximum/5;
        for(int tries=0;tries<6;++tries) {
            float up=gear==0 ? -.01f : gear==5 ? maximum : step*(gear-1+.6667f);
            float down=gear<=1 ? -.01f : step*(gear-2+.42f);
            if(v>up && gear<5 && (gear!=0 || throttle>0)){++gear;continue;}
            if(v<down && gear>0 && (gear!=1 || throttle<0)){--gear;continue;}
            break;
        }
        float limit=gear ? step*gear : -.2f;
        // Preserve the original integer-division speed multiplier and 0x2 boost.
        float speedMul=gear==0 ? -1.f : gear==5 ? 1.f : 4.f;
        float boost=gear==0 ? 2.f : gear==1 ? 1.6f : gear==2 ? 1.3f : 1.f;
        float target=limit*speedMul;
        if(std::fabs(v)>=std::fabs(limit))return 0;
        // RWD split among two driven wheels, integrated as a bounded planar car.
        return throttle*(target-v)*(24.f*.4f*.5f*boost)/std::fabs(target)/3600;
    }
    static float drag(float speed) {
        const float v=speed*60.f/50.f;
        return speed*std::pow(1.f/(1.f+(2.f*1.6f/1400.f)*.5f*v*v),50.f/60.f);
    }
};
}
