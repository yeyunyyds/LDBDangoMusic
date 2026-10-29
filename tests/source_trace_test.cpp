#include "source_trace_contract.hpp"
#include <iostream>
#include <stdexcept>
using namespace dango;
void check(bool v){if(!v)throw std::runtime_error("Trace/wall support regression");}
// Synthetic wall x=100; optional holes and intervening planes allow deterministic
// support and whole-panel occlusion tests without invoking a game DLL.
struct World {bool hole=false,block=false;
 bool operator()(Vec3 start,Vec3 end,trace::Result& r)const{r={};r.start=start;r.end=end;float plane=block&&start.x<50?50.f:100.f;float dx=end.x-start.x;if(std::abs(dx)<.001)return true;float f=(plane-start.x)/dx;
 if(f<0||f>1)return true;auto p=start+(end-start)*f;if(hole&&std::abs(p.y)<3&&std::abs(p.z)<3)return true;
 r.fraction=f;r.end=p;r.plane.normal={-1,0,0};r.plane.distance=-plane;r.contents=1;return true;}
};
int main(){
 auto ray=trace::ray({1,2,3},{11,22,33});check(reinterpret_cast<uintptr_t>(&ray)%16==0&&ray.start.x==1&&ray.delta.z==30&&ray.isRay==1&&ray.isSwept==1&&ray.transform==0);
 check(trace::ray({},{ }).isSwept==0);trace::GuardedResult guarded;check(guarded.intact());guarded.after[0]=0;check(!guarded.intact());
 trace::Result hit;World world;check(world({0,0,0},{200,0,0},hit)&&trace::valid(hit,{},{200,0,0})&&trace::wallHit(hit));hit.end.x=80;check(!trace::valid(hit,{},{200,0,0}));hit.end.x=100;hit.plane.normal={};check(!trace::valid(hit,{},{200,0,0}));hit.plane.normal={-1,0,0};hit.surface.flags=4;check(!trace::wallHit(hit));hit.surface.flags=0;hit.contents=2;check(!trace::wallHit(hit));hit.contents=1;hit.startSolid=1;check(!trace::wallHit(hit));
 wall::Panel panel;check(wall::makePanel({100,0,0},{-1,0,0},40,20,3,panel));check(trace::supported(panel,world));check(!trace::supported(panel,World{true,false}));check(trace::visible(panel,{0,0,0},world));check(!trace::visible(panel,{0,0,0},World{false,true}));check(!trace::visible(panel,{110,0,0},world));
 // API failures always reject placement/visibility rather than rendering through walls.
 auto failure=[](Vec3,Vec3,trace::Result&){return false;};check(!trace::supported(panel,failure)&&!trace::visible(panel,{},failure));
 std::cout<<"Passed: Ray/trace wire layouts and alignment, result canaries/validation, wall support holes, conservative occlusion and failed-call rejection\n";
}
