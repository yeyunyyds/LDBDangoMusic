#include "peak_style.hpp"
#include <iostream>
#include <stdexcept>
void check(bool v){if(!v)throw std::runtime_error("Peak style regression");}
int main(){using namespace dango;
 Line l{1000,"你好 世界","",{{1000,400,"你好"},{2000,400," 世界"}}};peak::Animation a(l,4000);
 check(a.starts[0]==1000&&a.starts[1]==1000&&a.starts[3]==2000);
 auto before=a.sample(0,999,0,true,false);auto at=a.sample(0,1000,0,true,false);auto done=a.sample(0,1160,0,true,false);
 check(before.alpha==0&&at.alpha==0&&std::abs(at.scale-1.55f)<.0001&&done.alpha==1&&done.scale==1);
 auto split=peak::splitTimeline({l,Line{5000,"end","",{}}});check(split.size()==3&&split[0].text=="你好"&&split[1].text=="世界"&&split[1].start==2000);
 check(peak::splitTimeline({Line{0,"hello world","",{}}}).size()==1);
 auto first=a.sample(0,2000,1.01);auto same=a.sample(0,2000,1.02);check(first.rotation==same.rotation&&first.x==same.x);check(first.rotation!=a.sample(0,2000,1.13).rotation);
 peak::Animation fallback(Line{0,"你好世界 Hello","",{}},4000);check(fallback.starts[0]==fallback.starts[1]&&fallback.starts[2]>fallback.starts[1]);
 auto layout=peak::layout(a.glyphs,[](auto&){return 10.f;},20,14);check(layout.rows.size()==3&&layout.glyphs[0].x==-10);
 auto outgoing=peak::retire(.2);check(std::abs(outgoing.alpha-.5f)<.0001&&outgoing.lift>0&&!outgoing.done&&peak::retire(.4).done);
 std::vector<float> mask(49,0);mask[24]=1;auto halo=peak::blur(mask,7,7,1);check(halo[24]>0&&halo[23]>0&&halo[24]<1);for(auto n:halo)check(std::isfinite(n)&&n>=0&&n<=1);
 CameraPose pose{{0,0,0},{0,0,1},{1,0,0},{0,1,0}};check(peak::position(pose,200,30,0,5).x<0&&peak::position(pose,200,30,1,5).x>0);check(std::abs(length(peak::keepDistance({0,0,20},pose,200))-120)<.01);
 // Keep-away is persisted by callers: repeatedly approaching cannot pass
 // through the old anchor or make it snap back when the camera retreats.
 Vec3 retreating{0,0,200};CameraPose walk=pose;
 for(int i=0;i<=250;++i){walk.eye.z=float(i);retreating=peak::keepDistance(retreating,walk,200);check(length(retreating-walk.eye)>=119.99f&&retreating.z>walk.eye.z);}
 auto held=retreating;walk.eye.z-=50;retreating=peak::keepDistance(retreating,walk,200);check(length(retreating-held)<.001f);
 walk.eye={0,0,0};check(std::abs(peak::keepDistance(walk.eye,walk,200).z-120)<.001f);check(peak::keepDistance({0,0,-1},walk,200).z>0);
 std::cout<<"Passed: Peak word-aligned pop, eight-Hz deterministic wiggle, CJK pair fallback, wrapping, 400ms lifecycle, cached blur, side placement/keep-away\n";
}
