#include "packed.h"
namespace slice {
size_t unpackLz10(const unsigned char* source,size_t size,unsigned char* output,size_t capacity) {
    if(size<4 || source[0]!=0x10)return 0;
    const size_t expected=source[1]|size_t(source[2])<<8|size_t(source[3])<<16;
    if(!expected || expected>capacity)return 0;
    size_t read=4,written=0;
    while(written<expected) {
        if(read>=size)return 0;
        unsigned flags=source[read++];
        for(unsigned bit=0;bit<8 && written<expected;++bit) {
            if(flags&(128u>>bit)) {
                if(read+2>size)return 0;
                unsigned value=unsigned(source[read])<<8|source[read+1];read+=2;
                const size_t length=(value>>12)+3,distance=(value&4095)+1;
                if(distance>written || written+length>expected)return 0;
                for(size_t i=0;i<length;++i){output[written]=output[written-distance];++written;}
            }else {
                if(read>=size)return 0;
                output[written++]=source[read++];
            }
        }
    }
    return read==size ? written : 0;
}
size_t unpackTerrain(const unsigned char* source,size_t size,unsigned char* output,size_t capacity) {
    if(size<4)return 0;
    const size_t expected=source[1]|size_t(source[2])<<8|size_t(source[3])<<16;
    if(expected%34)return 0;
    return unpackLz10(source,size,output,capacity);
}
}
