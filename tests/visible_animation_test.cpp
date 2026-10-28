#include "peak_style.hpp"
#include <iostream>
#include <stdexcept>
using namespace dango;
void check(bool ok){if(!ok)throw std::runtime_error("Visible animation regression");}
int main(){
 Line line{1000,"AB","",{{1000,100,"A"},{3000,100,"B"}}};peak::Animation animation(line,4000);peak::VisibleAnimation motion(2);
 // At first actual draw the 500ms sync offset has already consumed the old pop.
 auto old=animation.sample(0,1500,10,true,false);check(old.scale==1);
 auto initial=motion.sample(animation,0,1500,10,1,true,false,8,.8f);check(initial.alpha>0&&initial.scale>1.3f&&initial.y<0);
 auto settled=motion.sample(animation,0,1750,10.25,1,true,false,8,.8f);check(settled.alpha==1&&settled.scale==1&&settled.y==0);
 // Song time frozen between network samples must not freeze visual animation.
 auto jiggle1=motion.sample(animation,0,1750,10.5,1,false,true,8,.8f),jiggle2=motion.sample(animation,0,1750,10.75,1,false,true,8,.8f);check(jiggle1.rotation!=jiggle2.rotation||jiggle1.x!=jiggle2.x);check(jiggle1.alpha==1);
 auto staticFrame=motion.sample(animation,0,1750,11,1,false,false,8,.8f);check(staticFrame.scale==1&&staticFrame.x==0&&staticFrame.y==0&&staticFrame.rotation==0);
 check(motion.sample(animation,1,2500,11,1,true,true,8,.8f).alpha==0);
 auto second=motion.sample(animation,1,3500,12,1,true,false,8,.8f);check(second.alpha>0&&second.scale>1.3f); // missed onset still pops when seen
 auto noReplay=motion.sample(animation,1,3600,12.5,1,true,false,8,.8f);check(noReplay.scale==1);
 peak::VisibleAnimation fast(2),slow(2);fast.sample(animation,0,1500,20,2,true,false,8,.8f);slow.sample(animation,0,1500,20,.5f,true,false,8,.8f);
 check(fast.sample(animation,0,1500,20.1,2,true,false,8,.8f).scale==1&&slow.sample(animation,0,1500,20.1,.5f,true,false,8,.8f).scale>1);
 std::cout<<"Passed: offset/late first-draw pop, visible bounce, independent wall-clock wiggle, future-word gate, no replay and animation speed\n";
}
