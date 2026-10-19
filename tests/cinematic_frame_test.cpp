#include "cinematic_frame.h"
#include <cassert>
#include <cstring>
#include <vector>
#include <cstdio>
static std::vector<unsigned char> literals(const std::vector<unsigned char>& raw){
    unsigned h=unsigned(raw.size())<<8|0x10;std::vector<unsigned char> out;
    for(int i=0;i<4;++i)out.push_back(h>>(8*i));for(unsigned i=0;i<raw.size();i+=8){out.push_back(0);for(unsigned j=i;j<raw.size() && j<i+8;++j)out.push_back(raw[j]);}return out;
}
int main(){
    std::vector<unsigned char> old(9728),next(9728),scratch(9872),delta(9872);
    for(unsigned i=0;i<old.size();++i)old[i]=i%251;auto key=literals(old);
    assert(slice::decodeCinematicFrame(3,128,72,true,key.data(),key.size(),nullptr,next.data(),scratch.data()) && next==old);
    for(unsigned b=0;b<144;++b)delta[512+b]=40;
    // Move one interior block left by one source pixel; change one residual.
    delta[512+17]=39;delta[656+8*128+8]=3;auto motion=literals(delta);
    assert(slice::decodeCinematicFrame(3,128,72,false,motion.data(),motion.size(),old.data(),next.data(),scratch.data()));
    assert(next[512+8*128+8]==(old[512+8*128+7]^3));assert(next[512]==old[512]);
    delta[512]=39;motion=literals(delta); // Left border cannot predict outside frame.
    assert(!slice::decodeCinematicFrame(3,128,72,false,motion.data(),motion.size(),old.data(),next.data(),scratch.data()));
    delta[512]=81;motion=literals(delta);assert(!slice::decodeCinematicFrame(3,128,72,false,motion.data(),motion.size(),old.data(),next.data(),scratch.data()));
    assert(!slice::decodeCinematicFrame(3,128,72,false,key.data(),key.size(),nullptr,next.data(),scratch.data()));
    std::vector<unsigned char> xorBytes(9728,7);auto xorFrame=literals(xorBytes);
    assert(slice::decodeCinematicFrame(2,128,72,false,xorFrame.data(),xorFrame.size(),old.data(),next.data(),nullptr));for(unsigned i=0;i<old.size();++i)assert(next[i]==(old[i]^7));
    assert(!slice::decodeCinematicFrame(3,128,72,true,key.data(),key.size()-1,nullptr,next.data(),scratch.data()));
    std::puts("Cinematic codec: keys, XOR deltas, motion/residual reconstruction, border validation, invalid vectors and truncated frames passed");
}
