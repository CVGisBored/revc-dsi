// Build-only probe to expose missing platform symbols for the complete game.
// It is never packaged as a ROM or executed.
#include "common.h"
#include "Game.h"
#include "skeleton.h"
int main()
{
    if (!CGame::InitialiseOnceBeforeRW() || !CGame::InitialiseRenderWare() ||
        !CGame::InitialiseOnceAfterRW() || !CGame::Initialise("DATA/GTA_VC.DAT")) return 1;
    CGame::Process();
    return 0;
}
