#include "distant.h"
#include "packed.h"
#include "view.h"
#include <algorithm>
#include <cstring>
#include <cmath>
#include <new>
#ifdef ARM9
#include <nds.h>
#endif
namespace slice {
bool DistantWorld::open(const char* path) {
    static_assert(sizeof(Index)==28,"Coverage object index ABI");
    if(file)std::fclose(file);file=std::fopen(path,"rb");loaded=pending=false;count=groupCount=pendingCount=pendingGroupCount=objects=0;viewRevision=~0u;revision=reused=committed=0;index.reset();
    if(!file)return false;
    char magic[4];bool ok=std::fread(magic,1,4,file)==4 && !std::memcmp(magic,"VFO1",4) && std::fread(&objects,4,1,file)==1 && objects>0 && objects<=8192;
    ok=ok && std::fread(&packedSize,4,1,file)==1 && std::fread(&rawSize,4,1,file)==1 && packedSize>0 && packedSize<=65536 && rawSize>0 && rawSize<=Capacity*34;
    if(ok){packed.reset(new(std::nothrow) unsigned char[packedSize]);raw.reset(new(std::nothrow) unsigned char[rawSize]);index.reset(new(std::nothrow) Index[objects]);ok=packed && raw && index;}
    ok=ok && std::fread(index.get(),sizeof(Index),objects,file)==objects;
    uint64_t end=16+objects*sizeof(Index);
    for(unsigned i=0;ok && i<objects;++i) {
        const auto& e=index[i];ok=e.faces>0 && e.faces<=Capacity && unsigned(e.faces)*34<=rawSize && e.size>0 && e.size<=packedSize && e.offset==end;
        for(int k=0;k<3;++k)ok=ok && e.low[k]<=e.high[k];end+=e.size;
    }
    ok=ok && !std::fseek(file,0,SEEK_END) && std::ftell(file)==long(end);
    if(!ok){std::fclose(file);file=nullptr;objects=0;index.reset();}return ok;
}
void DistantWorld::select(Point global) {
    auto compare=[](const Candidate& a,const Candidate& b){return a.score==b.score ? a.id<b.id : a.score<b.score;};
    unsigned n=0;
    const int actor[3]={int(global.x*64),int(global.y*64),int(global.z*64)};
    for(unsigned i=0;i<objects;++i) {
        const auto& e=index[i];uint32_t score=0;bool fits=true;
        for(int k=0;k<3;++k) {
            int low=e.anchor[k]*4096+e.low[k],high=e.anchor[k]*4096+e.high[k];
            int d=actor[k]-std::max(low,std::min(high,actor[k]));
            if(std::abs(d)>96*64){fits=false;break;}
            int base=int((k==0 ? origin.x : k==1 ? origin.y : origin.z)*64);
            if(low-base<-32768 || high-base>32767){fits=false;break;}
            score+=d*d/(k==1 ? 4 : 1);
        }
        if(!fits || score>96*96*4096)continue;
        Candidate c={uint16_t(i),score};
        if(n<MaxObjects){selected[n++]=c;std::push_heap(selected,selected+n,compare);}
        else if(compare(c,selected[0])){std::pop_heap(selected,selected+n,compare);selected[n-1]=c;std::push_heap(selected,selected+n,compare);}
    }
    std::sort(selected,selected+n,compare);
    selections=0;unsigned faces=0;
    for(unsigned i=0;i<n;++i)if(faces+index[selected[i].id].faces<=Capacity){faces+=index[selected[i].id].faces;selected[selections++]=selected[i];}
    cursor=packedRead=decoded=record=pendingCount=pendingGroupCount=0;pending=true;request=global;
}
bool DistantWorld::step() {
    if(cursor==selections) {
        count=pendingCount;groupCount=pendingGroupCount;
        std::copy_n(staging,count,resident);std::copy_n(pendingGroups,groupCount,groups);
        pendingCount=pendingGroupCount=0;pending=false;loaded=true;last=request;++revision;++committed;return true;
    }
    const auto& e=index[selected[cursor].id];
    if(!decoded && !packedRead)for(unsigned i=0;i<groupCount;++i)if(groups[i].id==selected[cursor].id){
        if(pendingCount+groups[i].count>Capacity || pendingGroupCount==MaxObjects)return false;
        auto group=groups[i];std::copy_n(resident+group.first,group.count,staging+pendingCount);
        group.first=pendingCount;pendingGroups[pendingGroupCount++]=group;pendingCount+=group.count;++cursor;++reused;return true;
    }
    if(!decoded) {
        unsigned amount=std::min(2048u,unsigned(e.size)-packedRead);
        if(std::fseek(file,e.offset+packedRead,SEEK_SET) || std::fread(packed.get()+packedRead,1,amount,file)!=amount)return false;
        bytes+=amount;packedRead+=amount;
        if(packedRead<e.size)return true;
        decoded=unpackLz10(packed.get(),e.size,raw.get(),rawSize);
        if(decoded!=unsigned(e.faces)*34 || pendingGroupCount==MaxObjects)return false;
        auto& group=pendingGroups[pendingGroupCount++];group.first=pendingCount;group.count=e.faces;group.id=selected[cursor].id;
        const int base[3]={int(e.anchor[0]*4096-origin.x*64),int(e.anchor[1]*4096-origin.y*64),int(e.anchor[2]*4096-origin.z*64)};
        for(int k=0;k<3;++k){group.low[k]=e.low[k]+base[k];group.high[k]=e.high[k]+base[k];}
        record=0;return true;
    }
    const int base[3]={int(e.anchor[0]*4096-origin.x*64),int(e.anchor[1]*4096-origin.y*64),int(e.anchor[2]*4096-origin.z*64)};
    unsigned end=std::min(unsigned(e.faces),record+128);
    for(;record<end;++record) {
        const auto* r=raw.get()+record*34;Entry entry{};
        std::memcpy(&entry.triangle.color,r,2);std::memcpy(&entry.material.texture,r+2,2);
        std::memcpy(entry.triangle.vertex,r+4,18);std::memcpy(entry.material.uv,r+22,12);
        if(entry.material.texture>=32768 || pendingCount==Capacity)return false;
        for(auto& v:entry.triangle.vertex)for(int k=0;k<3;++k) {
            if(v[k]<e.low[k] || v[k]>e.high[k])return false;
            int value=v[k]+base[k];if(value<-32768 || value>32767)return false;v[k]=value;
        }
        staging[pendingCount++]=entry;
    }
    if(record==e.faces){++cursor;packedRead=decoded=record=0;}
    return true;
}
bool DistantWorld::update(Point global,Point anchor,bool incremental,bool allowWork) {
    bytes=reused=0;if(!file || !std::isfinite(global.x) || !std::isfinite(global.y) || !std::isfinite(global.z))return false;
    if(origin.x!=anchor.x || origin.y!=anchor.y || origin.z!=anchor.z) {
        const int shift[3]={int((origin.x-anchor.x)*64),int((origin.y-anchor.y)*64),int((origin.z-anchor.z)*64)};
        bool safe=true;
        for(unsigned i=0;i<groupCount;++i)for(int k=0;k<3;++k)if(int(groups[i].low[k])+shift[k]<-32768 || int(groups[i].high[k])+shift[k]>32767)safe=false;
        for(unsigned i=0;i<pendingGroupCount;++i)for(int k=0;k<3;++k)if(int(pendingGroups[i].low[k])+shift[k]<-32768 || int(pendingGroups[i].high[k])+shift[k]>32767)safe=false;
        if(pending)for(unsigned j=cursor;j<selections;++j) {
            const auto& e=index[selected[j].id];const int anchorValue[3]={int(anchor.x*64),int(anchor.y*64),int(anchor.z*64)};
            for(int k=0;k<3;++k)if(e.anchor[k]*4096+e.low[k]-anchorValue[k]<-32768 || e.anchor[k]*4096+e.high[k]-anchorValue[k]>32767)safe=false;
        }
        if(!safe){count=groupCount=pendingCount=pendingGroupCount=0;loaded=pending=false;}
        for(unsigned i=0;i<count;++i)for(auto& v:resident[i].triangle.vertex)for(int k=0;k<3;++k)v[k]+=shift[k];
        for(unsigned i=0;i<pendingCount;++i)for(auto& v:staging[i].triangle.vertex)for(int k=0;k<3;++k)v[k]+=shift[k];
        for(unsigned i=0;i<groupCount;++i)for(int k=0;k<3;++k){groups[i].low[k]+=shift[k];groups[i].high[k]+=shift[k];}
        for(unsigned i=0;i<pendingGroupCount;++i)for(int k=0;k<3;++k){pendingGroups[i].low[k]+=shift[k];pendingGroups[i].high[k]+=shift[k];}
        origin=anchor;++revision;
    }
    if(!allowWork && loaded)return true;
    if(!pending) {
        float dx=global.x-last.x,dz=global.z-last.z;
        if(loaded && dx*dx+dz*dz<64)return true;
        select(global);
    }
#ifdef ARM9
    const uint64_t started=tickGetCount();
#endif
    unsigned units=0;
    while(pending) {
        if(!step())return false;
        if(incremental && loaded) {
            if(++units>=8 || bytes>=4096)break;
#ifdef ARM9
            if(tickGetCount()-started>=TICK_FREQ*2/1000)break;
#endif
        }
    }
    return true;
}
int DistantWorld::visible(Point camera,float si,float co,float ps,float pc,Point focus,int budget) {
    budget=std::max(0,std::min(Budget,budget));if(!budget){visibleGroups=cachedCount=0;viewRevision=~0u;return 0;}
    const ViewFrustum view(camera,si,co,ps,pc);
    const int key[11]={ViewFrustum::bin(view.x),ViewFrustum::bin(view.y),ViewFrustum::bin(view.z),view.si,view.co,view.sp,view.cp,int(std::floor(focus.x*2)),int(std::floor(focus.y*2)),int(std::floor(focus.z*2)),budget};
    if(viewRevision==revision && !std::memcmp(key,viewKey,sizeof(key)))return cachedCount;
    std::memcpy(viewKey,key,sizeof(key));viewRevision=revision;
    Candidate candidates[MaxObjects];unsigned n=0;
    uint16_t costs[MaxObjects]{};bool faceVisible[Capacity]{};
    const int actor[3]={int(focus.x*64),int(focus.y*64),int(focus.z*64)};
    for(unsigned i=0;i<groupCount;++i) {
        const auto& g=groups[i];int depth;unsigned distance=0;
        for(int k=0;k<3;++k){int d=actor[k]-std::max(int(g.low[k]),std::min(int(g.high[k]),actor[k]));distance+=d*d/(k==1 ? 4 : 1);}
        // A complete LOD object is also the close-range coverage layer. There
        // is deliberately no near-distance exclusion or per-triangle truncation.
        if(distance>64*64*4096 || !view.bounds(g.low,g.high,depth))continue;
        for(unsigned j=0;j<g.count;++j)if(view.triangle(resident[g.first+j].triangle)){faceVisible[g.first+j]=true;++costs[i];}
        if(costs[i])candidates[n++]={uint16_t(i),distance};
    }
    std::sort(candidates,candidates+n,[](const Candidate& a,const Candidate& b){return a.score==b.score ? a.id<b.id : a.score<b.score;});
    int faces=0;visibleGroups=0;
    for(unsigned i=0;i<n;++i) {
        const auto& g=groups[candidates[i].id];
        const unsigned cost=costs[candidates[i].id];
        if(faces+int(cost)>budget)continue;
        drawGroups[visibleGroups]=candidates[i].id;drawCosts[visibleGroups++]=cost;
        for(unsigned face=0;face<g.count;++face)if(faceVisible[g.first+face])list[faces++]=g.first+face;
    }
    std::sort(list,list+faces,[this](unsigned a,unsigned b){return resident[a].material.texture<resident[b].material.texture;});
    return cachedCount=faces;
}
}
