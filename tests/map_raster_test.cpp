#include "map_raster.h"
#include <array>
#include <cassert>
#include <cmath>
#include <cstdio>
int main(){
    std::array<uint8_t,65536> pixels;for(unsigned i=0;i<pixels.size();++i)pixels[i]=16+(i*37+i/256)%240;
    std::array<uint16_t,32768> actual,expected;unsigned cases=0;
    for(float zoom:{400.f,4000.f})for(float cx:{-2600.f,-2230.f,-230.125f,1770.f,2200.f})for(float cz:{-3400.f,-1250.125f,740.f}){
        int columns[256],rows[144];const float scale=zoom/144,units=256.f/4000;
        for(int x=0;x<256;++x)columns[x]=int(std::floor((cx+(x-128)*scale+2230)*units));
        for(int y=16;y<160;++y)rows[y-16]=int(std::floor((cz+(y-88)*scale+3250)*units));
        for(bool partial:{false,true}){
            const int x0=partial ? 10 : 0,x1=partial ? 36 : 256,y0=partial ? 49 : 16,y1=partial ? 76 : 160;
            actual.fill(0xdead);expected=actual;
            for(int y=y0;y<y1;++y)for(int x=x0;x<x1;++x){
                const int tx=columns[x],ty=rows[y-16];const unsigned color=tx>=0 && tx<256 && ty>=0 && ty<256 ? pixels[ty*256+tx] : 1;
                auto& pair=expected[(y*256+x)/2];pair=x&1 ? (pair&255)|(color<<8) : (pair&0xff00)|color;
            }
            slice::restoreMapRows(pixels.data(),columns,rows,actual.data(),x0,y0,x1,y1);
            assert(actual==expected);++cases;
        }
    }
    std::printf("Map raster: %u full/partial, zoom and city-edge cases are pixel-identical; untouched VRAM retained\n",cases);
}
