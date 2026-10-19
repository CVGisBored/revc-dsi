#include "vehicle_animation.h"
#include <cstdio>
#include <cstring>
#include <cmath>
namespace slice {
bool VehicleAnimation::open(const char* path,int expected) {
    return loadRig(path,expected,own);
}
bool VehicleAnimation::loadRig(const char* path,int expected,Rig& rig) {
    rig.count=0;FILE* file=std::fopen(path,"rb");if(!file)return false;
    char magic[4];uint16_t n=0,parts=0;
    bool ok=std::fread(magic,4,1,file)==1 && !std::memcmp(magic,"VPA1",4) &&
        std::fread(&n,2,1,file)==1 && std::fread(&parts,2,1,file)==1 && n==expected && n>0 && n<=640 && parts==7 &&
        std::fread(rig.pivot,sizeof(rig.pivot),1,file)==1 && std::fread(rig.part,1,n,file)==n && std::fgetc(file)==EOF;
    std::fclose(file);for(unsigned i=0;ok && i<n;++i)ok=rig.part[i]<7;
    if(ok)rig.count=n;return ok;
}
const RevcTriangle* VehicleAnimation::pose(const RevcTriangle* source,int n,float roll,float steering,float door,int side,Point bodyTilt) {
    return pose(own,source,n,roll,steering,door,side,bodyTilt);
}
const RevcTriangle* VehicleAnimation::pose(const Rig& rig,const RevcTriangle* source,int n,float roll,float steering,float door,int side,Point bodyTilt) {
    const auto& pivot=rig.pivot;const auto& part=rig.part;
    int count=rig.count;
    if(n!=count || !source || !std::isfinite(roll+steering+door+bodyTilt.x+bodyTilt.z))return source;
    // Fixed-point matrices avoid software floating point per submitted vertex.
    int m[7][3][3]{};
    for(int p=0;p<7;++p) {
        float a=p>=1 && p<=4 ? roll : 0;
        float b=p==3 || p==4 ? -steering : p==5 && side<0 ? -door : p==6 && side>0 ? door : 0;
        float ca=std::cos(a),sa=std::sin(a),cb=std::cos(b),sb=std::sin(b);
        const float f[3][3]={{cb,sb*sa,sb*ca},{0,ca,-sa},{-sb,cb*sa,cb*ca}};
        for(int y=0;y<3;++y)for(int x=0;x<3;++x)m[p][y][x]=int(std::lround(f[y][x]*4096));
    }
    int body[3][3];
    float cp=std::cos(bodyTilt.x),sp=std::sin(bodyTilt.x),cr=std::cos(bodyTilt.z),sr=std::sin(bodyTilt.z);
    const float f[3][3]={{cr,-sr*cp,sr*sp},{sr,cr*cp,-cr*sp},{0,sp,cp}};
    for(int y=0;y<3;++y)for(int x=0;x<3;++x)body[y][x]=int(std::lround(f[y][x]*4096));
    const bool transfer=bodyTilt.x!=0 || bodyTilt.z!=0;
    for(int i=0;i<n;++i) {
        result[i]=source[i];int p=part[i];
        if(p)for(int j=0;j<3;++j)for(int axis=0;axis<3;++axis) {
            int sum=0;for(int k=0;k<3;++k)sum+=(source[i].vertex[j][k]-pivot[p][k])*m[p][axis][k];
            result[i].vertex[j][axis]=pivot[p][axis]+((sum+2048)>>12);
        }
        // Body weight transfer must not lift the wheel contact patches.
        if(transfer && (p==0 || p>=5))for(int j=0;j<3;++j) {
            int v[3]={result[i].vertex[j][0],result[i].vertex[j][1]-32,result[i].vertex[j][2]};
            for(int axis=0;axis<3;++axis) {
                int sum=0;for(int k=0;k<3;++k)sum+=body[axis][k]*v[k];
                result[i].vertex[j][axis]=((sum+2048)>>12)+(axis==1 ? 32 : 0);
            }
        }
    }
    return result;
}
}
