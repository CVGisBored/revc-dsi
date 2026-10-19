#pragma once
#include "street.h"
#include <cmath>
namespace slice {
// Touch geometry is shared by the real screen and native input tests.
class WeaponWheel {
public:
    static bool button(int x,int y) {return x>=212 && x<254 && y>=128 && y<158;}
    static int choice(int x,int y,const Street& inventory) {
        const int dx=x-128,dy=y-88,r2=dx*dx+dy*dy;
        if(r2<22*22)return -2; // Cancel without changing equipment.
        if(r2>76*76 || r2<28*28)return -1;
        float angle=std::atan2(float(dx),float(-dy));if(angle<0)angle+=6.283185307f;
        int sector=int((angle+.392699082f)/.785398163f)&7;
        if(sector==0)return 0;
        if(sector==1 && inventory.hasPistol())return 1;
        return -1;
    }
    static void drawButton();
    static void show(Street& inventory);
};
}
