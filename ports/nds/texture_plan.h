#pragma once
#include <algorithm>
namespace slice {
struct TextureRequest {unsigned id=0;int distance=0;bool coverage=false;};
class TexturePlan {
public:
    static constexpr unsigned Capacity=64;
    void offer(unsigned id,int distance,bool coverage){
        if(!id)return;
        // Near geometry always precedes the coverage shell, independently of
        // submission order. Duplicate material IDs consume one request slot.
        const int priority=distance+(coverage ? 1000000 : 0);
        // World faces arrive in material order. The common case needs one
        // comparison, rather than scanning all 64 requests for every face.
        if(last<count_ && requests[last].id==id){if(priority<score(requests[last]))requests[last]={id,distance,coverage};return;}
        unsigned worst=0;
        for(unsigned i=0;i<count_;++i){
            if(requests[i].id==id){last=i;if(priority<score(requests[i]))requests[i]={id,distance,coverage};return;}
            if(score(requests[i])>score(requests[worst]))worst=i;
        }
        if(count_<Capacity){last=count_;requests[count_++]={id,distance,coverage};}
        else if(priority<score(requests[worst])){last=worst;requests[worst]={id,distance,coverage};}
        else last=Capacity;
    }
    void sort(){std::sort(requests,requests+count_,[](const TextureRequest& a,const TextureRequest& b){return score(a)<score(b);});}
    unsigned count() const{return count_;}
    const TextureRequest& operator[](unsigned i) const{return requests[i];}
    static int uv(int value,unsigned scale){return scale==2 ? value>>1 : scale==0 ? value*2 : value;}
private:
    static int score(const TextureRequest& r){return r.distance+(r.coverage ? 1000000 : 0);}
    TextureRequest requests[Capacity]{};unsigned count_=0,last=Capacity;
};
}
