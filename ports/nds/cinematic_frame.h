#pragma once
#include <cstddef>
namespace slice {
// Output and previous are separate buffers for motion prediction. Scratch holds
// the unpacked residual; the caller only presents a frame after success.
bool decodeCinematicFrame(unsigned codec,unsigned width,unsigned height,bool key,
    const unsigned char* packed,size_t size,const unsigned char* previous,
    unsigned char* output,unsigned char* scratch);
}
