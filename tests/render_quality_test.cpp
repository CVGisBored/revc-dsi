#include "texture_plan.h"
#include "textures.h"
#include "lighting.h"
#include <cassert>
#include <string>
#include <cstdio>
int main(int argc,char** argv){
    assert(argc==2);slice::TexturePlan plan;
    plan.offer(1,3,true);plan.offer(2,200,false);plan.offer(1,500,false);plan.offer(1,700,false);plan.offer(0,0,false);plan.sort();
    assert(plan.count()==2 && plan[0].id==2 && plan[1].id==1 && !plan[1].coverage && plan[1].distance==500);
    for(unsigned id=3;id<100;++id)plan.offer(id,1000+id,true);plan.offer(100,1,false);plan.sort();assert(plan.count()==64 && plan[0].id==100);
    assert(slice::TexturePlan::uv(1024,0)==2048 && slice::TexturePlan::uv(1024,1)==1024 && slice::TexturePlan::uv(1024,2)==512);
    assert(slice::TexturePlan::uv(-1024,0)==-2048 && slice::TexturePlan::uv(-1024,2)==-512);
    std::string root=argv[1];slice::DetailTextureFile file;assert(file.open((root+"/world/detail.vct").c_str()));
    assert(!file.key(0) && !file.key(40000));slice::DetailTexturePage page;assert(!file.load(0,page));unsigned loaded=0;
    for(unsigned id=1;id<32768 && loaded<20;++id)if(file.key(id)){
        assert(file.load(id,page));assert(!(page.flags&~1u));for(auto c:page.palette)assert(c<32768);++loaded;
    }
    assert(loaded==20);file.close();assert(!file.key(1));
    slice::Lighting light;assert(light.open((root+"/world/lighting.vcl").c_str()));
    assert(light.surface(32767)==32767);unsigned grey=16|(16<<5)|(16<<10),value=light.surface(grey);
    assert((value&31)>16 && (value&31)<31);assert(((value>>10)&31)>16 && ((value>>10)&31)<31);
    for(unsigned channel=0;channel<3;++channel)for(unsigned v=0;v<31;++v){unsigned a=(light.surface(v<<(channel*5))>>(channel*5))&31,b=(light.surface((v+1)<<(channel*5))>>(channel*5))&31;assert(a<=b);}
    std::puts("Render quality: near-material priorities, bounded requests, UV resolution scaling, original 128px pages and contrast-preserving lighting passed");
}
