#include <chrono>
long _dwOperatingSystemVersion = 0;
unsigned int gGameState = 0;
extern "C" double psTimer()
{
    using namespace std::chrono;
    return duration<double, std::milli>(steady_clock::now().time_since_epoch()).count();
}
