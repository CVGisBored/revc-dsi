#include "citymap.h"
#include <nds.h>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <algorithm>
#include <new>
namespace slice {
namespace {constexpr int Top=16,Bottom=160;}
bool CityMap::open(const char* path) {
    backgroundValid=false;oldMarkerCount=0;
    FILE* f=std::fopen(path,"rb");if(!f)return false;
    char magic[4];pixels.reset(new(std::nothrow) uint8_t[65536]);
    bool ok=pixels && std::fread(magic,1,4,f)==4 && !std::memcmp(magic,"VRM1",4) &&
        std::fread(palette,2,240,f)==240 && std::fread(pixels.get(),1,65536,f)==65536 && std::fgetc(f)==EOF;
    std::fclose(f);if(!ok) {pixels.reset();return false;}
    for(unsigned i=0;i<65536;++i)if(pixels[i]<16) {pixels.reset();return false;}
    const int bg=bgInitSub(3,BgType_Bmp8,BgSize_B8_256x256,4,0);bgSetPriority(bg,3);
    bitmap=bgGetGfxPtr(bg);
    for(unsigned i=0;i<32768;++i)bitmap[i]=0;
    BG_PALETTE_SUB[0]=0;
    for(int i=0;i<240;++i)BG_PALETTE_SUB[i+16]=palette[i];
    BG_PALETTE_SUB[1]=RGB15(0,0,0);BG_PALETTE_SUB[2]=RGB15(31,7,24);
    BG_PALETTE_SUB[3]=RGB15(2,31,31);BG_PALETTE_SUB[4]=RGB15(31,29,0);
    BG_PALETTE_SUB[5]=RGB15(31,31,31);bgUpdate();return true;
}
int CityMap::screenX(float x) const {return 128+int(std::round((x-center.x)/scale));}
int CityMap::screenY(float z) const {return 88+int(std::round((z-center.z)/scale));}
void CityMap::input(int x,int y,Point player,bool touch,bool zoom,bool clear) {
    if(zoom)overview=!overview;
    if(clear)marked=false;
    // Use exactly the transform most recently rendered, including after pause/zoom.
    if(!touch || y<Top || y>=Bottom || !scale)return;
    Point p={center.x+(x-128)*scale,player.y,center.z+(y-88)*scale};
    if(p.x<-2230 || p.x>1770 || p.z<-3250 || p.z>750)return;
    if(marked && std::abs(screenX(marker.x)-x)<7 && std::abs(screenY(marker.z)-y)<7)marked=false;
    else {marker=p;marked=true;}
}
void CityMap::arrived(Point p,bool inCar) {
    if(!marked)return;
    float x=p.x-marker.x,z=p.z-marker.z;
    if(x*x+z*z<(inCar ? 225.f : 64.f))marked=false;
}
void CityMap::pixel(int x,int y,int color) {
    if(x<0 || x>255 || y<Top || y>=Bottom)return;
    auto* v=bitmap;auto& pair=v[(y*256+x)/2];
    const unsigned old=pair;pair=x&1 ? (old&255)|(color<<8) : (old&0xff00)|color;
}
void CityMap::line(int x,int y,int x2,int y2,int color) {
    // All marker endpoints are clipped before drawing, keeping this bounded.
    int dx=std::abs(x2-x),sx=x<x2 ? 1 : -1,dy=-std::abs(y2-y),sy=y<y2 ? 1 : -1,err=dx+dy;
    for(int i=0;i<520;++i) {pixel(x,y,color);if(x==x2 && y==y2)break;int e=2*err;if(e>=dy){err+=dy;x+=sx;}if(e<=dx){err+=dx;y+=sy;}}
}
void CityMap::dot(int x,int y,int color,int radius) {
    x=std::max(5,std::min(250,x));y=std::max(Top+5,std::min(Bottom-6,y));
    for(int j=-radius-1;j<=radius+1;++j)for(int i=-radius-1;i<=radius+1;++i)
        if(i*i+j*j<=(radius+1)*(radius+1))pixel(x+i,y+j,1);
    for(int j=-radius;j<=radius;++j)for(int i=-radius;i<=radius;++i)
        if(i*i+j*j<=radius*radius)pixel(x+i,y+j,color);
}
void CityMap::draw(Point player,int heading,const Point* objective,bool paused) {
    if(!pixels)return;
    (void)paused;
    center=overview ? Point{-230,0,-1250} : player;scale=(overview ? 4000.f : 400.f)/144;
    auto* v=bitmap;
    // 16-bit stores are required for DS VRAM; byte writes are ignored by hardware.
    const float units=256.f/4000;
    int columns[256];
    for(int x=0;x<256;++x)columns[x]=int(std::floor((center.x+(x-128)*scale+2230)*units));
    int rows[144];for(int y=Top;y<Bottom;++y)rows[y-Top]=int(std::floor((center.z+(y-88)*scale+3250)*units));
    const bool changed=!backgroundValid || std::memcmp(columns,lastColumns,sizeof(columns)) || std::memcmp(rows,lastRows,sizeof(rows));
    auto restore=[&](int x0,int y0,int x1,int y1) {
        x0=std::max(0,x0)&~1;x1=std::min(256,x1+1)&~1;y0=std::max(Top,y0);y1=std::min(Bottom,y1);
        for(int y=y0;y<y1;++y) {
            int ty=rows[y-Top];
            for(int x=x0;x<x1;x+=2) {
                int tx=columns[x],tx2=columns[x+1];
                unsigned a=ty>=0 && ty<256 && tx>=0 && tx<256 ? pixels[ty*256+tx] : 1;
                unsigned b=ty>=0 && ty<256 && tx2>=0 && tx2<256 ? pixels[ty*256+tx2] : 1;
                v[(y*256+x)/2]=a|(b<<8);
            }
        }
    };
    if(changed) {
        restore(0,Top,256,Bottom);
        std::memcpy(lastColumns,columns,sizeof(columns));std::memcpy(lastRows,rows,sizeof(rows));backgroundValid=true;
    } else for(unsigned i=0;i<oldMarkerCount;++i)restore(oldMarkers[i][0]-9,oldMarkers[i][1]-9,oldMarkers[i][0]+10,oldMarkers[i][1]+10);
    oldMarkerCount=0;
    auto remember=[&](int x,int y){oldMarkers[oldMarkerCount][0]=std::max(5,std::min(250,x));oldMarkers[oldMarkerCount++][1]=std::max(Top+5,std::min(Bottom-6,y));};
    if(objective)remember(screenX(objective->x),screenY(objective->z));
    if(marked)remember(screenX(marker.x),screenY(marker.z));
    if(objective)dot(screenX(objective->x),screenY(objective->z),2,3);
    if(marked)dot(screenX(marker.x),screenY(marker.z),4,3);
    int px=std::max(6,std::min(249,screenX(player.x))),py=std::max(Top+6,std::min(Bottom-7,screenY(player.z)));
    remember(px,py);
    const float si=sinLerp(heading)/4096.f,co=cosLerp(heading)/4096.f;
    dot(px,py,3,2);
    int tipx=px+int(si*7),tipy=py-int(co*7);
    line(tipx,tipy,px+int(-co*4-si*3),py+int(-si*4+co*3),5);
    line(tipx,tipy,px+int(co*4-si*3),py+int(si*4+co*3),5);
    line(px+int(-co*4-si*3),py+int(-si*4+co*3),px+int(co*4-si*3),py+int(si*4+co*3),3);
    // North arrow stays fixed in a north-up map.
    line(246,22,246,33,5);line(246,22,242,27,5);line(246,22,250,27,5);
}
}
