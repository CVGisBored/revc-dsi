#pragma once
#include "gameplay.h"
#include <algorithm>
#include <cstdint>
#include <cmath>
namespace slice {
// Matches glRotateXi(pitch) * glRotateYi(yaw) and the 65 degree, 4:3
// projection. Bounds are conservative across the cached camera bins.
struct ViewFrustum {
    int x,y,z,si,co,sp,cp;
    static int bin(int value){return value>=0 ? value/32 : -((-value+31)/32);}
    ViewFrustum(Point camera,float sine,float cosine,float pitchSine,float pitchCosine):
        x(int(camera.x*64)),y(int(camera.y*64)),z(int(camera.z*64)),si(int(sine*4096)),co(int(cosine*4096)),sp(int(pitchSine*4096)),cp(int(pitchCosine*4096)){}
    bool triangle(const RevcTriangle& t) const {
        int outside[6]={};
        for(const auto& v:t.vertex) {
            int dx=v[0]-x,dy=v[1]-y,dz=v[2]-z;
            int f=(dx*si-dz*co)>>12,s=(dx*co+dz*si)>>12,d=(f*cp-dy*sp)>>12,u=(dy*cp+f*sp)>>12;
            outside[0]+=s*4096 < -d*3480-96*4096;outside[1]+=s*4096>d*3480+96*4096;
            outside[2]+=u*4096 < -d*2610-96*4096;outside[3]+=u*4096>d*2610+96*4096;
            outside[4]+=d < -64;outside[5]+=d>80*64+64;
        }
        for(int n:outside)if(n==3)return false;
        return true;
    }
    bool bounds(const int16_t low[3],const int16_t high[3],int& nearest) const {
        int outside[6]={};nearest=32767;
        for(int corner=0;corner<8;++corner) {
            int dx=((corner&1)?high[0]:low[0])-x,dy=((corner&2)?high[1]:low[1])-y,dz=((corner&4)?high[2]:low[2])-z;
            int forward=(dx*si-dz*co)>>12,side=(dx*co+dz*si)>>12;
            int depth=(forward*cp-dy*sp)>>12,up=(dy*cp+forward*sp)>>12;
            nearest=std::min(nearest,depth);
            outside[0]+=side*4096 < -depth*3480-96*4096;outside[1]+=side*4096>depth*3480+96*4096;
            outside[2]+=up*4096 < -depth*2610-96*4096;outside[3]+=up*4096>depth*2610+96*4096;
            outside[4]+=depth < -64;outside[5]+=depth>80*64+64;
        }
        for(int n:outside)if(n==8)return false;
        return true;
    }
};
// Keep the gameplay camera outside dynamic vehicles, which are absent from
// the static COL grid. The view direction stays aimed at the same focus.
inline Point cameraOutsideVehicle(Point focus,Point camera,Point center,float sine,float cosine,float height=1.9f) {
    auto local=[&](Point p){float x=p.x-center.x,z=p.z-center.z;return Point{x*cosine+z*sine,p.y-center.y,x*sine-z*cosine};};
    Point a=local(focus),b=local(camera);
    const float from[3]={a.x,a.y,a.z},to[3]={b.x,b.y,b.z},low[3]={-1.4f,-.1f,-3.f},high[3]={1.4f,height,3.f};
    float enter=0,leave=1;
    for(int k=0;k<3;++k) {
        float delta=to[k]-from[k];
        if(std::fabs(delta)<1e-6f){if(from[k]<low[k] || from[k]>high[k])return camera;}
        else {
            float t=(low[k]-from[k])/delta,u=(high[k]-from[k])/delta;if(t>u)std::swap(t,u);
            enter=std::max(enter,t);leave=std::min(leave,u);if(enter>leave)return camera;
        }
    }
    if(enter>=1)return camera;
    float dx=camera.x-focus.x,dy=camera.y-focus.y,dz=camera.z-focus.z,length=std::sqrt(dx*dx+dy*dy+dz*dz);
    float t=std::max(0.f,enter-(length>.001f ? .15f/length : 0.f));
    return {focus.x+dx*t,focus.y+dy*t,focus.z+dz*t};
}
// Pitch is an orbit/downward-look angle: reducing it raises the view.
inline int cameraVerticalInput(bool x,bool b){return int(b)-int(x);}
}
