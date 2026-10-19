#pragma once
namespace slice {
class Cinematic {
public:
    enum Result { Completed, Skipped, SkipAll, Error };
    // Synchronous streaming freezes gameplay while the DS audio hardware plays the ring buffer.
    static Result play(const char* path,const char* title);
    struct PlaybackStats {unsigned sourceFps=0,presented=0,decoded=0,dropped=0,milliseconds=0,peakDecodeMicroseconds=0;};
    static PlaybackStats stats;
    static bool subtitlesEnabled;
    static int lastError;
    static const char* lastPath;
};
}
