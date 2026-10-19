#include "textures.h"
#include <cstring>
#include <new>
namespace slice {
bool TextureFile::open(const char* path) {
    static_assert(sizeof(TexturePage)==4612,"Texture page ABI");close();
    file=std::fopen(path,"rb");if(!file)return false;
    char magic[4];uint32_t count=0;
    bool valid=std::fread(magic,1,4,file)==4 &&
        std::fread(&count,4,1,file)==1 && count>0 && count<32768;
    uint32_t unique=count,start=8;
    if(valid && !std::memcmp(magic,"VTX2",4)) {
        valid=std::fread(&unique,4,1,file)==1 && unique>0 && unique<=count;
        if(valid)offsets.reset(new(std::nothrow) uint32_t[count]);
        valid=valid && offsets && std::fread(offsets.get(),4,count,file)==count;start=12+count*4;
        if(valid)for(unsigned i=0;i<count;++i)if(offsets[i]<start || (offsets[i]-start)%sizeof(TexturePage) ||
            offsets[i]>=start+unique*sizeof(TexturePage))valid=false;
    }else valid=valid && !std::memcmp(magic,"VTX1",4);
    valid=valid && !std::fseek(file,0,SEEK_END) && std::ftell(file)==long(start+unique*sizeof(TexturePage));
    if(!valid) {close();return false;}
    pages=count;cache.reset(new(std::nothrow) Cached[8]);return true;
}
void TextureFile::close() {if(file)std::fclose(file);file=nullptr;pages=0;offsets.reset();averages.reset();cache.reset();clock=hits=reads=0;}
bool TextureFile::loadAverages(const char* path) {
    FILE* f=std::fopen(path,"rb");if(!f)return false;char magic[4];uint32_t n=0;
    bool ok=std::fread(magic,1,4,f)==4 && !std::memcmp(magic,"VTC1",4) && std::fread(&n,4,1,f)==1 && n==pages;
    if(ok)averages.reset(new(std::nothrow) uint16_t[n]);ok=ok && averages && std::fread(averages.get(),2,n,f)==n && std::fgetc(f)==EOF;
    if(ok)for(unsigned i=0;i<n;++i)if(averages[i]>=32768)ok=false;
    std::fclose(f);if(!ok)averages.reset();return ok;
}
bool TextureFile::load(unsigned id,TexturePage& page) {
    if(!file || !id || id>pages)return false;
    unsigned address=key(id);Cached* slot=cache ? &cache[0] : nullptr;
    if(cache)for(unsigned i=0;i<8;++i){auto& c=cache[i];if(c.key==address){page=c.page;c.age=++clock;++hits;return true;}if(c.age<slot->age)slot=&c;}
    if(std::fseek(file,address,SEEK_SET) || std::fread(&page,sizeof(page),1,file)!=1 || page.flags&~1u)return false;
    ++reads;if(slot){slot->page=page;slot->key=address;slot->age=++clock;}return true;
}
}

#include "packed.h"
namespace slice {
bool DetailTextureFile::open(const char* path) {
    close();file=std::fopen(path,"rb");if(!file)return false;
    char magic[4];uint32_t count=0,unique=0;
    bool valid=std::fread(magic,1,4,file)==4 && !std::memcmp(magic,"VHD1",4) && std::fread(&count,4,1,file)==1 && count>0 && count<32768 && std::fread(&unique,4,1,file)==1 && unique>0 && unique<=count;
    if(valid){offsets.reset(new(std::nothrow) uint32_t[count]);packed.reset(new(std::nothrow) unsigned char[19200]);}
    valid=valid && offsets && packed && std::fread(offsets.get(),4,count,file)==count && !std::fseek(file,0,SEEK_END);
    long bytes=valid ? std::ftell(file) : -1;valid=valid && bytes>long(12+count*4) && bytes<0x40000000;
    if(valid)for(unsigned i=0;i<count;++i)if(offsets[i] && (offsets[i]<12+count*4 || offsets[i]>unsigned(bytes)-8))valid=false;
    if(!valid){close();return false;}pages=count;length=unsigned(bytes);return true;
}
void DetailTextureFile::close(){if(file)std::fclose(file);file=nullptr;pages=length=0;offsets.reset();packed.reset();}
bool DetailTextureFile::load(unsigned id,DetailTexturePage& page){
    static_assert(sizeof(DetailTexturePage)==16900,"Detail texture ABI");
    unsigned at=key(id);if(!at || std::fseek(file,at,SEEK_SET))return false;
    uint32_t size=0;if(std::fread(&size,4,1,file)!=1 || size<4 || size>19200 || at+4+size>length || std::fread(packed.get(),1,size,file)!=size)return false;
    return unpackLz10(packed.get(),size,reinterpret_cast<unsigned char*>(&page),sizeof(page))==sizeof(page) && !(page.flags&~1u);
}
}
