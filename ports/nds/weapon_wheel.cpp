#include "weapon_wheel.h"
#include <nds.h>
#include <cstdio>
namespace slice {
namespace {
void pixel(int x,int y,int color) {
    if(x<0 || x>=256 || y<16 || y>=160)return;
    auto* p=reinterpret_cast<volatile uint16_t*>(bgGetGfxPtr(3+4));
    unsigned at=y*256+x;uint16_t v=p[at/2];p[at/2]=(at&1) ? ((v&255)|(color<<8)) : ((v&0xff00)|color);
}
void rect(int x,int y,int w,int h,int color){for(int j=y;j<y+h;++j)for(int i=x;i<x+w;++i)pixel(i,j,color);}
void line(int x,int y,int b,int c,int color){int dx=std::abs(b-x),sx=x<b?1:-1,dy=-std::abs(c-y),sy=y<c?1:-1,e=dx+dy;for(;;){pixel(x,y,color);if(x==b && y==c)break;int e2=e*2;if(e2>=dy){e+=dy;x+=sx;}if(e2<=dx){e+=dx;y+=sy;}}}
void palette(){BG_PALETTE_SUB[6]=RGB15(3,5,7);BG_PALETTE_SUB[7]=RGB15(7,9,11);BG_PALETTE_SUB[8]=RGB15(12,14,16);BG_PALETTE_SUB[9]=RGB15(5,24,28);BG_PALETTE_SUB[10]=RGB15(28,30,31);BG_PALETTE_SUB[11]=RGB15(2,3,4);}
void fist(int x,int y,int color){rect(x-8,y-3,17,12,color);for(int i=0;i<4;++i)rect(x-8+i*4,y-10+(i==3?2:0),3,12,color);rect(x+7,y+1,5,7,color);rect(x-5,y+9,12,5,color);}
void pistol(int x,int y,int color){rect(x-12,y-7,27,6,color);rect(x-11,y-1,7,13,color);rect(x-4,y-1,10,2,color);line(x+5,y,x+4,y+5,color);line(x-3,y+5,x+4,y+5,color);}
void draw(const Street& inventory){
    palette();rect(0,16,256,144,11);
    for(int y=16;y<160;++y)for(int x=48;x<208;++x){int dx=x-128,dy=y-88,r2=dx*dx+dy*dy;if(r2<78*78 && r2>=25*25){float angle=std::atan2(float(dx),float(-dy));if(angle<0)angle+=6.283185307f;int sector=int((angle+.392699082f)/.785398163f)&7;bool enabled=sector==0 || (sector==1 && inventory.hasPistol());int col=enabled?7:6;if((sector==0 && !inventory.armed)||(sector==1 && inventory.armed))col=9;if(r2>75*75 || r2<28*28)col=enabled?10:8;pixel(x,y,col);}}
    for(int i=0;i<8;++i){float a=(i-.5f)*.785398163f;line(128+int(std::sin(a)*27),88-int(std::cos(a)*27),128+int(std::sin(a)*77),88-int(std::cos(a)*77),11);}
    fist(128,36,10);if(inventory.hasPistol())pistol(169,48,10);
    line(121,81,135,95,10);line(121,95,135,81,10);
    std::printf("\x1b[0;0H          WEAPONS\x1b[1;0H     Tap a weapon to equip\x1b[12;10H%s",inventory.armed?"PISTOL":"UNARMED");
    if(inventory.hasPistol())std::printf("\x1b[14;9HAmmo %02d / %02d",inventory.ammo,inventory.reserve);
    std::printf("\x1b[20;0H%s\x1b[22;0H Tap centre / Start to return",inventory.hasPistol() ? " Fists: top | Pistol: top-right" : "        Fists: top segment");
}
}
void WeaponWheel::drawButton(){palette();rect(212,128,42,30,10);rect(214,130,38,26,6);pistol(233,140,10);rect(218,152,30,2,9);}
void WeaponWheel::show(Street& inventory){
    consoleClear();draw(inventory);
    // Wait for the opening touch to lift so it cannot select a wheel segment.
    do{swiWaitForVBlank();scanKeys();}while(keysHeld()&KEY_TOUCH);
    while(pmMainLoop()){
        swiWaitForVBlank();scanKeys();unsigned down=keysDown();if(down&KEY_START)break;
        if(down&KEY_TOUCH){touchPosition p{};touchRead(&p);int selected=choice(p.px,p.py,inventory);if(selected==-2)break;if(selected>=0 && inventory.selectWeapon(selected==1))break;}
    }
    // Selection must not become a city-map waypoint or a gameplay button press.
    do{swiWaitForVBlank();scanKeys();}while(keysHeld()&(KEY_TOUCH|KEY_START));
    consoleClear();
}
}
