#include "cinematic_camera.hpp"
#include <cassert>
#include <vector>
using namespace dango;
int main(){
 cinematic::Director d;
 assert(d.start({{10,20,60},80,179,0}));
 auto clear=[](Vec3,Vec3){return true;};
 auto p=d.step(1000,6000,clear);
 assert(length(p.eye-Vec3{10,20,60})<.001f&&p.pitch==80);
 p=d.step(2600,6000,clear);assert(std::abs(p.pitch)<18.f);
 p=d.step(6999,6000,clear);auto q=d.step(7000,6000,clear);
 assert(length(q.eye-p.eye)<.01f&&std::abs(cinematic::wrap(q.yaw-p.yaw))<.01f);
 for(unsigned i=0;i<100;++i){p=d.step(7000+i*1000,6000,clear);assert(cinematic::valid(p)&&length(p.eye-Vec3{10,20,60})<50);}
 assert(d.start({{200,300,80},-70,-179,0})); // re-acquisition from actual new pose
 auto blocked=[](Vec3,Vec3){return false;};
 p=d.step(1,6000,blocked);q=d.step(3001,6000,blocked);
 assert(length(q.eye-p.eye)<.001f&&std::abs(q.pitch)<18.f);
 assert(std::abs(cinematic::mixAngle(179,-179,.5f))>179.f);
 struct Line{std::int64_t start;};std::vector<Line> lines{{0},{1000},{2000},{5500},{9000}};
 assert(cinematic::duration(lines,1000,1)==4500);
 assert(cinematic::duration(lines,1000,2)==4000);
 assert(cinematic::duration(lines,-1,1)==6000);
}
