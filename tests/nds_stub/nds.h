#pragma once
#include <cstdint>
#include <cstddef>
extern uint16_t testFramebuffer[256*192];
#define VRAM_D testFramebuffer
#define RGB15(r,g,b) ((r)|((g)<<5)|((b)<<10))
constexpr unsigned KEY_START=1,KEY_SELECT=2,VRAM_D_LCD=0,MODE_FB3=3,MODE_0_3D=0,SOUND_START=16;
enum SoundMode {SoundMode_Repeat=1};enum SoundFmt {SoundFmt_Pcm8=0};
void DC_FlushRange(const void*,size_t);
void soundInit();void soundEnable();void soundStop(unsigned);void soundSynchronize();
unsigned soundTimerFromHz(unsigned);
void soundPreparePcm(unsigned,unsigned,unsigned,unsigned,SoundMode,SoundFmt,const void*,unsigned,unsigned);
void vramSetBankD(unsigned);void videoSetMode(unsigned);void swiWaitForVBlank();
bool pmMainLoop();void scanKeys();unsigned keysDown();
