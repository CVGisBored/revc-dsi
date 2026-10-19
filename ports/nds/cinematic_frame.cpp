#include "cinematic_frame.h"
#include "packed.h"
namespace slice {
bool decodeCinematicFrame(unsigned codec,unsigned width,unsigned height,bool key,
    const unsigned char* packed,size_t size,const unsigned char* previous,
    unsigned char* output,unsigned char* scratch) {
    if(!output || !packed || codec<1 || codec>3 ||
       !((width==128 && height==72)||(width==256 && height==144)))return false;
    const unsigned bytes=512+width*height;
    if(codec==1 || key)return unpackLz10(packed,size,output,bytes)==bytes;
    if(!previous)return false;
    if(codec==2){if(unpackLz10(packed,size,output,bytes)!=bytes)return false;for(unsigned i=0;i<bytes;++i)output[i]^=previous[i];return true;}
    if(width!=128 || height!=72 || !scratch || unpackLz10(packed,size,scratch,bytes+144)!=bytes+144)return false;
    for(unsigned i=0;i<512;++i)output[i]=scratch[i]^previous[i];
    const auto* residual=scratch+656;
    for(unsigned block=0;block<144;++block){
        unsigned code=scratch[512+block];if(code>=81)return false;
        int x=(block%16)*8,y=(block/16)*8,dx=int(code%9)-4,dy=int(code/9)-4;
        if(x+dx<0 || y+dy<0 || x+dx+8>128 || y+dy+8>72)return false;
        for(int row=0;row<8;++row){unsigned at=(y+row)*128+x,old=(y+row+dy)*128+x+dx;
            for(unsigned column=0;column<8;++column)output[512+at+column]=previous[512+old+column]^residual[at+column];}
    }
    return true;
}
}
