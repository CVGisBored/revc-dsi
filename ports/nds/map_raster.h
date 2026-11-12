#pragma once
#include <cstdint>
namespace slice {
// Nearest-neighbour map sampling, with the same source coordinates and VRAM
// halfword writes. At street zoom several output rows share one source row.
inline void restoreMapRows(const uint8_t* pixels,const int* columns,const int* rows,
                           volatile uint16_t* bitmap,int x0,int y0,int x1,int y1) {
    uint16_t line[128];int previous=-1;
    for(int y=y0;y<y1;++y){
        const int source=rows[y-16];
        // Out-of-map rows all have the same black pixels.
        const int row=source>=0 && source<256 ? source : -2;
        if(row!=previous){
            if(row<0)for(int x=x0;x<x1;x+=2)line[x/2]=0x0101;
            else {const auto* input=pixels+row*256;
                for(int x=x0;x<x1;x+=2){
                    const int a=columns[x],b=columns[x+1];
                    line[x/2]=(a>=0 && a<256 ? input[a] : 1) |
                              ((b>=0 && b<256 ? input[b] : 1)<<8);
                }
            }
            previous=row;
        }
        auto* output=bitmap+y*128;
        for(int x=x0/2;x<x1/2;++x)output[x]=line[x];
    }
}
}
