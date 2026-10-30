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
 // Progressive visibility must survive static presets and be independently disabled.
 peak::VisibleAnimation fixed(2);
 check(peak::revealFrame(fixed,animation,1,2500,30,1,true,false,false,8,0).alpha==0);
 check(peak::revealFrame(fixed,animation,1,2500,30,1,false,false,false,8,0).alpha==1);
 auto translation=peak::translationAnimation(animation,u8"甲乙丙丁",4000);peak::VisibleAnimation translated(4);
 check(translation.starts.front()==1000&&translation.starts.back()==3000);
 check(peak::revealFrame(translated,translation,0,1500,30,1,true,false,false,8,0).alpha==1);
 check(peak::revealFrame(translated,translation,3,1500,30,1,true,false,false,8,0).alpha==0);
 check(peak::revealFrame(translated,translation,3,3500,32,1,true,false,false,8,0).alpha==1);
 // Untimed lyrics default to the whole sentence; gradual fallback is opt-in.
 peak::Animation untimed(Line{1000,"Hello world","",{}},5000);check(!untimed.hasWordTiming&&animation.hasWordTiming);
 peak::VisibleAnimation full,gradual;size_t last=untimed.glyphs.size()-1;
 check(peak::revealFrame(full,untimed,last,1000,40,1,true,false,false,8,0).alpha==1);
 check(peak::revealFrame(gradual,untimed,last,1000,40,1,true,false,false,8,0,true).alpha==0);
 auto trFallback=peak::translationAnimation(untimed,u8"你好世界",5000);check(!trFallback.hasWordTiming);
 peak::VisibleAnimation trWhole;check(peak::revealFrame(trWhole,trFallback,3,1000,40,1,true,false,false,8,0).alpha==1);
 // First actual draw establishes estimated timing, so a 500ms offset or
 // late API refresh cannot collapse every short/unbroken line into one frame.
 auto startFrame=peak::revealFrame(gradual,untimed,0,1500,40,1,false,false,false,8,0,true);check(startFrame.alpha>0&&startFrame.alpha<1);
 auto middle=peak::revealFrame(gradual,untimed,last,2500,40.5,1,false,false,false,8,0,true);check(middle.alpha==0);
 auto onsetFrame=peak::revealFrame(gradual,untimed,last,4500,43.25,1,false,false,false,8,0,true);check(onsetFrame.alpha>0&&onsetFrame.alpha<1);
 check(peak::revealFrame(gradual,untimed,last,4700,43.5,1,false,false,false,8,0,true).alpha==1);
 peak::Animation shortLine(Line{1000,u8"你好","",{}},6000);peak::VisibleAnimation shortMotion;
 check(peak::revealFrame(shortMotion,shortLine,0,1500,50,1,true,false,false,8,0,true).alpha>0);
 check(peak::revealFrame(shortMotion,shortLine,1,1500,50,1,true,false,false,8,0,true).alpha==0);
 check(peak::revealFrame(shortMotion,shortLine,1,1600,50.1,1,false,false,false,8,0,true).alpha==0); // later draws do not force an unrevealed glyph opaque
 auto oldVisible=peak::revealFrame(shortMotion,shortLine,0,1600,50.3,1,false,false,false,8,0,true);check(oldVisible.alpha==1);
 // A synthetic line-wide word token is treated as lacking fine timing.
 peak::Animation synthetic(Line{1000,"Hello","",{{1000,4000,"Hello"}}},5000);check(!synthetic.hasWordTiming);peak::VisibleAnimation syntheticMotion;
 check(peak::revealFrame(syntheticMotion,synthetic,4,1500,60,1,false,false,false,8,0,true).alpha==0);
 // A genuinely delayed single token still keeps its real onset.
 peak::Animation delayed(Line{1000,"Hello","",{{2000,1000,"Hello"}}},4000);check(delayed.hasWordTiming);
 check(peak::revealFrame(syntheticMotion,synthetic,0,1500,60,1,false,false,false,8,0,true).alpha>0);
 // Estimated scheduling follows song progress (including playback rate),
 // while the glyph entrance uses render time. Translation shares the span.
 peak::Animation rateLine(Line{1000,"UnbrokenWord","",{}},5000);auto rateTranslation=peak::translationAnimation(rateLine,u8"中文翻译",5000);
 peak::VisibleAnimation rateMotion,rateTrMotion;
 peak::revealFrame(rateMotion,rateLine,0,1000,70,1,false,false,false,8,0,true);
 peak::revealFrame(rateTrMotion,rateTranslation,0,1000,70,1,false,false,false,8,0,true);
 check(peak::revealFrame(rateMotion,rateLine,rateLine.glyphs.size()-1,4199,71.59,1,false,false,false,8,0,true).alpha==0);
 check(peak::revealFrame(rateMotion,rateLine,rateLine.glyphs.size()-1,4200,71.6,1,false,false,false,8,0,true).alpha>0);
 check(peak::revealFrame(rateTrMotion,rateTranslation,rateTranslation.glyphs.size()-1,4200,71.6,1,false,false,false,8,0,true).alpha>0);
 peak::Animation one(Line{0,u8"啊","",{}},3000);peak::VisibleAnimation oneMotion;
 auto oneFrame=peak::revealFrame(oneMotion,one,0,500,80,1,false,false,false,8,0,true);check(oneFrame.alpha>0&&oneFrame.alpha<1);
 peak::Animation oneSynthetic(Line{0,u8"啊","",{{0,1000,u8"啊"}}},3000);peak::VisibleAnimation oneSyntheticMotion;auto oneSyntheticFrame=peak::revealFrame(oneSyntheticMotion,oneSynthetic,0,500,90,1,false,false,false,8,0,true);check(oneSyntheticFrame.alpha>0&&oneSyntheticFrame.alpha<1);
 auto wrapped=peak::layout(animation.glyphs,[](const std::string&){return 10.f;},10,20);
 auto longer=peak::layout(animation.glyphs,[](const std::string&){return 10.f;},20,20);
 check(wrapped.rows.size()==2&&longer.rows.size()==1&&longer.glyphs[0].width==wrapped.glyphs[0].width);
 std::cout<<"Passed: offset/late first-draw pop, visible bounce, independent wall-clock wiggle, future-word gate, no replay and animation speed\n";
}
