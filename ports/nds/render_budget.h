#pragma once
#include <algorithm>
namespace slice {
struct RenderBudget {
    static constexpr int Near=512,Coverage=256,Reserve=64;
    static bool admits(int scene,int plannedActors,int cost){return plannedActors+cost+Near+Coverage+Reserve<=scene;}
    static int nearby(int scene,int actors,int requested){return std::min(std::max(int(Near),requested),scene-actors-Reserve-Coverage);}
    static int distant(int scene,int actors,int near){return std::max(0,scene-actors-Reserve-near);}
    static int slower(int current){return std::max(int(Near),current-32);}
};
}
