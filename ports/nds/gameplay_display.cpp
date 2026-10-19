#include "gameplay_display.h"
#include <nds.h>

namespace slice {
#ifdef REVC_GAMEPLAY_CINEMA_RESOLUTION
namespace {
volatile bool active=false,captured=false;
bool initialized=false;
int bitmap=-1;
constexpr unsigned Capture=DCAP_ENABLE|DCAP_MODE(DCAP_MODE_A)|
    DCAP_SRC_A(DCAP_SRC_A_3DONLY)|DCAP_SIZE(DCAP_SIZE_128x128)|DCAP_BANK(DCAP_BANK_VRAM_D);

void present() {
    if(!active)return;
    // Capture finishes during visible scanout. Copy only a completed image at
    // VBlank; this also works when simulation/SD I/O takes several refreshes.
    // DMA 2 belongs to this presenter. libnds' ordinary dmaCopy uses DMA 3.
    if(captured && !(REG_DISPCAPCNT&DCAP_ENABLE))
        dmaCopyWords(2,VRAM_D,BG_BMP_RAM(0),GameplayDisplay::Width*GameplayDisplay::Height*2);
    REG_DISPCAPCNT=Capture;
    captured=true;
}
}
#endif

void GameplayDisplay::init() {
#ifdef REVC_GAMEPLAY_CINEMA_RESOLUTION
    if(initialized)return;
    swiWaitForVBlank();
    // A/B remain the renderer's 256 KiB of texture VRAM, E its palettes,
    // and C the bottom map/HUD. F/G provide a separate 32 KiB bitmap.
    vramSetBankF(VRAM_F_MAIN_BG_0x06000000);
    vramSetBankG(VRAM_G_MAIN_BG_0x06004000);
    vramSetBankD(VRAM_D_LCD);
    videoSetMode(MODE_5_2D|ENABLE_3D);
    bitmap=bgInit(2,BgType_Bmp16,BgSize_B16_128x128,0,0);
    bgWrapOff(bitmap);
    dmaFillHalfWords(0,BG_BMP_RAM(0),128*128*2);
    BG_PALETTE[0]=RGB15(0,0,0);
    // Affine sampling advances half a source pixel per screen pixel. Negative
    // source Y and the cleared rows below Height give exact 24-pixel bars.
    bgSetAffineMatrixScroll(bitmap,128,0,0,128,0,-Top*128);
    // DS viewport Y is bottom-up; capture reads the top-left 128x128 area.
    glViewport(0,192-Height,Width-1,191);
    // Preserve horizontal framing and the size of people/cars. Crop the old
    // 4:3 view vertically to 16:9 instead of stretching it into the letterbox.
    glMatrixMode(GL_PROJECTION);
    glScalef32(1<<12,(4<<12)/3,1<<12);
    glMatrixMode(GL_MODELVIEW);
    initialized=true;
    irqSet(IRQ_VBLANK,present);
    irqEnable(IRQ_VBLANK);
    resume();
#endif
}

void GameplayDisplay::suspend() {
#ifdef REVC_GAMEPLAY_CINEMA_RESOLUTION
    if(!initialized)return;
    swiWaitForVBlank();
    active=false;
    REG_DISPCAPCNT=0;
    captured=false;
#endif
}

void GameplayDisplay::resume() {
#ifdef REVC_GAMEPLAY_CINEMA_RESOLUTION
    if(!initialized)return;
    swiWaitForVBlank();
    vramSetBankD(VRAM_D_LCD);
    videoSetMode(MODE_5_2D|ENABLE_3D|DISPLAY_BG2_ACTIVE);
    bgSetAffineMatrixScroll(bitmap,128,0,0,128,0,-Top*128);
    captured=false;
    active=true;
#endif
}
}
