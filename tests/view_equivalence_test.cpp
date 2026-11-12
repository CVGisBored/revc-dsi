#include "view.h"
#include <cassert>
#include <cstdio>
static unsigned state=1234;
static int random(int range){state=state*1664525+1013904223;return int((state>>1)%range);}
static bool reference(const slice::ViewFrustum& view,const int16_t (*vertices)[3],int count,int& nearest){
    int rejected[6]{};nearest=32767;
    for(int i=0;i<count;++i){
        const int dx=vertices[i][0]-view.x,dy=vertices[i][1]-view.y,dz=vertices[i][2]-view.z;
        const int f=(dx*view.si-dz*view.co)>>12,s=(dx*view.co+dz*view.si)>>12;
        const int d=(f*view.cp-dy*view.sp)>>12,u=(dy*view.cp+f*view.sp)>>12;
        nearest=std::min(nearest,d);
        rejected[0]+=s*4096 < -d*3480-96*4096;rejected[1]+=s*4096>d*3480+96*4096;
        rejected[2]+=u*4096 < -d*2610-96*4096;rejected[3]+=u*4096>d*2610+96*4096;
        rejected[4]+=d < -64;rejected[5]+=d>80*64+64;
    }
    for(int n:rejected)if(n==count)return false;return true;
}
int main(){
    for(unsigned i=0;i<100000;++i){
        float yaw=random(32768)/5215.189175f,pitch=(random(16000)-8000)/5215.189175f;
        slice::Point eye{(random(16384)-8192)/64.f,(random(8192)-4096)/64.f,(random(16384)-8192)/64.f};
        slice::ViewFrustum view(eye,std::sin(yaw),std::cos(yaw),std::sin(pitch),std::cos(pitch));
        RevcTriangle t{};int16_t low[3],high[3],corners[8][3];int oldDepth,newDepth;
        for(auto& v:t.vertex)for(auto& c:v)c=random(32768)-16384;
        assert(view.triangle(t)==reference(view,t.vertex,3,oldDepth));
        for(int k=0;k<3;++k){low[k]=std::min({t.vertex[0][k],t.vertex[1][k],t.vertex[2][k]});high[k]=std::max({t.vertex[0][k],t.vertex[1][k],t.vertex[2][k]});}
        for(int corner=0;corner<8;++corner)for(int k=0;k<3;++k)corners[corner][k]=(corner&(1<<k)) ? high[k] : low[k];
        assert(view.bounds(low,high,newDepth)==reference(view,corners,8,oldDepth) && oldDepth==newDepth);
    }
    std::puts("Visibility: 100000 triangle/box cases match all six original clip planes and exact sorting depth");
}
