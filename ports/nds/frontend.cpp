#include "frontend.h"
#include "cinematic.h"
#include "mission_library.h"
#include <algorithm>
#include <nds.h>
#include <cstdio>
#include <memory>
#include <new>
#include <initializer_list>
namespace slice {
unsigned Frontend::selectedMission=0;
bool Frontend::movies() {
    consoleClear();
    for(const char* path : {"nitro:/cutscenes/logo.vcv","nitro:/cutscenes/gtatitles.vcv"}) {
        auto result=Cinematic::play(path,"");
        if(result==Cinematic::Error)return false;
        if(result==Cinematic::Skipped || result==Cinematic::SkipAll)break;
    }
    return true;
}
bool Frontend::story() {
    for(const char* path : {"nitro:/cutscenes/int_a.vcv","nitro:/cutscenes/int_m.vcv","nitro:/cutscenes/int_d.vcv","nitro:/cutscenes/departure.vcv"}) {
        auto result=Cinematic::play(path,"In the Beginning...");
        if(result==Cinematic::Error)return false;
        if(result==Cinematic::SkipAll)break;
    }
    return true;
}
Frontend::Result Frontend::run(bool paused) {
    FILE* f=std::fopen("nitro:/cutscenes/frontend.bin","rb");if(!f)return Error;
    std::unique_ptr<uint16_t[]> art(new(std::nothrow) uint16_t[256*192]);
    bool ok=art && std::fread(art.get(),2,256*192,f)==256*192 && std::fgetc(f)==EOF;
    std::fclose(f);if(!ok)return Error;
    vramSetBankD(VRAM_D_LCD);dmaCopy(art.get(),VRAM_D,256*192*2);videoSetMode(MODE_FB3);
    // The menu uses the HUD console. Preserve the city-map BG underneath it.
    bool mapVisible=(REG_DISPCNT_SUB&DISPLAY_BG3_ACTIVE)!=0;
    REG_DISPCNT_SUB&=~DISPLAY_BG3_ACTIVE;
    int page=0,selected=0;const char* notice="";bool dirty=true;Result result=Resume;
    while(pmMainLoop()) {
        if(dirty) {
            consoleClear();
            std::printf("\x1b[2;4HGRAND THEFT AUTO\x1b[4;8HVICE CITY");
            if(page==0) {
                std::printf("\x1b[7;4H%s",paused ? "PAUSE MENU" : "MAIN MENU");
                const char* normal[]={"START GAME","OPTIONS","CUTSCENES","REPLAY INTRO"};
                const char* pause[]={"RESUME GAME","MAP","MISSIONS","CUTSCENES","NEW GAME","OPTIONS"};
                for(int i=0;i<(paused ? 6 : 4);++i)
                    std::printf("\x1b[%d;4H%c %s",10+i*2,i==selected ? '>' : ' ',paused ? pause[i] : normal[i]);
            } else if(page==1) {
                std::printf("\x1b[7;4HSTART GAME\x1b[11;4H> NEW GAME\x1b[15;4HB: BACK");
            } else if(page==2) {
                std::printf("\x1b[7;4HOPTIONS\x1b[10;4H%c SUBTITLES: %s\x1b[12;4H%c CONTROLLER SETUP",
                    selected==0 ? '>' : ' ',Cinematic::subtitlesEnabled ? "ON" : "OFF",selected==1 ? '>' : ' ');
            } else if(page==4 || page==5) {
                const bool movies=page==5;const unsigned total=movies ? MissionLibrary::movieCount() : MissionLibrary::count();
                std::printf("\x1b[6;1H%s %d/%u",movies ? "CUTSCENES" : "MISSIONS",selected+1,total);
                const int first=selected/8*8;
                for(int i=first;i<std::min(first+8,int(total));++i){
                    const char* title=movies ? MissionLibrary::movie(i).name : MissionLibrary::mission(i).title;
                    const char type=movies ? (MissionLibrary::movie(i).available ? '+' : '-') :
                        MissionLibrary::mission(i).kind==MissionLibrary::Adapted ? 'A' : MissionLibrary::mission(i).kind==MissionLibrary::Route ? 'R' : MissionLibrary::mission(i).kind==MissionLibrary::Activity ? '+' : '-';
                    std::printf("\x1b[%d;0H%c%02d %c %.25s",8+i-first,i==selected ? '>' : ' ',i,type,title);
                }
                std::printf("\x1b[17;0H%s",movies ? "Start:skip scene Select:stop" : "A:story R:early +:activity");
                std::printf("\x1b[18;0H%s",movies ? MissionLibrary::movie(selected).title : MissionLibrary::mission(selected).kind==MissionLibrary::Route ? "Early objectives; scripts pending" : "Choose a mission to replay");
                std::printf("\x1b[20;0H%.32s",notice);
            } else {
                std::printf("\x1b[7;3HCONTROLLER SETUP\x1b[9;1HD-pad: move / steer / throttle\x1b[11;1HX/B: camera up/down\x1b[12;1HY/A: camera left/right\x1b[14;1HL: jump / brake\x1b[15;1HR: sprint / attack\x1b[16;1HL+R: enter / exit stopped car\x1b[18;1HStart: pause menu\x1b[19;1HSelect: restart journey");
            }
            std::printf("\x1b[22;1H%s",page>=4 ? "D-pad:pick/page A:OK B:back" : page==3 ? "B: back" : "Up/down: choose A: OK B: back");dirty=false;
        }
        swiWaitForVBlank();scanKeys();unsigned down=keysDown();
        int count=page==4 ? MissionLibrary::count() : page==5 ? MissionLibrary::movieCount() : page==0 ? (paused ? 6 : 4) : page==2 ? 2 : 1;
        if(page>=4 && (down&(KEY_LEFT|KEY_RIGHT))){selected=(selected+((down&KEY_LEFT) ? count-std::min(8,count) : 8))%count;notice="";dirty=true;}
        if(down&(KEY_UP|KEY_DOWN)) {selected=(selected+((down&KEY_UP) ? count-1 : 1))%count;dirty=true;}
        if(down&KEY_B) {
            if(page){page=page==3 ? 2 : 0;selected=0;dirty=true;}
            else if(paused){result=Resume;break;}
        }
        if(!(down&(KEY_A|KEY_START)))continue;
        if(page==4){
            if(MissionLibrary::mission(selected).kind==MissionLibrary::Catalog){notice="World setup runs automatically";dirty=true;continue;}
            selectedMission=selected;result=Mission;break;
        }
        if(page==5){
            const auto& movie=MissionLibrary::movie(selected);
            if(!movie.available){notice="Sequence adapter pending";dirty=true;continue;}
            char path[96];std::snprintf(path,sizeof(path),"nitro:/cutscenes/%s.vcv",movie.name);
            consoleClear();auto played=Cinematic::play(path,movie.title);
            notice=played==Cinematic::Error ? "Movie unavailable; choose another" : "";
            dmaCopy(art.get(),VRAM_D,256*192*2);videoSetMode(MODE_FB3);dirty=true;continue;
        }
        if(page==3){page=2;selected=0;dirty=true;continue;}
        if(page==2) {
            if(selected==0)Cinematic::subtitlesEnabled=!Cinematic::subtitlesEnabled;
            else page=3;
            dirty=true;continue;
        }
        if(page==1){result=NewGame;break;}
        if(paused) {
            if(selected==0){result=Resume;break;}
            if(selected==1){result=Map;break;}
            page=selected==2 ? 4 : selected==3 ? 5 : selected==4 ? 1 : 2;
        } else {
            if(selected==3) {
                if(!movies()){result=Error;break;}
                // Cinematic restores 3D mode; the untouched framebuffer remains.
                dmaCopy(art.get(),VRAM_D,256*192*2);videoSetMode(MODE_FB3);dirty=true;continue;
            }
            page=selected==0 ? 1 : selected==2 ? 5 : 2;
        }
        selected=0;dirty=true;
    }
    videoSetMode(MODE_0_3D);
    if(mapVisible)REG_DISPCNT_SUB|=DISPLAY_BG3_ACTIVE;
    consoleClear();return result;
}
}
