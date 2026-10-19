#include <nds.h>
#include <calico/system/tick.h>
#include <cstdio>
#include "cinematic.h"
#include "rom_storage.h"
int main(int argc,char** argv){
    tickInit();videoSetMode(MODE_0_3D);videoSetModeSub(MODE_0_2D);vramSetBankC(VRAM_C_SUB_BG);
    PrintConsole hud;consoleInit(&hud,0,BgType_Text4bpp,BgSize_T_256x256,31,0,false,true);consoleSelect(&hud);lcdMainOnTop();glInit();
    bool mounted=slice::mountGameAssets(argc,argv);
    if(mounted){auto result=slice::Cinematic::play("nitro:/sample.vcv","30 FPS SOURCE-ANIMATION PROBE");auto stats=slice::Cinematic::stats;
        consoleClear();std::printf("30 FPS CUTSCENE PROBE\nResult %d / error %d\nSource: %u fps\nPresented: %u\nDecoded: %u\nDropped: %u\nElapsed: %u ms\nAverage: %u fps\nPeak decode: %u us\n",result,slice::Cinematic::lastError,stats.sourceFps,stats.presented,stats.decoded,stats.dropped,stats.milliseconds,stats.milliseconds ? stats.presented*1000/stats.milliseconds : 0,stats.peakDecodeMicroseconds);
    }else std::printf("Probe asset mount failed\n");
    while(pmMainLoop())swiWaitForVBlank();return 0;
}
