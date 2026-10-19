#pragma once
namespace slice {
class ActorTexture {
public:
    bool open(const char* path);
    void bind() const;
private:
    int handle=0;
};
}
