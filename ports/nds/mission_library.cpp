#include "mission_library.h"
#include "mission_library_data.h"
namespace slice {
const FlightCourse& MissionLibrary::course(unsigned id){
    static const FlightCourse empty={nullptr,0};
    return id>=84 && id<=87 ? libraryData::Courses[id-84] : empty;
}
unsigned MissionLibrary::count(){return sizeof(libraryData::Missions)/sizeof(libraryData::Missions[0]);}
unsigned MissionLibrary::movieCount(){return sizeof(libraryData::Movies)/sizeof(libraryData::Movies[0]);}
const LibraryMission& MissionLibrary::mission(unsigned i){return libraryData::Missions[i<count() ? i : 0];}
const LibraryMovie& MissionLibrary::movie(unsigned i){return libraryData::Movies[i<movieCount() ? i : 0];}
int MissionLibrary::nextStory(int current){
    if(current<0)return libraryData::Order[0];
    for(unsigned i=0;i<sizeof(libraryData::Order)/sizeof(libraryData::Order[0]);++i)
        if(int(libraryData::Order[i])==current)return i+1<sizeof(libraryData::Order)/sizeof(libraryData::Order[0]) ? int(libraryData::Order[i+1]) : -1;
    return -1;
}
}
