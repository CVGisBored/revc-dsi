#include "animation.h"
#include "scripts.h"
#include "textures.h"
#include "water.h"
#include "lighting.h"
#include "ScriptCommands.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>
#include <set>
#include <vector>
#include <unistd.h>

int main(int argc,char** argv) {
    assert(argc==2);std::string root=argv[1];
    slice::Lighting lighting;assert(lighting.open((root+"/world/lighting.vcl").c_str()));
    assert(lighting.surface(0)==(15|(15<<5)|(10<<10)));
    assert(lighting.surface(32767)==32767 && lighting.waterAlpha()==24);
    slice::Water water;assert(water.open((root+"/world/water.vcw").c_str()));
    float height=0;assert(!water.cell(-1,0,height) && !water.cell(128,0,height));
    int waterCells=0;
    for(int x=0;x<128;++x)for(int y=0;y<128;++y)if(water.cell(x,y,height)) {
        assert(height==6.f);++waterCells;
    }
    assert(waterCells==11669 && water.texture()>0);
    slice::Animation actor;
    assert(actor.open((root+"/world/player.vca").c_str(),520));assert(actor.count()>=500 && actor.count()<=520);
    assert(actor.speed(slice::Animation::Walk)>.7f && actor.speed(slice::Animation::Walk)<.9f);
    assert(actor.speed(slice::Animation::Run)>6 && actor.speed(slice::Animation::Run)<7);
    std::set<unsigned> poses;
    for(int clip=0;clip<slice::Animation::ClipCount;++clip) {
        actor.advance(static_cast<slice::Animation::Clip>(clip),0,true);
        for(int tick=0;tick<180;++tick) {
            const auto* mesh=actor.mesh();unsigned hash=2166136261u;
            int lowY=32767,highY=-32768;
            for(int i=0;i<actor.count();++i)for(const auto& vertex:mesh[i].vertex)for(int v:vertex) {
                assert(v>-320 && v<320);hash=(hash^unsigned(v))*16777619u;
            }
            for(int i=0;i<actor.count();++i)for(const auto& vertex:mesh[i].vertex) {
                assert(vertex[1]>=0); // Walking/running poses must not bury the soles.
                lowY=std::min(lowY,int(vertex[1]));highY=std::max(highY,int(vertex[1]));
            }
            if(clip==slice::Animation::Fire || clip==slice::Animation::Reload)
                assert(highY-lowY>90); // Partial weapon clips retain an upright idle root.
            poses.insert(hash);actor.advance(static_cast<slice::Animation::Clip>(clip),1);
        }
    }
    assert(poses.size()>=35 && actor.uv()!=nullptr);
    // Fractional gait timing must agree across steady and streaming-stalled frames.
    actor.advance(slice::Animation::Run,0,true);
    for(int i=0;i<20;++i)actor.advance(slice::Animation::Run,1,false,.435f);
    unsigned steady=actor.frame();actor.advance(slice::Animation::Run,0,true);
    actor.advance(slice::Animation::Run,20,false,.435f);assert(actor.frame()==steady);
    actor.advance(slice::Animation::Run,0,true);actor.advance(slice::Animation::Run,20,false,1.f);
    assert(actor.frame()>steady);
    // Pose blending must stay between source poses without changing topology.
    std::vector<RevcTriangle> a(actor.poseMesh(slice::Animation::Run,.25f,slice::Animation::Run,.25f,0),actor.poseMesh(slice::Animation::Run,.25f,slice::Animation::Run,.25f,0)+actor.count());
    auto* b=actor.poseMesh(slice::Animation::Sprint,.25f,slice::Animation::Sprint,.25f,0);
    std::vector<RevcTriangle> sprintPose(b,b+actor.count());
    const auto* mixed=actor.poseMesh(slice::Animation::Run,.25f,slice::Animation::Sprint,.25f,.5f);
    for(int i=0;i<actor.count();++i)for(int j=0;j<3;++j)for(int axis=0;axis<3;++axis)
        assert(std::abs(2*int(mixed[i].vertex[j][axis])-int(a[i].vertex[j][axis])-int(sprintPose[i].vertex[j][axis]))<=1);
    actor.advance(slice::Animation::Walk,0,true);auto frame=actor.frame();actor.advance(slice::Animation::Walk,0);assert(actor.frame()==frame);
    actor.advance(slice::Animation::Fire,0,true);for(int i=0;i<100;++i)actor.advance(slice::Animation::Fire,4);assert(actor.frame()==7);
    assert(actor.open((root+"/world/player3.vca").c_str(),520));assert(actor.uv() && actor.meshAt(slice::Animation::Sprint,10) && actor.meshAt(slice::Animation::Punch,10));
    assert(actor.open((root+"/world/guard.vca").c_str(),120));assert(actor.count()>0 && actor.count()<=120);
    assert(!actor.open((root+"/world/player.vca").c_str(),120));assert(actor.mesh()==nullptr);
    slice::TextureFile textures;assert(textures.open((root+"/world/textures.vct").c_str()));
    assert(textures.count()>1000 && textures.loadAverages((root+"/world/averages.vcc").c_str()));assert(textures.average(1)<32768);slice::TexturePage page;
    assert(!textures.load(0,page));assert(!textures.load(textures.count()+1,page));
    for(unsigned id=1;id<=textures.count();id+=101) {
        assert(textures.load(id,page));for(auto color:page.palette)assert(color<32768);
    }

    slice::Scripts scripts;assert(scripts.open((root+"/scripts/main.scm").c_str()));assert(scripts.missionCount()==97);
    scripts.step(0);assert(scripts.status()==slice::Scripts::Unsupported);
    assert(scripts.opcode()==COMMAND_DO_FADE);assert(scripts.address()==39662);assert(std::strcmp(scripts.name(),"MAIN")==0);
    // Unsupported commands never advance: no silent skips or pretend mission success.
    unsigned pc=scripts.address();scripts.step(10000);assert(scripts.address()==pc);
    std::set<unsigned> blockers;
    for(int mission=0;mission<int(scripts.missionCount());++mission) {
        assert(scripts.start(mission));
        for(unsigned tick=0;tick<120 && (scripts.status()==slice::Scripts::Running || scripts.status()==slice::Scripts::Waiting);++tick)scripts.step(tick*100);
        assert(scripts.status()==slice::Scripts::Unsupported);
        if(scripts.status()==slice::Scripts::Unsupported)blockers.insert(scripts.opcode());
    }
    assert(!scripts.start(97));assert(!scripts.start(-2));
    std::printf("Original animation: %zu distinct poses; SCM: 97 bounded mission starts, %zu engine blockers\n",poses.size(),blockers.size());
    for(unsigned opcode:blockers)std::printf("SCM missing engine binding: %04X\n",opcode);
    // Exercise parameter decoding, yielding, bytecode bounds and loop budgets.
    FILE* original=std::fopen((root+"/scripts/main.scm").c_str(),"rb");assert(original);
    std::fseek(original,0,SEEK_END);std::vector<unsigned char> bytes(std::ftell(original));std::rewind(original);
    assert(std::fread(bytes.data(),1,bytes.size(),original)==bytes.size());std::fclose(original);
    char path[]="/tmp/revc-scm-test-XXXXXX";int fd=mkstemp(path);assert(fd>=0);
    FILE* modified=fdopen(fd,"wb");assert(modified);
    const unsigned char code[]={6,0,3,0,0,4,50, 1,0,3,0,0, 78,0};
    static_assert(COMMAND_TERMINATE_THIS_SCRIPT==78,"SCM opcode ABI");
    std::memcpy(bytes.data()+39652,code,sizeof(code));assert(std::fwrite(bytes.data(),1,bytes.size(),modified)==bytes.size());std::fclose(modified);
    slice::Scripts fixture;assert(fixture.open(path));fixture.step(0);assert(fixture.status()==slice::Scripts::Waiting && fixture.local(0)==50);
    fixture.step(49);assert(fixture.status()==slice::Scripts::Waiting);fixture.step(50);assert(fixture.status()==slice::Scripts::Finished);
    assert(fixture.start());fixture.step(0,4);assert(fixture.status()==slice::Scripts::Running && fixture.local(0)==50);
    std::vector<unsigned char> program;
    auto opcode=[&](unsigned v){program.push_back(v&255);program.push_back(v>>8);};
    auto local=[&](unsigned i){program.push_back(3);program.push_back(i);program.push_back(0);};
    auto small=[&](int i){program.push_back(4);program.push_back(static_cast<unsigned char>(i));};
    opcode(COMMAND_SET_LVAR_INT);local(0);small(-5);
    opcode(COMMAND_ADD_VAL_TO_INT_LVAR);local(0);small(10);
    opcode(COMMAND_IS_INT_LVAR_GREATER_THAN_NUMBER|0x8000);local(0);small(4);
    opcode(COMMAND_GOTO_IF_FALSE);program.push_back(1);unsigned label=program.size();
    for(int i=0;i<4;++i)program.push_back(0);
    opcode(COMMAND_SET_LVAR_INT);local(0);small(99);
    unsigned end=39652+program.size();opcode(COMMAND_TERMINATE_THIS_SCRIPT);
    for(int i=0;i<4;++i)program[label+i]=(end>>(8*i))&255;
    auto replace=[&]() {
        std::memcpy(bytes.data()+39652,program.data(),program.size());FILE* f=std::fopen(path,"wb");assert(f);
        assert(std::fwrite(bytes.data(),1,bytes.size(),f)==bytes.size());std::fclose(f);assert(fixture.open(path));
    };
    replace();fixture.step(0);assert(fixture.status()==slice::Scripts::Finished && fixture.local(0)==5);
    program.clear();opcode(COMMAND_GOTO);program.push_back(1);
    for(int i=0;i<4;++i)program.push_back((39652>>(8*i))&255);
    replace();fixture.step(0,256);assert(fixture.status()==slice::Scripts::Running); // bounded infinite loop
    program.clear();opcode(COMMAND_RETURN);replace();fixture.step(0);assert(fixture.status()==slice::Scripts::Invalid);
    unlink(path);
}
