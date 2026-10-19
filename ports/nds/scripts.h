#pragma once
#include <cstdint>
#include <cstdio>
#include <memory>
namespace slice {
// Bounded SCM bring-up. Missing engine commands stop execution explicitly.
class Scripts {
public:
    enum Status { Invalid, Running, Waiting, Unsupported, Finished };
    bool open(const char* path);
    bool start(int mission=-1);
    void step(uint32_t milliseconds,unsigned budget=64);
    unsigned missionCount() const {return missions;}
    Status status() const {return state;}
    unsigned opcode() const {return blockedOpcode;}
    unsigned address() const {return commandAddress;}
    const char* name() const {return scriptName;}
    int32_t local(unsigned index) const {return index<18 ? locals[index] : 0;}
    ~Scripts() {if(file)std::fclose(file);}
private:
    std::unique_ptr<uint8_t[]> main,mission;
    FILE* file=nullptr;
    uint32_t offsets[128]{},mainSize=0,missionSize=0,variableSize=0,pc=0,length=0;
    uint32_t wake=0,commandAddress=0;
    unsigned missions=0,blockedOpcode=0;
    int32_t locals[18]{};
    char scriptName[9]{};
    bool inMission=false;
    bool condition=false;
    unsigned andOr=0,stackSize=0;
    uint32_t stack[6]{};
    Status state=Invalid;
    bool read(unsigned bytes,uint32_t& value);
    bool parameter(int32_t& value);
    bool store(int32_t value);
    bool jump(int32_t target);
};
}
