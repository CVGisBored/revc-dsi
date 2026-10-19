#include "animation.h"
#include <cstdio>
#include <cstring>
#include <algorithm>
#include <new>
#include <cmath>

namespace slice {
bool Animation::open(const char* path,int maximum) {
    data.reset();coordinates.reset();vertices.reset();posed.reset();faces.reset();vertexCount=0;cachedFrame=cachedClip=~0u;triangles=0;reset();
    FILE* f=std::fopen(path,"rb");if(!f)return false;
    unsigned char header[8]{};bool ok=std::fread(header,1,8,f)==8;
    auto u16=[](const unsigned char* p){return unsigned(p[0])|(unsigned(p[1])<<8);};
    unsigned count=u16(header+4),clips=u16(header+6),total=0;
    bool extended=std::memcmp(header,"VCA5",4)==0 || std::memcmp(header,"VCA6",4)==0;
    bool textured=extended || std::memcmp(header,"VCA4",4)==0;
    bool indexed=textured || std::memcmp(header,"VCA3",4)==0;
    bool motion=indexed || std::memcmp(header,"VCA2",4)==0;
    if(indexed) {
        unsigned char spec[4]{};ok=ok && std::fread(spec,1,4,f)==4;
        vertexCount=u16(spec);ok=ok && vertexCount>0 && vertexCount<=count*3 && !u16(spec+2);
    }
    ok=ok && (motion || std::memcmp(header,"VCA1",4)==0) && count>0 && count<=unsigned(maximum) && (clips==5 || (extended && (clips==12 || clips==ClipCount)));
    for(auto& item:sequence)item=Sequence{};
    for(unsigned i=0;ok && i<clips;++i) {
        unsigned char s[4]{};ok=std::fread(s,1,4,f)==4;if(!ok)break;
        sequence[i].frames=u16(s);sequence[i].duration=u16(s+2);sequence[i].offset=total;
        sequence[i].speed=0;if(motion)ok=std::fread(&sequence[i].speed,4,1,f)==1 && std::isfinite(sequence[i].speed) && sequence[i].speed>=0 && sequence[i].speed<50;
        ok=ok && sequence[i].frames>0 && sequence[i].frames<=32 && sequence[i].duration>0;
        total+=sequence[i].frames*(indexed ? vertexCount : count);
    }
    std::unique_ptr<RevcTriangle[]> loaded;
    if(ok) {loaded.reset(new(std::nothrow) RevcTriangle[indexed ? count : total]);ok=bool(loaded);}
    if(ok && indexed) {
        faces.reset(new(std::nothrow) Face[count]);vertices.reset(new(std::nothrow) Vertex[total]);
        posed.reset(new(std::nothrow) Vertex[vertexCount]);
        ok=faces && vertices && posed;
        if(textured) {
            coordinates.reset(new(std::nothrow) RevcUV[count]);ok=ok && bool(coordinates);
            for(unsigned i=0;ok && i<count;++i)ok=std::fread(&faces[i],sizeof(Face),1,f)==1 && std::fread(&coordinates[i],sizeof(RevcUV),1,f)==1;
        } else if(ok)ok=std::fread(faces.get(),sizeof(Face),count,f)==count;
        for(unsigned i=0;ok && i<count;++i)for(auto index:faces[i].index)if(index>=vertexCount)ok=false;
        if(ok)ok=std::fread(vertices.get(),sizeof(Vertex),total,f)==total;
    } else if(ok)ok=std::fread(loaded.get(),sizeof(RevcTriangle),total,f)==total;
    if(ok)ok=std::fgetc(f)==EOF;
    std::fclose(f);
    if(!ok)return false;
    data=std::move(loaded);triangles=count;return true;
}
void Animation::advance(Clip next,unsigned ticks,bool restart,float rate) {
    if(next<Idle || next>=ClipCount || !sequence[next].frames)return;
    if(next!=clip || restart) {clip=next;clock=0;fraction=0;}
    if(!std::isfinite(rate))rate=1;
    fraction+=std::min(ticks,30u)*std::max(0.f,std::min(rate,4.f));
    const unsigned whole=unsigned(fraction);fraction-=whole;clock+=whole;
    if(!triangles)return;
    if(clip!=Idle && clip!=Walk && clip!=Run && clip!=Sprint) clock=std::min<uint32_t>(clock,sequence[clip].duration-1);
    else clock%=sequence[clip].duration;
}
unsigned Animation::frame() const {
    if(!triangles)return 0;
    const auto& s=sequence[clip];return std::min<uint32_t>(s.frames-1,clock*s.frames/s.duration);
}
const RevcTriangle* Animation::mesh() const {
    if(!data)return nullptr;
    return decode(clip,frame());
}
const RevcTriangle* Animation::meshAt(Clip next,unsigned ticks) const {
    if(!data || next<Idle || next>=ClipCount || !sequence[next].frames)return nullptr;
    const auto& s=sequence[next];
    unsigned time=next!=Idle && next!=Walk && next!=Run && next!=Sprint ? std::min(ticks,s.duration-1) : ticks%s.duration;
    unsigned frame=std::min(s.frames-1,time*s.frames/s.duration);
    return decode(next,frame);
}
const RevcTriangle* Animation::poseMesh(Clip first,float phase,Clip second,float secondPhase,float blend,bool allowLoops) const {
    if(!data || !vertices || first<Idle || first>=ClipCount || !sequence[first].frames)return mesh();
    if(second<Idle || second>=ClipCount || !sequence[second].frames){second=first;secondPhase=phase;blend=0;}
    blend=std::max(0.f,std::min(1.f,blend));
    struct Sample {const Vertex* a;const Vertex* b;int mix;};
    auto sample=[&](Clip clip,float phase) {
        const auto& seq=sequence[clip];
        const bool loop=allowLoops && (clip==Idle || clip==Walk || clip==Run || clip==Sprint);
        phase=std::max(0.f,std::min(1.f,phase));
        float position=phase*(loop ? seq.frames : seq.frames-1);
        unsigned a=std::min(seq.frames-1,unsigned(position)),b=loop ? (a+1)%seq.frames : std::min(seq.frames-1,a+1);
        return Sample{vertices.get()+seq.offset+a*vertexCount,vertices.get()+seq.offset+b*vertexCount,int((position-a)*256)};
    };
    const auto a=sample(first,phase),b=sample(second,secondPhase);const int weight=int(blend*256);
    const int weights[4]={(256-a.mix)*(256-weight),a.mix*(256-weight),(256-b.mix)*weight,b.mix*weight};
    // Interpolate each shared vertex once using fixed-point weights. Floating
    // point work per triangle is prohibitively expensive on the DS ARM9.
    for(unsigned v=0;v<vertexCount;++v)for(int axis=0;axis<3;++axis) {
        int value=a.a[v].xyz[axis]*weights[0]+a.b[v].xyz[axis]*weights[1]+b.a[v].xyz[axis]*weights[2]+b.b[v].xyz[axis]*weights[3];
        posed[v].xyz[axis]=int16_t((value+32768)>>16);
    }
    for(int i=0;i<triangles;++i) {
        data[i].color=faces[i].color;
        for(int j=0;j<3;++j)for(int axis=0;axis<3;++axis)data[i].vertex[j][axis]=posed[faces[i].index[j]].xyz[axis];
    }
    cachedFrame=cachedClip=~0u;return data.get();
}

const RevcTriangle* Animation::decode(Clip selected,unsigned frame) const {
    if(!vertexCount)return data.get()+sequence[selected].offset+frame*triangles;
    if(cachedClip!=unsigned(selected) || cachedFrame!=frame) {
        const auto* pose=vertices.get()+sequence[selected].offset+frame*vertexCount;
        for(int i=0;i<triangles;++i) {
            data[i].color=faces[i].color;
            for(int v=0;v<3;++v)for(int axis=0;axis<3;++axis)data[i].vertex[v][axis]=pose[faces[i].index[v]].xyz[axis];
        }
        cachedClip=selected;cachedFrame=frame;
    }
    return data.get();
}

}
