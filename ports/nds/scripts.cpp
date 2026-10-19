#include "scripts.h"
#include "ScriptCommands.h"
#include <cstring>
#include <new>
#include <algorithm>
namespace slice {
namespace {
uint32_t word(const uint8_t* p) {return p[0]|uint32_t(p[1])<<8|uint32_t(p[2])<<16|uint32_t(p[3])<<24;}
}
bool Scripts::open(const char* path) {
    if(file)std::fclose(file);
    file=nullptr;main.reset();mission.reset();state=Invalid;missions=0;
    file=std::fopen(path,"rb");if(!file)return false;
    uint8_t header[7];
    if(std::fread(header,1,7,file)!=7 || header[0]!=2 || header[1] || header[2]!=1)return false;
    variableSize=word(header+3);
    if(variableSize<8 || variableSize>65536 || std::fseek(file,variableSize,SEEK_SET))return false;
    if(std::fread(header,1,7,file)!=7 || header[0]!=2 || header[1] || header[2]!=1)return false;
    uint32_t objectsEnd=word(header+3);
    if(objectsEnd<variableSize || objectsEnd>256*1024 || std::fseek(file,objectsEnd+8,SEEK_SET))return false;
    uint8_t meta[12];if(std::fread(meta,1,12,file)!=12)return false;
    mainSize=word(meta);missionSize=word(meta+4);missions=meta[8]|unsigned(meta[9])<<8;
    if(!missions || missions>128 || mainSize<=objectsEnd+20+missions*4 || mainSize>256*1024 || !missionSize || missionSize>65536)return false;
    if(std::fread(offsets,4,missions,file)!=missions || std::fseek(file,0,SEEK_END))return false;
    long size=std::ftell(file);
    if(size<long(mainSize) || size>16*1024*1024)return false;
    for(unsigned i=0;i<missions;++i)if(offsets[i]<mainSize || offsets[i]>=static_cast<unsigned long>(size) || (i && offsets[i]<=offsets[i-1]))return false;
    for(unsigned i=0;i<missions;++i)if((i+1<missions ? offsets[i+1] : uint32_t(size))-offsets[i]>missionSize)return false;
    main.reset(new(std::nothrow) uint8_t[mainSize]);mission.reset(new(std::nothrow) uint8_t[missionSize]);
    if(!main || !mission || std::fseek(file,0,SEEK_SET) || std::fread(main.get(),1,mainSize,file)!=mainSize)return false;
    return start();
}
bool Scripts::start(int index) {
    state=Invalid;
    if(!main || index<-1 || index>=int(missions))return false;
    inMission=index>=0;
    if(inMission) {
        if(std::fseek(file,0,SEEK_END))return false;
        length=(index+1<int(missions) ? offsets[index+1] : uint32_t(std::ftell(file)))-offsets[index];
        if(length>missionSize || std::fseek(file,offsets[index],SEEK_SET) || std::fread(mission.get(),1,length,file)!=length)return false;
    } else length=mainSize;
    // No game state is shared with Street Job; this VM is an integration diagnostic.
    std::memset(locals,0,sizeof(locals));std::memset(scriptName,0,sizeof(scriptName));
    pc=wake=blockedOpcode=commandAddress=stackSize=andOr=0;condition=false;state=Running;return true;
}
bool Scripts::read(unsigned n,uint32_t& value) {
    if(pc>length || n>length-pc) {state=Invalid;return false;}
    value=0;const auto* bytes=inMission ? mission.get() : main.get();
    for(unsigned i=0;i<n;++i)value|=uint32_t(bytes[pc++])<<(8*i);
    return true;
}
bool Scripts::parameter(int32_t& value) {
    uint32_t type,raw;if(!read(1,type))return false;
    unsigned bytes=type==1 || type==6 ? 4 : type==4 ? 1 : 2;
    if(type<1 || type>6 || !read(bytes,raw)) {state=Invalid;return false;}
    if(type==2) {
        if(raw<8 || raw+4>variableSize) {state=Invalid;return false;}
        value=int32_t(word(main.get()+raw));
    } else if(type==3) {
        if(raw>=18) {state=Invalid;return false;}
        value=locals[raw];
    } else value=type==4 ? int8_t(raw) : type==5 ? int16_t(raw) : int32_t(raw);
    return true;
}
bool Scripts::store(int32_t value) {
    uint32_t type,index;if(!read(1,type) || !read(2,index))return false;
    if(type==2 && index>=8 && index+4<=variableSize) {
        for(int i=0;i<4;++i)main[index+i]=uint32_t(value)>>(8*i);
    } else if(type==3 && index<18)locals[index]=value;
    else {state=Invalid;return false;}
    return true;
}
bool Scripts::jump(int32_t target) {
    // Mission-relative labels are negative in original Vice City scripts.
    if((inMission && target>=0) || (!inMission && target<0)) {state=Unsupported;return false;}
    uint32_t address=inMission ? uint32_t(-int64_t(target)) : uint32_t(target);
    if(address>=length) {state=Invalid;return false;}
    pc=address;return true;
}
void Scripts::step(uint32_t now,unsigned budget) {
    if(state==Waiting) {if(int32_t(now-wake)<0)return;state=Running;}
    if(state!=Running)return;
    for(unsigned i=0;i<std::min(budget,256u) && state==Running;++i) {
        commandAddress=pc;uint32_t raw;if(!read(2,raw))return;
        blockedOpcode=raw&0x7fff;
        if(blockedOpcode>=24 && blockedOpcode<=75) {
            int32_t a,b;if(!parameter(a) || !parameter(b))return;
            const unsigned op=blockedOpcode;
            const bool floating=(op>=32 && op<=39) || (op>=48 && op<=55) || op>=66;
            float fa,fb;std::memcpy(&fa,&a,4);std::memcpy(&fb,&b,4);
            bool result=op<40 ? (floating ? fa>fb : a>b) : op<56 ? (floating ? fa>=fb : a>=b) :
                ((op>=61 && op<=65) || op>=71) ? (floating ? fa!=fb : a!=b) : (floating ? fa==fb : a==b);
            if(raw&0x8000)result=!result;
            if(!andOr)condition=result;
            else if(andOr<=8) {condition=condition && result;--andOr;}
            else if(andOr>=21 && andOr<=28) {condition=condition || result;andOr=andOr==21 ? 0 : andOr-1;}
            continue;
        }
        if(raw&0x8000) {state=Unsupported;return;}
        if(blockedOpcode>=8 && blockedOpcode<=23) {
            uint32_t destination=pc;int32_t a,b;if(!parameter(a) || !parameter(b))return;
            unsigned op=(blockedOpcode-8)/4;
            if(blockedOpcode&1) {
                float fa,fb;std::memcpy(&fa,&a,4);std::memcpy(&fb,&b,4);
                if(op==3 && fb==0) {state=Invalid;return;}
                float result=op==0 ? fa+fb : op==1 ? fa-fb : op==2 ? fa*fb : fa/fb;
                std::memcpy(&a,&result,4);
            } else {
                if(op==3 && !b) {state=Invalid;return;}
                a=op==0 ? int32_t(uint32_t(a)+uint32_t(b)) : op==1 ? int32_t(uint32_t(a)-uint32_t(b)) :
                    op==2 ? int32_t(uint32_t(a)*uint32_t(b)) : int32_t(int64_t(a)/b);
            }
            uint32_t after=pc;pc=destination;if(!store(a))return;pc=after;continue;
        }
        int32_t value;
        switch(blockedOpcode) {
        case COMMAND_NOP:break;
        case COMMAND_GOTO:if(!parameter(value) || !jump(value))return;break;
        case COMMAND_GOTO_IF_FALSE:
            if(!parameter(value))return;
            if(!condition && !jump(value))return;
            break;
        case COMMAND_GOSUB:
            if(!parameter(value))return;
            if(stackSize==6) {state=Invalid;return;}
            stack[stackSize++]=pc;if(!jump(value))return;break;
        case COMMAND_RETURN:
            if(!stackSize) {state=Invalid;return;}
            pc=stack[--stackSize];break;
        case COMMAND_ANDOR:
            if(!parameter(value))return;
            if(!value) {andOr=0;condition=false;}
            else if(value>=1 && value<=8) {andOr=unsigned(value)+1;condition=true;}
            else if(value>=21 && value<=28) {andOr=unsigned(value)+1;condition=false;}
            else {state=Invalid;return;}
            break;
        case COMMAND_WAIT:
            if(!parameter(value))return;
            wake=now+uint32_t(std::max<int32_t>(0,value));state=Waiting;return;
        case COMMAND_SET_VAR_INT:case COMMAND_SET_VAR_FLOAT:
        case COMMAND_SET_LVAR_INT:case COMMAND_SET_LVAR_FLOAT:
        case COMMAND_SET_VAR_INT_TO_VAR_INT:case COMMAND_SET_LVAR_INT_TO_LVAR_INT:
        case COMMAND_SET_VAR_FLOAT_TO_VAR_FLOAT:case COMMAND_SET_LVAR_FLOAT_TO_LVAR_FLOAT:
        case COMMAND_SET_VAR_FLOAT_TO_LVAR_FLOAT:case COMMAND_SET_LVAR_FLOAT_TO_VAR_FLOAT:
        case COMMAND_SET_VAR_INT_TO_LVAR_INT:case COMMAND_SET_LVAR_INT_TO_VAR_INT: {
            uint32_t destination=pc;int32_t ignored;
            if(!parameter(ignored) || !parameter(value))return;
            uint32_t after=pc;pc=destination;if(!store(value))return;pc=after;break;
        }
        case COMMAND_SCRIPT_NAME:
            for(int j=0;j<8;++j) {uint32_t ch;if(!read(1,ch))return;scriptName[j]=char(ch);}break;
        case COMMAND_TERMINATE_THIS_SCRIPT:state=Finished;return;
        default:state=Unsupported;return;
        }
    }
}
}
