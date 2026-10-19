#pragma once
#include <cstddef>
namespace slice {
size_t unpackLz10(const unsigned char* source,size_t size,unsigned char* output,size_t capacity);
size_t unpackTerrain(const unsigned char* source,size_t size,unsigned char* output,size_t capacity);
}
