#pragma once
namespace slice {
struct Projection {
    static constexpr float Near=.25f,Far=80.f;
    static float depth(float distance){return Far/(Far-Near)*(1-Near/distance);}
    static float distance(float depth){return Far*Near/(Far-depth*(Far-Near));}
};
}
