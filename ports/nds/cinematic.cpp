#include "cinematic.h"
#include "packed.h"
#include "cinematic_frame.h"
#include <nds.h>
#include <calico/system/tick.h>
#include <cstdio>
#include <cstring>
#include <memory>
#include <algorithm>
#include <new>
namespace slice {
bool Cinematic::subtitlesEnabled=true;
Cinematic::PlaybackStats Cinematic::stats{};
int Cinematic::lastError=0;
const char* Cinematic::lastPath="";
namespace {
struct Header {
    char magic[4];uint16_t width,height,fps,subs;
    uint32_t frames,rate,audioBytes,duration,maxPacked,rawBytes,subtitleStride;
};
static_assert(sizeof(Header)==40,"Cinematic header layout");
struct Subtitle {uint32_t start,end;char text[80];};
static_assert(sizeof(Subtitle)==88,"Subtitle layout");
constexpr unsigned Half=16384,Raw=512+256*144;
struct File {FILE* p=nullptr;~File(){if(p)std::fclose(p);}};
void clearLine(int row) {std::printf("\x1b[%d;0H%*s",row,row==23 ? 31 : 32,"");}
void subtitle(const char* text) {
    clearLine(20);clearLine(21);
    const char* newline=std::strchr(text,'\n');
    unsigned length=std::strlen(text);
    std::printf("\x1b[20;0H%.*s",int(std::min(32u,newline ? unsigned(newline-text) : length)),text);
    if(newline)std::printf("\x1b[21;0H%.32s",newline+1);
}
}
Cinematic::Result Cinematic::play(const char* path,const char* title) {
    lastPath=path;lastError=1;stats=PlaybackStats{};
    clearLine(0);clearLine(1);std::printf("\x1b[0;0HLoading scene...");
    File movie,audio;movie.p=std::fopen(path,"rb");audio.p=std::fopen(path,"rb");
    if(!movie.p || !audio.p)return Error;
    // Read-ahead amortizes NitroFS/SD transactions across several small deltas.
    std::setvbuf(movie.p,nullptr,_IOFBF,32768);
    Header h{};
    if(std::fread(&h,1,sizeof(h),movie.p)!=sizeof(h) || (std::memcmp(h.magic,"VCV1",4) && std::memcmp(h.magic,"VCV2",4) && std::memcmp(h.magic,"VCV3",4)) ||
       (!((h.width==256 && h.height==144) || (h.width==128 && h.height==72))) || h.fps<1 || h.fps>30 || !h.frames || h.frames>36000 ||
       h.subs>512 || h.rate!=Half || !h.audioBytes || h.duration<100 || h.duration>1200000 ||
       h.audioBytes>uint64_t(h.rate)*(h.duration+1000)/1000 ||
       h.frames!=(h.duration*h.fps+999)/1000 || h.maxPacked<4 || h.maxPacked>65536 || h.rawBytes!=512u+h.width*h.height || h.subtitleStride!=sizeof(Subtitle))return Error;
    const unsigned codec=h.magic[3]-'0';
    if(codec==3 && h.width!=128)return Error;
    stats.sourceFps=h.fps;
    lastError=2;
    std::unique_ptr<Subtitle[]> subs(new(std::nothrow) Subtitle[h.subs]);
    std::unique_ptr<uint32_t[]> offsets(new(std::nothrow) uint32_t[h.frames+1]);
    std::unique_ptr<unsigned char[]> packed(new(std::nothrow) unsigned char[h.maxPacked]);
    std::unique_ptr<unsigned char[]> raw(new(std::nothrow) unsigned char[h.rawBytes]);
    std::unique_ptr<unsigned char[]> previous(codec>1 ? new(std::nothrow) unsigned char[h.rawBytes] : nullptr);
    std::unique_ptr<unsigned char[]> residual(codec==3 ? new(std::nothrow) unsigned char[h.rawBytes+144] : nullptr);
    std::unique_ptr<unsigned char[]> keys(codec>1 ? new(std::nothrow) unsigned char[h.frames] : nullptr);
    // Align for the ARM7 and cache flushes. One second per half leaves I/O margin.
    std::unique_ptr<unsigned char[]> allocation(new(std::nothrow) unsigned char[Half*2+31]);
    if((h.subs && !subs) || !offsets || !packed || !raw || !allocation || (codec>1 && (!previous || !keys)) || (codec==3 && !residual))return Error;
    auto* ring=reinterpret_cast<unsigned char*>((reinterpret_cast<uintptr_t>(allocation.get())+31)&~uintptr_t(31));
    lastError=3;
    if(std::fread(subs.get(),sizeof(Subtitle),h.subs,movie.p)!=h.subs)return Error;
    for(unsigned i=0;i<h.subs;++i) {
        if(subs[i].start>=subs[i].end || subs[i].end>h.duration ||
           !std::memchr(subs[i].text,0,sizeof(subs[i].text)) || (i && subs[i].start<subs[i-1].start))return Error;
    }
    uint32_t payload=sizeof(Header)+sizeof(Subtitle)*h.subs+4*h.frames+h.audioBytes;
    offsets[0]=payload;
    unsigned chain=0;
    for(unsigned i=0;i<h.frames;++i) {
        uint32_t size=0;if(std::fread(&size,4,1,movie.p)!=1)return Error;
        if(codec>1){keys[i]=(size>>31)!=0;size&=0x7fffffffu;
            if(keys[i])chain=0;else if(!i || ++chain>=30)return Error;}
        if(size<4 || size>h.maxPacked || offsets[i]>0x7fffffffu-size)return Error;
        offsets[i+1]=offsets[i]+size;
    }
    if(std::fseek(movie.p,0,SEEK_END) || std::ftell(movie.p)!=long(offsets[h.frames]))return Error;
    const uint32_t audioStart=payload-h.audioBytes;
    auto fill=[&](unsigned slot,unsigned segment)->bool {
        unsigned char* target=ring+slot*Half;
        const uint32_t start=segment*Half;
        unsigned bytes=start<h.audioBytes ? std::min<uint32_t>(Half,h.audioBytes-start) : 0;
        if(bytes && (std::fseek(audio.p,audioStart+start,SEEK_SET) || std::fread(target,1,bytes,audio.p)!=bytes))return false;
        if(bytes<Half)std::memset(target+bytes,0,Half-bytes);
        DC_FlushRange(target,Half);return true;
    };
    lastError=4;
    if(!fill(0,0) || !fill(1,1))return Error;
    soundInit();soundEnable();
    // Own one channel for the entire scene. Confirm ARM7 has processed a stop
    // before replacing/releasing its sample RAM; VBlank is not an IPC fence.
    constexpr unsigned Channel=15,Mask=1u<<Channel;
    soundStop(Mask);soundSynchronize();
    auto startAudio=[&]() {
        soundPreparePcm(Channel|SOUND_START,127<<4,64,soundTimerFromHz(Half),SoundMode_Repeat,SoundFmt_Pcm8,ring,0,Half*2/4);
        soundSynchronize();
    };
    startAudio();
    vramSetBankD(VRAM_D_LCD);
    for(unsigned i=0;i<256*192;++i)VRAM_D[i]=RGB15(0,0,0)|0x8000;
    videoSetMode(MODE_FB3);
    clearLine(0);clearLine(1);clearLine(22);clearLine(23);
    std::printf("\x1b[22;0H%s",*title ? "Start: scene  Select: skip all" : "Start/Select: skip intro");
    // Print separately: title is user-facing and never interpreted as a format string.
    std::printf("\x1b[0;0H%.31s",title);
    uint64_t start=tickGetCount(),fpsTick=start;unsigned presentedAtTick=0,actualFps=0;
    unsigned generation=0,audioBase=0,lastFrame=~0u,subtitleIndex=0,lastSubtitle=~0u,lastSecond=~0u;
    Result result=Completed;
    while(pmMainLoop()) {
        scanKeys();const unsigned down=keysDown();
        if(down&KEY_SELECT){result=SkipAll;break;}
        if(down&KEY_START) {result=Skipped;break;}
        const uint64_t now=tickGetCount();
        const uint32_t milliseconds=uint32_t((now-start)*1000/TICK_FREQ);
        if(milliseconds>=h.duration)break;
        const unsigned half=milliseconds/1000;
        // Recover coherently after a long storage stall instead of replaying stale speech.
        if(half>generation+1) {
            soundStop(Mask);soundSynchronize();
            lastError=4;
            if(!fill(0,half) || !fill(1,half+1)) {result=Error;break;}
            startAudio();
            audioBase=generation=half;
            start=tickGetCount()-uint64_t(half)*TICK_FREQ;
            continue;
        }
        if(half>generation) {
            lastError=4;
            if(!fill((generation-audioBase)&1,generation+2)) {result=Error;break;}
            ++generation;
        }
        const unsigned frame=std::min(h.frames-1,milliseconds*h.fps/1000);
        if(frame!=lastFrame) {
            const uint64_t decodeStart=tickGetCount();
            unsigned first=frame;
            if(codec>1){while(first && !keys[first])--first;
                if(lastFrame!=~0u && lastFrame>=first && lastFrame<frame)first=lastFrame+1;}
            lastError=5;
            for(unsigned decode=first;decode<=frame;++decode){
                const unsigned size=offsets[decode+1]-offsets[decode];
                if((std::ftell(movie.p)!=long(offsets[decode]) && std::fseek(movie.p,offsets[decode],SEEK_SET)) ||
                   std::fread(packed.get(),1,size,movie.p)!=size ||
                   !decodeCinematicFrame(codec,h.width,h.height,codec==1 || keys[decode],packed.get(),size,
                       previous.get(),raw.get(),residual.get())){result=Error;break;}
                ++stats.decoded;if(codec>1)raw.swap(previous);
            }
            if(result==Error)break;
            const auto* display=codec>1 ? previous.get() : raw.get();
            const auto* palette=reinterpret_cast<const uint16_t*>(display);
            const unsigned char* pixels=display+512;
            // Each source pixel becomes a pair of pixels on two rows. Packed
            // 32-bit stores halve framebuffer writes versus per-pixel scaling.
            if(h.width==256)for(unsigned i=0;i<256*144;++i)VRAM_D[256*24+i]=palette[pixels[i]];
            else for(unsigned y=0;y<72;++y){
                auto* row=reinterpret_cast<volatile uint32_t*>(VRAM_D+256*(24+y*2));
                for(unsigned x=0;x<128;++x){uint32_t c=palette[pixels[y*128+x]];c|=c<<16;row[x]=c;row[x+128]=c;}}
            ++stats.presented;
            if(lastFrame!=~0u && frame>lastFrame+1)stats.dropped+=frame-lastFrame-1;
            stats.peakDecodeMicroseconds=std::max(stats.peakDecodeMicroseconds,unsigned((tickGetCount()-decodeStart)*1000000/TICK_FREQ));
            lastFrame=frame;
        }
        while(subtitleIndex<h.subs && milliseconds>=subs[subtitleIndex].end)++subtitleIndex;
        unsigned active=subtitleIndex<h.subs && milliseconds>=subs[subtitleIndex].start ? subtitleIndex : ~0u;
        if(active!=lastSubtitle) {subtitle(active==~0u || !subtitlesEnabled ? "" : subs[active].text);lastSubtitle=active;}
        if(now-fpsTick>=TICK_FREQ){actualFps=unsigned((stats.presented-presentedAtTick)*TICK_FREQ/(now-fpsTick));presentedAtTick=stats.presented;fpsTick=now;}
        stats.milliseconds=milliseconds;
        if(milliseconds/1000!=lastSecond) {
            lastSecond=milliseconds/1000;const unsigned total=(h.duration+999)/1000;
            std::printf("\x1b[23;0H%02u:%02u/%02u:%02u %2u/%ufps",lastSecond/60,lastSecond%60,total/60,total%60,actualFps,h.fps);
        }
        swiWaitForVBlank();
    }
    soundStop(Mask);soundSynchronize();
    videoSetMode(MODE_0_3D);clearLine(0);clearLine(1);clearLine(20);clearLine(21);clearLine(22);clearLine(23);
    if(result!=Error)lastError=0;
    return result;
}
}
