#pragma once
namespace slice {
class Frontend {
public:
    enum Result { NewGame, Resume, Map, Error };
    static Result run(bool paused);
    static bool movies();
    static bool story();
};
}
