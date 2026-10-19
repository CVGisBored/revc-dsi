#include "actor_texture.h"
#include <nds.h>
#include <cstdio>
#include <cstring>
#include <memory>
#include <new>
namespace slice {
bool ActorTexture::open(const char* path) {
    FILE* f=std::fopen(path,"rb");if(!f)return false;
    char magic[4];uint16_t width=0,height=0,palette[256];
    bool ok=std::fread(magic,1,4,f)==4 && !std::memcmp(magic,"ATX1",4) &&
        std::fread(&width,2,1,f)==1 && std::fread(&height,2,1,f)==1 &&
        (width==128 || width==256) && (height==128 || height==256) && std::fread(palette,2,256,f)==256;
    std::unique_ptr<unsigned char[]> pixels;
    if(ok)pixels.reset(new(std::nothrow) unsigned char[width*height]);
    ok=ok && pixels && std::fread(pixels.get(),1,width*height,f)==unsigned(width*height) && std::fgetc(f)==EOF;
    std::fclose(f);if(!ok)return false;
    if(!handle)glGenTextures(1,&handle);
    glBindTexture(GL_TEXTURE,handle);
    auto sizeX=width==128 ? TEXTURE_SIZE_128 : TEXTURE_SIZE_256;
    auto sizeY=height==128 ? TEXTURE_SIZE_128 : TEXTURE_SIZE_256;
    ok=glTexImage2D(GL_TEXTURE,0,GL_RGB256,sizeX,sizeY,0,GL_TEXTURE_WRAP_S|GL_TEXTURE_WRAP_T,pixels.get());
    if(ok)glColorTableEXT(GL_TEXTURE,0,256,0,0,palette);
    glBindTexture(GL_TEXTURE,0);return ok;
}
void ActorTexture::bind() const {glBindTexture(GL_TEXTURE,handle);}
}
