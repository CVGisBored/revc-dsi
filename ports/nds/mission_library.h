#pragma once
#include "gameplay.h"
namespace slice {
struct LibraryMovie {const char* name;const char* title;unsigned mission;bool available;};
struct LibraryMission {
    const char* title;unsigned kind;int adapted;unsigned reward;Point start;
    unsigned routeCount;Point route[8];unsigned movieCount;unsigned movies[8];unsigned id,action,targets;
};
struct FlightCourse {const Point* points;unsigned count;};
class MissionLibrary {
public:
    enum Kind {Catalog,Adapted,Route,Activity};
    enum ActivityType {Purchase=3,Fire,Patrol,Sales,Flight};
    static const FlightCourse& course(unsigned id);
    static unsigned count();static unsigned movieCount();
    static const LibraryMission& mission(unsigned index);
    static const LibraryMovie& movie(unsigned index);
    static int nextStory(int current=-1);
};
}
