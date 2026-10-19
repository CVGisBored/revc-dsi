#include "lighting.h"
#include <cstdio>
#include <cstring>
#include <algorithm>
namespace slice {
namespace {
uint16_t rgb(const unsigned char* color) {
    return (color[0]>>3)|((color[1]>>3)<<5)|((color[2]>>3)<<10);
}
}
bool Lighting::open(const char* path) {
    FILE* file=std::fopen(path,"rb");if(!file)return false;char magic[4];
    bool valid=std::fread(magic,1,4,file)==4 && !std::memcmp(magic,"VLI1",4) &&
        std::fread(colors,sizeof(colors),1,file)==1 && std::fgetc(file)==EOF;
    std::fclose(file);
    if(valid)for(int k=0;k<3;++k)for(int value=0;value<32;++value){
        // Ambient fills unlit faces without clipping midtones to white. Source
        // baked prelights retain their contrast, as in the cutscene converter.
        int ambient=colors[k]>>3;surfaces[k][value]=value+(ambient*(31-value)+15)/31;
    }
    return valid;
}
uint16_t Lighting::surface(uint16_t prelight) const {
    uint16_t result=0;
    for(int k=0;k<3;++k)result|=unsigned(surfaces[k][(prelight>>(k*5))&31])<<(k*5);
    return result;
}
uint16_t Lighting::sky() const {return rgb(colors+3);}
uint16_t Lighting::water() const {return rgb(colors+6);}
}
