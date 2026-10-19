#include "vehicle_models.h"
#include "fleet_data.h"
#include <cstdio>
#include <cstring>
#include <new>
namespace slice {
const char* VehicleModels::name(int id){static const char* names[]={"sentinel","taxi","admiral","spand"};return names[id>=0 && id<Count ? id : 0];}
bool VehicleModels::open(const char* root) {
    for(int id=0;id<Count;++id) {
        auto& model=models[id];model.seat=fleetData::models[id].seat;model.radius=fleetData::models[id].radius;model.wheelbase=fleetData::models[id].wheelbase;
        for(int level=0;level<2;++level) {
            auto& mesh=level ? model.far : model.detail;char path[512],base[512],magic[4];uint32_t n=0;
            std::snprintf(base,sizeof(base),"%s/world/fleet-%s%s",root,name(id),level ? "-low" : "");
            std::snprintf(path,sizeof(path),"%s.vcm",base);FILE* f=std::fopen(path,"rb");if(!f)return false;
            bool ok=std::fread(magic,4,1,f)==1 && !std::memcmp(magic,"VCM1",4) && std::fread(&n,4,1,f)==1 && n>0 && n<=unsigned(level ? 160 : 640);
            if(ok){mesh.triangles.reset(new(std::nothrow) RevcTriangle[n]);mesh.uv.reset(new(std::nothrow) RevcUV[n]);ok=mesh.triangles && mesh.uv;}
            ok=ok && std::fread(mesh.triangles.get(),sizeof(RevcTriangle),n,f)==n && std::fgetc(f)==EOF;std::fclose(f);if(!ok)return false;mesh.count=n;
            std::snprintf(path,sizeof(path),"%s.vcu",base);f=std::fopen(path,"rb");if(!f)return false;
            ok=std::fread(magic,4,1,f)==1 && !std::memcmp(magic,"VCU1",4) && std::fread(&n,4,1,f)==1 && n==unsigned(mesh.count) && std::fread(mesh.uv.get(),sizeof(RevcUV),n,f)==n && std::fgetc(f)==EOF;std::fclose(f);if(!ok)return false;
            std::snprintf(path,sizeof(path),"%s.vpa",base);if(!VehicleAnimation::loadRig(path,mesh.count,mesh.rig))return false;
        }
    }
    return true;
}
}
