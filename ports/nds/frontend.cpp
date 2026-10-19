#include "frontend.h"
#include "cinematic.h"
#include <nds.h>
#include <cstdio>
#include <memory>
#include <new>
#include <initializer_list>
namespace slice {
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
    int page=0,selected=0;bool dirty=true;Result result=Resume;
    while(pmMainLoop()) {
        if(dirty) {
            consoleClear();
            std::printf("\x1b[2;4HGRAND THEFT AUTO\x1b[4;8HVICE CITY");
            if(page==0) {
                std::printf("\x1b[7;4H%s",paused ? "PAUSE MENU" : "MAIN MENU");
                const char* normal[]={"START GAME","OPTIONS","REPLAY INTRO"};
                const char* pause[]={"RESUME GAME","MAP","NEW GAME","OPTIONS"};
                for(int i=0;i<(paused ? 4 : 3);++i)
                    std::printf("\x1b[%d;4H%c %s",10+i*2,i==selected ? '>' : ' ',paused ? pause[i] : normal[i]);
            } else if(page==1) {
                std::printf("\x1b[7;4HSTART GAME\x1b[11;4H> NEW GAME\x1b[15;4HB: BACK");
            } else if(page==2) {
                std::printf("\x1b[7;4HOPTIONS\x1b[10;4H%c SUBTITLES: %s\x1b[12;4H%c CONTROLLER SETUP",
                    selected==0 ? '>' : ' ',Cinematic::subtitlesEnabled ? "ON" : "OFF",selected==1 ? '>' : ' ');
            } else {
                std::printf("\x1b[7;3HCONTROLLER SETUP\x1b[9;1HD-pad: move / steer / throttle\x1b[11;1HX/B: camera up/down\x1b[12;1HY/A: camera left/right\x1b[14;1HL: jump / brake\x1b[15;1HR: sprint / attack\x1b[16;1HL+R: enter / exit stopped car\x1b[18;1HStart: pause menu\x1b[19;1HSelect: restart journey");
            }
            std::printf("\x1b[22;1H%s",page==3 ? "B: back" : "Up/down: choose A: OK B: back");dirty=false;
        }
        swiWaitForVBlank();scanKeys();unsigned down=keysDown();
        int count=page==0 ? (paused ? 4 : 3) : page==2 ? 2 : 1;
        if(down&(KEY_UP|KEY_DOWN)) {selected=(selected+((down&KEY_UP) ? count-1 : 1))%count;dirty=true;}
        if(down&KEY_B) {
            if(page){page=page==3 ? 2 : 0;selected=0;dirty=true;}
            else if(paused){result=Resume;break;}
        }
        if(!(down&(KEY_A|KEY_START)))continue;
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
            page=selected==2 ? 1 : 2;
        } else {
            if(selected==2) {
                if(!movies()){result=Error;break;}
                // Cinematic restores 3D mode; the untouched framebuffer remains.
                dmaCopy(art.get(),VRAM_D,256*192*2);videoSetMode(MODE_FB3);dirty=true;continue;
            }
            page=selected==0 ? 1 : 2;
        }
        selected=0;dirty=true;
    }
    videoSetMode(MODE_0_3D);
    if(mapVisible)REG_DISPCNT_SUB|=DISPLAY_BG3_ACTIVE;
    consoleClear();return result;
}
}
