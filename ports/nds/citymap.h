#pragma once
#include "gameplay.h"
#include <memory>
#include <cstdint>
namespace slice {
class CityMap {
public:
    void invalidate() {backgroundValid=false;}
    void pause(bool paused) {overview=paused;}
    bool open(const char* path);
    void input(int x,int y,Point player,bool touch,bool zoom,bool clear);
    void draw(Point player,int heading,const Point* objective,bool paused);
    bool waypoint() const {return marked;}
    Point destination() const {return marker;}
    void arrived(Point player,bool inCar);
private:
    std::unique_ptr<uint8_t[]> pixels;
    uint16_t palette[240]{};
    volatile uint16_t* bitmap=nullptr;
    bool overview=false,marked=false;
    Point center{},marker{};
    float scale=0;
    int lastColumns[256]{},lastRows[144]{};
    int oldMarkers[3][2]{};
    bool backgroundValid=false;
    unsigned oldMarkerCount=0;
    int screenX(float x) const;
    int screenY(float z) const;
    void dot(int x,int y,int color,int radius);
    void line(int x,int y,int x2,int y2,int color);
    void pixel(int x,int y,int color);
};
}
