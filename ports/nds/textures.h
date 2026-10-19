#pragma once
#include <cstdint>
#include <cstdio>
#include <memory>
namespace slice {
struct TexturePage {
    uint32_t flags;
    uint16_t palette[256];
    unsigned char pixels[64*64];
};
class TextureFile {
public:
    bool open(const char* path);
    void close();
    bool loadAverages(const char* path);
    uint16_t average(unsigned id) const {return averages && id && id<=pages ? averages[id-1] : 32767;}
    bool load(unsigned id,TexturePage& page);
    unsigned count() const {return pages;}
    unsigned key(unsigned id) const {return id && id<=pages ? (offsets ? offsets[id-1] : 8+(id-1)*sizeof(TexturePage)) : 0;}
    unsigned cacheHits() const {return hits;}
    unsigned diskReads() const {return reads;}
    ~TextureFile() {close();}
private:
    FILE* file=nullptr;
    unsigned pages=0;
    std::unique_ptr<uint32_t[]> offsets;
    std::unique_ptr<uint16_t[]> averages;
    struct Cached {unsigned key=0,age=0;TexturePage page;};
    std::unique_ptr<Cached[]> cache;
    unsigned clock=0,hits=0,reads=0;
};
}

namespace slice {
struct DetailTexturePage {uint32_t flags;uint16_t palette[256];unsigned char pixels[128*128];};
class DetailTextureFile {
public:
    bool open(const char* path);void close();
    unsigned key(unsigned id) const {return offsets && id && id<=pages ? offsets[id-1] : 0;}
    bool load(unsigned id,DetailTexturePage& page);
    ~DetailTextureFile(){close();}
private:
    FILE* file=nullptr;unsigned pages=0,length=0;
    std::unique_ptr<uint32_t[]> offsets;
    std::unique_ptr<unsigned char[]> packed;
};
}
