#pragma once
#include <cstdint>
namespace slice {
class Water {
public:
    bool open(const char* path);
    bool cell(int x,int y,float& level) const;
    unsigned texture() const {return textureId;}
private:
    uint32_t levelCount=0,textureId=0;
    float levels[48]{};
    int8_t mask[128*128]{};
};
}
