#include <cstdio>
#include <cstring>
#include <cstdlib>
typedef int int32;
typedef unsigned int uint32;
#define VALIDATE_SIZE(type, size) static_assert(sizeof(type) >= size, "host layout")
#include "CdStream.h"

static void require(bool value) { if (!value) std::abort(); }
int main(int argc, char** argv)
{
    require(argc == 2);
    unsigned char data[4096];
    for (int i = 0; i < 4096; i++) data[i] = static_cast<unsigned char>(i / 2048 + 17);
    FILE* file = std::fopen(argv[1], "wb");
    require(file != nullptr);
    require(std::fwrite(data, 1, sizeof(data), file) == sizeof(data));
    std::fclose(file);
    CdStreamInit(2);
    require(!CdStreamAddImage("/definitely-missing-revc-file"));
    require(CdStreamAddImage(argv[1]));
    require(GetGTA3ImgSize() == 4096);
    unsigned char read[2048] = {};
    require(CdStreamRead(0, read, 1, 1) == STREAM_SUCCESS);
    require(read[0] == 18 && read[2047] == 18);
    require(CdStreamSync(0) == STREAM_NONE);
    require(CdStreamGetLastPosn() == 2);
    require(CdStreamRead(0, read, 2, 1) == STREAM_NONE);
    require(CdStreamGetStatus(0) == STREAM_ERROR);
    require(CdStreamGetStatus(0) == STREAM_NONE);
    require(CdStreamRead(-1, read, 0, 1) == STREAM_NONE);
    require(CdStreamRead(0, nullptr, 0, 1) == STREAM_NONE);
    require(CdStreamRead(0, read, 1u << 24, 1) == STREAM_NONE);
    require(CdStreamAddImage(argv[1]));
    require(CdStreamRead(1, read, (1u << 24) | 1, 1) == STREAM_SUCCESS);
    require(read[0] == 18);
    CdStreamShutdown();
    require(CdStreamGetNumImages() == 0);
    std::puts("Synchronous streaming: bounds, offsets, channels, errors, multiple images passed");
}
