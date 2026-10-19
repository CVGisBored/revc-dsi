#pragma once
#include "vehicle_animation.h"
#include <memory>
namespace slice {
class VehicleModels {
public:
    static constexpr int Count=4;
    struct Mesh {int count=0;std::unique_ptr<RevcTriangle[]> triangles;std::unique_ptr<RevcUV[]> uv;VehicleAnimation::Rig rig;};
    struct Model {Mesh detail,far;Point seat{};float radius=.35f,wheelbase=3.3f;};
    bool open(const char* root);
    const Model& model(int id) const {return models[id>=0 && id<Count ? id : 0];}
    static const char* name(int id);
private:
    Model models[Count];
};
}
