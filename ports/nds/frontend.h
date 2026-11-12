#pragma once
namespace slice {
class Frontend {
public:
    enum Result { NewGame, Resume, Map, Mission, Error };
    static Result run(bool paused);
    static unsigned selectedMission;
    static bool movies();
    static bool story();
};
}
