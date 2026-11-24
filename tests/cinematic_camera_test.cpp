#include "cinematic_camera.hpp"
#include "lyric_lifecycle.hpp"
#include "plugin_lifetime.hpp"
#include "cinematic_recovery.hpp"
#include "cinematic_architecture.hpp"
#include "cinematic_subject.hpp"
#include <cassert>
#include <vector>
using namespace dango;
using namespace dango::cinematic;
// Analytic static building: x in [100,180], |y| <= 180, z in [0,200].
static bool building(Vec3 a,Vec3 b,trace::Result& r){
 r={};r.start=a;r.end=b;
 auto d=b-a;
 if(a.x>100&&a.x<180&&std::abs(a.y)<180&&a.z>0&&a.z<200){r.startSolid=1;return true;}
 auto hit=[&](float t,Vec3 n){
  if(t<0||t>=r.fraction)return;
  auto p=a+d*t;
  if(p.x<99.99f||p.x>180.01f||std::abs(p.y)>180.01f||p.z<-.01f||p.z>200.01f)return;
  r.fraction=t;r.end=p;r.plane.normal=n;r.plane.distance=wall::dot(n,p);
 };
 if(d.x>0)hit((100-a.x)/d.x,{-1,0,0});
 if(d.x<0)hit((180-a.x)/d.x,{1,0,0});
 if(d.z<0)hit((200-a.z)/d.z,{0,0,1});
 return true;
}
static inventory::Record survivor(uint32_t index,uint32_t handle,int character,const char* name="CTerrorPlayer"){
 inventory::Record r;r.index=index;r.handle=handle;r.className=name;r.team=2;r.health=100;r.lifeState=0;r.dormant=false;r.character=character;r.networkOrigin=Vec3{10,0,0};r.networkAngles=Vec3{};return r;
}
int main(){
 {lifetime::Guard guard;assert(guard&&lifetime::callbacks.load()==1);lifetime::requestStop();lifetime::Guard denied;assert(!denied&&lifetime::callbacks.load()==1);}assert(!lifetime::callbacks.load());
 trace::Result facade;assert(building({0,0,80},{200,0,80},facade)&&trace::wallHit(facade));
 wall::Panel roof;assert(roofPanel(facade,1,building,roof));
 assert(roof.center.x>100&&roof.center.z-roof.height*.5f>200&&roof.normal.x<-.99f);
 auto empty=[](Vec3 a,Vec3 b,trace::Result& r){r={};r.start=a;r.end=b;return true;};
 assert(!roofPanel(facade,1,empty,roof)); // no inferred roof from a wall alone
 inventory::Frame players;players.records.push_back(survivor(1,4097,0));
 players.records.push_back(survivor(2,4098,1));
 assert(subject(players,1,0xffffffffu,{})->handle==4097);
 players.records[0].team=1;players.records.push_back(survivor(3,8195,0,"SurvivorBot"));
 assert(subject(players,1,4097,0)->handle==8195);
 assert(subject(players,1,8195,0)->handle==8195);
 players.records[0].observerTarget=4098;
 assert(subject(players,1,0xffffffffu,{})->handle==4098);
 players.records.push_back(survivor(4,8196,0,"SurvivorBot"));
 assert(subject(players,1,4097,0)->handle==4098); // ambiguous character: only actual observer fallback
 players.records[0].observerTarget.reset();
 assert(!subject(players,1,4097,0));

 Tracker tracker;auto a=tracker.sample(1,100,1000,{0,0,40},0);
 auto b=tracker.sample(1,100,1100,{30,0,40},0);
 assert(b.velocity.x>100);
 auto c=tracker.sample(1,101,1200,{3000,0,40},0);assert(length(c.velocity)==0);
 Departure departure;
 auto safe=[](Vec3,Vec3){return true;};
 Pose initial{{0,0,64},0,0,0};
 assert(departure.plan(1000,initial,a,safe));
 assert(length(departure.step(1000).eye-initial.eye)<.001f);
 assert(length(departure.step(1150).eye-initial.eye)>30);
 auto cleared=departure.step(1300);assert(!departure.active&&length(cleared.eye-initial.eye)>=64);
 assert(!departure.plan(1400,cleared,a,safe));
 assert(!departure.plan(1500,initial,a,[](Vec3,Vec3){return false;}));
 assert(departure.plan(1600,initial,a,[](Vec3 from,Vec3 to){return to.x<from.x;}));
 assert(departure.step(1900).eye.x<0);
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
 assert(choose(Style::Mixed,0,3,true)==Style::Wall);
 assert(surfaceStyle(Style::Wall)&&surfaceStyle(Style::Architecture));
 assert(choose(Style::Mixed,300,0,true,styleBit(Style::Orbit))==Style::Orbit);
 assert(choose(Style::Mixed,0,1,false,styleBit(Style::Wall))==Style::Mixed);
 assert(choose(Style::Mixed,0,1,false,styleBit(Style::Bird))==Style::Bird);
 assert(choose(Style::Mixed,0,0,true,0)==Style::Mixed);
 assert(choose(Style::Bird,0,0,false,0)==Style::Bird);
 assert(nextSequence(0,Style::Wide)==3);
 assert(healthyView(true,500,850)&&!healthyView(false,100,850)&&!healthyView(true,900,850));
 RecoveryGate gate;assert(gate.healthy(1000,false,200,850,false));
 assert(gate.healthy(1240,false,200,850,false));assert(!gate.healthy(1360,false,200,850,false));
 assert(gate.healthy(1400,true,200,850,false));assert(!gate.healthy(1500,false,1500,850,false));
 MotionController motion;Pose actual{{0,0,64},0,179,0};Pose desired{{100,0,64},0,-179,0};
 auto first=motion.step(actual,desired,.004f);assert(first.eye.x>0&&first.eye.x<1&&std::abs(wrap(first.yaw-actual.yaw))<1);
 auto second=motion.step(first,desired,.004f);assert(second.eye.x-first.eye.x>first.eye.x);
 desired.eye.x=120;auto third=motion.step(second,desired,.004f);assert(third.eye.x>second.eye.x&&length(third.eye-second.eye)<4);
 motion.blocked();assert(length(motion.linear)==0);
 Director predictive;auto runner=a;runner.velocity={220,0,0};
 assert(predictive.start({{220,-100,180},0,0,0}));
 assert(predictive.plan(1000.25,6000,runner,Style::Orbit,{},clear));
 assert(predictive.anticipating&&predictive.intercept.x>300&&predictive.to.eye.x>predictive.intercept.x);
 auto pre=predictive.step(2200.25,runner);runner.center.x+=30;
 auto pre2=predictive.step(2300.25,runner);assert(length(pre.eye-pre2.eye)<.01f);
 assert(!predictive.needsPlan(2300.25,runner,Style::Orbit));
 runner.velocity={-220,0,0};assert(predictive.needsPlan(2300.25,runner,Style::Orbit));
 Director lateRunner;assert(lateRunner.start({{-200,0,80},0,0,0}));
 assert(lateRunner.plan(1000,6000,a,Style::Orbit,{},clear));assert(lateRunner.needsPlan(2300,runner,Style::Orbit));
 auto seq=predictive.sequence;assert(predictive.rebase(pre2)&&predictive.sequence==seq);
 Director noAllowed;assert(noAllowed.start({{0,0,64},0,0,0}));
 assert(!noAllowed.plan(1000,6000,a,Style::Mixed,{},clear,false,styleBit(Style::Wall)));
 assert(noAllowed.active==Style::Mixed);
 Composition roofComp;roofComp.ready=true;roofComp.first=roof;
 assert(roofPanel(facade,1,building,roofComp.first));
 Director arch;assert(arch.start({{-200,0,80},0,0,0}));
 assert(arch.plan(1000,6000,a,Style::Architecture,roofComp,clear));
 auto raised=arch.step(7000,a);assert(raised.eye.z>roofComp.first.center.z);
 Composition wallComp;wallComp.ready=true;assert(wall::makePanel(facade.end,facade.plane.normal,180,48,1,wallComp.first,2));
 Director wallCam;assert(wallCam.start({{-200,0,80},0,0,0}));
 assert(wallCam.plan(1000,6000,a,Style::Wall,wallComp,clear));
 auto low=wallCam.step(7000,a);assert(low.eye.z<=wallComp.first.center.z+70.01f&&raised.eye.z>low.eye.z+100);

 Director follow;assert(follow.start({{-200,0,80},0,0,0}));
 assert(follow.plan(1000,6000,a,Style::Orbit,{},clear,true));
 auto fixed=follow.step(3000,a);auto shifted=a;shifted.center.x+=250;
 auto tracked=follow.step(3000,shifted);assert(std::abs(tracked.eye.x-fixed.eye.x-250)<.01f);
 auto birdPanel=freePanel(Style::Bird,a,{100,100,500});assert(birdPanel.normal.z==1&&birdPanel.center.z<a.center.z);
 auto orbitPanel=freePanel(Style::Orbit,a,{-200,0,80});assert(orbitPanel.normal.x<-.9f);
 auto widePanel=freePanel(Style::Wide,a,{-200,0,80});assert(widePanel.width>orbitPanel.width);auto wideOther=freePanel(Style::Wide,a,{-200,0,80},true);assert(length(wideOther.center-widePanel.center)>300);
 // Dynamic panel orientation follows the actual render eye even between actor samples.
 RenderActor renderActor;auto ra=renderActor.update(a,1000);
 auto eyeA=freePanel(Style::Orbit,ra,{300,0,80});auto eyeB=freePanel(Style::Orbit,ra,{300,30,80});
 assert(eyeA.normal.y==0&&eyeB.normal.y>0&&length(eyeA.center-eyeB.center)>1);
 auto moved=a;moved.center.x+=20;
 auto r1=renderActor.update(moved,1016);auto r2=renderActor.update(moved,1032);
 assert(r1.center.x>0&&r1.center.x<r2.center.x&&r2.center.x<20);
 moved.handle++;assert(renderActor.update(moved,1048).center.x==20);
 assert(chooseLyricMode(true,true,true)==LyricMode::Second);
 assert(chooseLyricMode(true,true,false)==LyricMode::First);
 assert(chooseLyricMode(false,false,true)==LyricMode::Second);
 assert(chooseLyricMode(false,false,false)==LyricMode::Subtitle);
 LyricLifecycle lyricLife;lyricLife.update("song",0,"A",1,100,300,false,2,false);
 auto locked=chooseLyricMode(false,true,false);auto firstLease=lyricLife.entries.back().id;
 lyricLife.update("song",0,"A",1.1,100,300,false,2,false);
 assert(lyricLife.entries.back().id==firstLease&&locked==LyricMode::First);
 lyricLife.update("song",1,"B",1.2,100,300,false,2,false);
 assert(lyricLife.entries.size()==2&&lyricLife.entries.front().id==firstLease&&lyricLife.entries.front().retired>=0);
 assert(chooseLyricMode(false,false,false)==LyricMode::Subtitle&&locked==LyricMode::First);
 Recovery recovery;auto far=a;far.center={1500,0,40};Pose stuck{{0,0,80},0,0,0};
 auto localClear=[](Vec3 from,Vec3 to){return from.x>1000&&to.x>1000;};
 auto seen=[](Vec3,const Actor&){return true;};
 assert(recovery.probe(1000,stuck,far,false,localClear,seen)==Recovery::Action::Hold);
 assert(recovery.probe(3600,stuck,far,false,localClear,seen)==Recovery::Action::Cut);
 assert(length(recovery.target-far.center)<250);
 recovery.reset();assert(recovery.probe(1000,stuck,far,false,blocked,seen)==Recovery::Action::Hold);
 assert(recovery.probe(7600,stuck,far,false,blocked,seen)==Recovery::Action::Native);
 struct L{int64_t start;};std::vector<L> lines{{0},{1000},{2000},{5500},{9000}};
 assert(duration(lines,1000,1)==4500);
 assert(duration(lines,-1,1)==6000);
}
