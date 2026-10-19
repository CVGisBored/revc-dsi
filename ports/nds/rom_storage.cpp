#include "rom_storage.h"
#include <nds.h>
#include <filesystem.h>
#include <calico/nds/nitrorom.h>
#include <cstdio>
#include <cstring>
#include <initializer_list>
namespace slice {
namespace {
struct RomFile {FILE* file=nullptr;unsigned length=0;};
RomFile image;NitroRom filesystem;
bool readImage(void* user,u32 offset,void* bytes,u32 size){
    auto& r=*static_cast<RomFile*>(user);
    return r.file && offset<=r.length && size<=r.length-offset &&
        (std::ftell(r.file)==long(offset) || !std::fseek(r.file,offset,SEEK_SET)) && std::fread(bytes,1,size,r.file)==size;
}
void closeImage(void* user){auto& r=*static_cast<RomFile*>(user);if(r.file)std::fclose(r.file);r.file=nullptr;}
const NitroRomIface fileInterface{readImage,closeImage};
bool tryImage(const char* path){
    if(!path || !*path)return false;
    image.file=std::fopen(path,"rb");if(!image.file)return false;
    EnvNdsHeader header{};const auto& running=*g_envAppNdsHeader;
    bool valid=std::fread(&header,1,sizeof(header),image.file)==sizeof(header) &&
        !std::memcmp(header.gamecode,running.gamecode,4) && header.arm9_size==running.arm9_size &&
        header.arm9_rom_offset==running.arm9_rom_offset && header.arm7_size==running.arm7_size &&
        header.fat_rom_offset==running.fat_rom_offset && header.fat_size==running.fat_size &&
        header.fnt_rom_offset==running.fnt_rom_offset && header.fnt_size==running.fnt_size;
    long length=-1;if(valid && !std::fseek(image.file,0,SEEK_END))length=std::ftell(image.file);
    valid=valid && length>512 && length<0x7fffffff && header.fat_size && header.fnt_size &&
        uint64_t(header.fat_rom_offset)+header.fat_size<=uint64_t(length) &&
        uint64_t(header.fnt_rom_offset)+header.fnt_size<=uint64_t(length);
    if(!valid){closeImage(&image);return false;}
    image.length=unsigned(length);
    NitroRomParams parameters{header.fat_rom_offset,header.fat_size,header.fnt_rom_offset,header.fnt_size,0};
    if(!nitroromOpen(&filesystem,&parameters,&fileInterface,&image)){closeImage(&image);return false;}
    if(nitroFSMount(&filesystem))return true;
    nitroromClose(&filesystem);return false;
}
}
bool mountGameAssets(int argc,char** argv){
    // TWiLight's supplied ROM path remains the normal mounting route. Direct
    // emulator/card boot may fail for a larger cartridge image; fall back to
    // the matching SD file without requiring loose assets or changing the ROM.
    if(nitroFSInit(nullptr))return true;
    if(argc>0 && argv && tryImage(argv[0]))return true;
    for(const char* path : {"fat:/rom.nds","fat:/revc_dsi.nds","sd:/rom.nds","sd:/revc_dsi.nds"})if(tryImage(path))return true;
    return false;
}
}
