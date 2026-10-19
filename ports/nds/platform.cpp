#include <nds.h>
#include <cstddef>
#include <calico/system/tick.h>

long _dwOperatingSystemVersion = 0;
unsigned int gGameState = 0; // GS_START_UP

extern "C" double psTimer()
{
    return static_cast<double>(tickGetCount()) * 1000.0 / TICK_FREQ;
}
