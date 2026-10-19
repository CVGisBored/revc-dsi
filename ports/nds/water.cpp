#include "water.h"
#include <cstdio>
#include <cstring>
#include <cmath>
namespace slice {
bool Water::open(const char* path) {
    levelCount=textureId=0;FILE* file=std::fopen(path,"rb");if(!file)return false;
    char magic[4];uint32_t count=0,texture=0;
    bool valid=std::fread(magic,1,4,file)==4 && !std::memcmp(magic,"VWA1",4) &&
        std::fread(&count,4,1,file)==1 && count>0 && count<=48 &&
        std::fread(&texture,4,1,file)==1 && texture>0 && texture<32768 &&
        std::fread(levels,sizeof(levels),1,file)==1 &&
        std::fread(mask,sizeof(mask),1,file)==1 && std::fgetc(file)==EOF;
    std::fclose(file);
    if(valid) {
        for(unsigned i=0;i<count;++i)if(!std::isfinite(levels[i]))valid=false;
        for(int8_t index:mask)if(index!=-128 && (index<0 || unsigned(index)>=count))valid=false;
    }
    if(valid) {levelCount=count;textureId=texture;}
    return valid;
}
bool Water::cell(int x,int y,float& level) const {
    if(x<0 || y<0 || x>=128 || y>=128 || !levelCount)return false;
    int index=mask[x*128+y];if(index<0)return false;
    level=levels[index];return true;
}
}
