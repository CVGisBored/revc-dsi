#pragma once
namespace slice {
// Presentation only: the world renderer still submits the same scene.
class GameplayDisplay {
public:
    static constexpr int Width=128,Height=72,Scale=2,Top=24;
    static void init();
    static void suspend();
    static void resume();
    // Movies and menu artwork share capture bank D. Keep capture stopped for
    // the entire call, including early/error returns, then restore gameplay.
    template<class Function> static auto framebuffer(Function function)->decltype(function()) {
        suspend();
        auto result=function();
        resume();
        return result;
    }
};
}
