#include "cinematic_camera.hpp"
#include <cassert>
#include <vector>
using namespace dango;
using namespace dango::cinematic;
int main(){
 Tracker tracker;auto a=tracker.sample(1,100,1000,{0,0,40},0);
 auto b=tracker.sample(1,100,1100,{30,0,40},0);
 assert(b.velocity.x>100);
 auto c=tracker.sample(1,101,1200,{3000,0,40},0);assert(length(c.velocity)==0);
 Director d;assert(d.start({{0,0,64},80,179,0}));
 auto clear=[](Vec3,Vec3){return true;};
 Composition comp;
 assert(d.plan(1000,6000,a,Style::Orbit,comp,clear));
 auto p=d.step(1000,a);assert(length(p.eye-Vec3{0,0,64})<.001f&&p.pitch==80);
 p=d.step(7000,a);assert(length(p.eye-a.center)>300);
 auto next=a;next.center={500,0,40};assert(d.needsPlan(7000,next,Style::Wide));
 assert(d.plan(7000,6000,next,Style::Wide,comp,clear));
 auto q=d.step(7000,next);assert(length(q.eye-p.eye)<.01f);
 q=d.step(13000,next);assert(length(q.eye-next.center)>600);
 assert(d.plan(13000,6000,next,Style::Bird,comp,clear));
 q=d.step(19000,next);assert(q.eye.z>next.center.z+400);
 auto blocked=[](Vec3,Vec3){return false;};
 p=q;assert(!d.plan(19000,6000,next,Style::Orbit,comp,blocked));
 q=d.step(25000,next);assert(length(q.eye-p.eye)<.001f);
 assert(choose(Style::Mixed,300,0,false)==Style::Bird);
 struct L{int64_t start;};std::vector<L> lines{{0},{1000},{2000},{5500},{9000}};
 assert(duration(lines,1000,1)==4500);
 assert(duration(lines,-1,1)==6000);
}
