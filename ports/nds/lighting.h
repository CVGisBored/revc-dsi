#pragma once
#include <cstdint>
namespace slice {
class Lighting {
public:
    bool open(const char* path);
    uint16_t surface(uint16_t prelight) const;
    uint16_t sky() const;
    uint16_t water() const;
    unsigned waterAlpha() const {return colors[9]>>3;}
private:
    unsigned char colors[10]{};
    unsigned char surfaces[3][32]{};
};
}
