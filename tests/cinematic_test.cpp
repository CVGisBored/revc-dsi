#include "cinematic.h"
#include <nds.h>
#include <cassert>
#include <cstdio>
#include <string>
#include <vector>
#include <fstream>
#include <unistd.h>
#include <dirent.h>
#include <algorithm>
uint16_t testFramebuffer[256*192]{};
static uint64_t ticks=0;static unsigned frames=0,stopAcks=0,starts=0,skipAt=0,skipKey=0,stallAt=0;
static const void* active=nullptr;static bool stopPending=false;
static bool initialized=false;
uint64_t tickGetCount(){return ticks;}
void DC_FlushRange(const void* p,size_t n){assert(p && n==16384);}
void soundInit(){initialized=true;}
void soundEnable(){assert(initialized);}
void soundStop(unsigned mask){assert(mask==(1u<<15));stopPending=true;}
void soundSynchronize(){if(stopPending){active=nullptr;stopPending=false;++stopAcks;}}
unsigned soundTimerFromHz(unsigned rate){assert(rate==16384);return rate;}
void soundPreparePcm(unsigned ch,unsigned vol,unsigned pan,unsigned timer,SoundMode,SoundFmt,const void* p,unsigned start,unsigned words) {
    assert(!active && !stopPending && ch==(15|SOUND_START) && vol==2032 && pan==64 && timer==16384);
    assert((uintptr_t(p)&31)==0 && start==0 && words==8192);active=p;++starts;
}
void vramSetBankD(unsigned){}void videoSetMode(unsigned){}
void swiWaitForVBlank(){ticks+=100;++frames;if(frames==stallAt)ticks+=2500;assert(frames<20000);}
bool pmMainLoop(){return true;}void scanKeys(){}
unsigned keysDown(){return skipAt && frames>=skipAt ? skipKey : 0;}
static void reset(){frames=skipAt=stallAt=0;assert(!active && !stopPending);}
int main(int argc,char** argv) {
    assert(argc==2);assert(std::freopen("/dev/null","w",stdout));
    std::string root=argv[1];
    std::vector<std::string> movies;DIR* directory=opendir((root+"/cutscenes").c_str());assert(directory);
    while(auto* entry=readdir(directory)){std::string name=entry->d_name;if(name.size()>4 && name.substr(name.size()-4)==".vcv")movies.push_back(name.substr(0,name.size()-4));}
    closedir(directory);std::sort(movies.begin(),movies.end());assert(movies.size()>=17);
    for(const auto& movie : movies) {const char* name=movie.c_str();
        reset();std::string file=root+"/cutscenes/"+name+".vcv";
        auto result=slice::Cinematic::play(file.c_str(),name);if(result!=slice::Cinematic::Completed)std::fprintf(stderr,"Movie %s failed: result %d error %d\n",name,int(result),slice::Cinematic::lastError);assert(result==slice::Cinematic::Completed);
        assert(!active && !stopPending && !slice::Cinematic::lastError);
    }
    std::string file=root+"/cutscenes/int_m.vcv";
    reset();skipAt=5;skipKey=KEY_START;assert(slice::Cinematic::play(file.c_str(),"skip")==slice::Cinematic::Skipped);assert(!active);
    reset();skipAt=5;skipKey=KEY_SELECT;assert(slice::Cinematic::play(file.c_str(),"skip all")==slice::Cinematic::SkipAll);assert(!active);
    reset();stallAt=15;unsigned before=starts;assert(slice::Cinematic::play(file.c_str(),"stall")==slice::Cinematic::Completed);assert(starts>before+1 && !active);
    // Damage the first compressed frame: playback must fail visibly and release
    // ARM7 ownership before its ring buffer goes out of scope.
    std::ifstream input(file,std::ios::binary);std::vector<char> data((std::istreambuf_iterator<char>(input)),{});
    auto u16=[&](int p){return unsigned(uint8_t(data[p]))|(unsigned(uint8_t(data[p+1]))<<8);};
    auto u32=[&](int p){unsigned n=0;for(int i=0;i<4;++i)n|=unsigned(uint8_t(data[p+i]))<<(8*i);return n;};
    size_t offset=40+u16(10)*88+u32(12)*4+u32(20);assert(offset<data.size());data[offset]=0;
    char path[]="/tmp/revc-cinematic-XXXXXX";int fd=mkstemp(path);assert(fd>=0);close(fd);
    {std::ofstream out(path,std::ios::binary);out.write(data.data(),data.size());}
    reset();assert(slice::Cinematic::play(path,"broken")==slice::Cinematic::Error);assert(slice::Cinematic::lastError==5 && !active && !stopPending);unlink(path);
    std::fprintf(stderr,"%zu complete movies, sequential ARM7 stop acknowledgements, scene/all skip, storage-stall recovery and corrupt-frame cleanup passed (%u starts, %u acknowledged stops)\n",movies.size(),starts,stopAcks);
}
