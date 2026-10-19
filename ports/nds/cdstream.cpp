// Synchronous SD/FAT implementation of reVC's sector streaming API.
// Each read completes before returning; no pthread or semaphore dependency.
#ifdef REVC_STREAM_HOST_TEST
#include <cstdio>
#include <cstdint>
typedef int int32;
typedef unsigned int uint32;
#define VALIDATE_SIZE(type, size) static_assert(sizeof(type) >= size, "host layout")
#define MAX_CDCHANNELS 5
#define MAX_CDIMAGES 8
#define fcaseopen fopen
#else
#include "common.h"
#include "crossplatform.h"
#endif
#include "CdStream.h"
#include <cstdio>
#include <cstring>
#include <climits>

namespace {
FILE* images[MAX_CDIMAGES] = {};
char names[MAX_CDIMAGES][256] = {};
int status[MAX_CDCHANNELS] = {};
int channels = 0, imageCount = 0, lastPosition = 0;
bool validChannel(int channel) { return channel >= 0 && channel < channels; }
}

void CdStreamInit(int numChannels)
{
    CdStreamRemoveImages();
    channels = numChannels > 0 && numChannels <= MAX_CDCHANNELS ? numChannels : 0;
    std::memset(status, 0, sizeof(status));
    lastPosition = 0;
}
void CdStreamInitThread(void) { /* Deliberately synchronous. */ }
void CdStreamShutdown(void) { CdStreamRemoveImages(); channels = 0; }

int CdStreamRead(int channel, void* buffer, unsigned int offset, unsigned int sectors)
{
    if (!validChannel(channel)) return STREAM_NONE;
    const unsigned image = offset >> 24;
    const unsigned sector = offset & 0xffffff;
    const uint64_t byteOffset = static_cast<uint64_t>(sector) * CDSTREAM_SECTOR_SIZE;
    const uint64_t byteCount = static_cast<uint64_t>(sectors) * CDSTREAM_SECTOR_SIZE;
    if (!buffer || image >= static_cast<unsigned>(imageCount) || !images[image] ||
        byteOffset > LONG_MAX || byteCount > SIZE_MAX || sectors == 0) {
        status[channel] = STREAM_ERROR;
        return STREAM_NONE;
    }
    FILE* file = images[image];
    clearerr(file);
    if (std::fseek(file, static_cast<long>(byteOffset), SEEK_SET) != 0 ||
        std::fread(buffer, 1, static_cast<size_t>(byteCount), file) != byteCount) {
        status[channel] = STREAM_ERROR;
        return STREAM_NONE;
    }
    lastPosition = static_cast<int>(offset + sectors);
    status[channel] = STREAM_NONE;
    return STREAM_SUCCESS;
}
int CdStreamGetStatus(int channel)
{
    if (!validChannel(channel)) return STREAM_ERROR;
    const int result = status[channel];
    status[channel] = STREAM_NONE;
    return result;
}
int CdStreamSync(int channel) { return validChannel(channel) ? status[channel] : STREAM_ERROR; }
int CdStreamGetLastPosn(void) { return lastPosition; }

bool CdStreamAddImage(const char* path)
{
    if (!path || imageCount >= MAX_CDIMAGES || std::strlen(path) >= sizeof(names[0])) return false;
    FILE* file = fcaseopen(path, "rb");
    if (!file) return false;
    std::strcpy(names[imageCount], path);
    images[imageCount++] = file;
    return true;
}
char* CdStreamGetImageName(int cd) { return cd >= 0 && cd < imageCount ? names[cd] : nullptr; }
int32 CdStreamGetNumImages(void) { return imageCount; }
void CdStreamRemoveImages(void)
{
    for (int i = 0; i < imageCount; i++) {
        if (images[i]) std::fclose(images[i]);
        images[i] = nullptr;
        names[i][0] = 0;
    }
    imageCount = 0;
}
uint32 GetGTA3ImgSize(void)
{
    if (imageCount == 0) return 0;
    FILE* file = images[0];
    const long old = std::ftell(file);
    if (old < 0 || std::fseek(file, 0, SEEK_END) != 0) return 0;
    const long size = std::ftell(file);
    std::fseek(file, old, SEEK_SET);
    return size >= 0 ? static_cast<uint32>(size) : 0;
}

void AddToQueue(Queue* queue, int item)
{
    if (!queue || !queue->items || queue->size <= 1) return;
    const int next = (queue->tail + 1) % queue->size;
    if (next == queue->head) return;
    queue->items[queue->tail] = item;
    queue->tail = next;
}
int32 GetFirstInQueue(Queue* queue)
{
    return queue && queue->items && queue->size > 0 && queue->head != queue->tail
        ? queue->items[queue->head] : -1;
}
void RemoveFirstInQueue(Queue* queue)
{
    if (queue && queue->size > 0 && queue->head != queue->tail)
        queue->head = (queue->head + 1) % queue->size;
}
