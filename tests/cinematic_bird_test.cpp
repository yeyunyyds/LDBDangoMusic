#include "cinematic_bird.hpp"
#include "cinematic_recovery.hpp"
#include <cassert>
#include <iostream>
using namespace dango;using namespace dango::cinematic;
inventory::Record record(uint32_t handle,Vec3 origin,int team,const char* name,int health=100){
 inventory::Record r;r.handle=handle;r.networkOrigin=origin;r.networkAngles=Vec3{};r.aimAngles=Vec3{};r.team=team;r.className=name;r.health=health;r.dormant=false;r.lifeState=0;return r;
}
int main(){
 auto clear=[](Vec3,Vec3){return true;};auto space=[](Vec3){return 1.f;};
 Actor a{1,1,1000,{0,0,38},{220,0,0},0,true};inventory::Frame f;f.epoch=1;
 f.records={record(1,{0,0,0},2,"CTerrorPlayer"),record(2,{20,30,0},2,"SurvivorBot"),record(3,{300,0,0},3,"Hunter"),record(4,{1000,0,0},2,"SurvivorBot")};
 auto ghost=record(5,{100,0,0},3,"CTerrorPlayer");ghost.zombieClass=1;ghost.ghost=true;f.records.push_back(ghost);
 BirdDirector bird;bird.scene.sample(1000,a,f,clear);bird.scene.sample(1200,a,f,clear);bird.scene.sample(1400,a,f,clear);
 assert(bird.scene.team.size()==2&&bird.scene.enemies.size()==1&&bird.scene.combatHandle==3);
 assert(bird.scene.focus(a).x>0);
 bird.scene.engagement=0;bird.scene.advance=1;bird.scene.direction={1,0,0};auto aim=bird.scene.focus(a);
 assert(bird.score({-400,0,250},aim,{-400,0,250},a,{},clear)>bird.score({600,0,250},aim,{600,0,250},a,{},clear));
 auto sightBlocked=[](Vec3,Vec3){return false;};
 bird.scene.sample(2500,a,f,sightBlocked);assert(!bird.scene.combatHandle);
 assert(birdFrame({0,0,0},{100,0,0},{50,0,0},{}));
 assert(!birdFrame({0,0,0},{100,0,0},{-50,0,0},{}));
 // Ceiling: high candidates unavailable, low angle position remains legal.
 auto indoor=[](Vec3 x,Vec3 y){return x.z<=160&&y.z<=160;};
 Pose p{{-20,0,64},0,0,0};auto d=bird.update(3000,.016f,p,a,{},false,indoor,indoor,space);
 assert(!d.cut&&bird.moving&&d.pose.eye.z<=160);
 auto target=bird.target;a.center.x+=10;
 d=bird.update(3120,.016f,p,a,{},false,indoor,indoor,space);
 assert(length(target-bird.target)<.001f);
 // Actual arrival: short reframing hold, continuous tracking remains available.
 p.eye=target;d=bird.update(3200,.016f,p,a,{},false,indoor,indoor,space);assert(!bird.moving);
 d=bird.update(3300,.016f,p,a,{},false,indoor,indoor,space);assert(length(d.pose.eye-p.eye)<.001f);
 // An anchored shot follows small displacement even inside reframe cooldown.
 bird.reset();a.center={0,0,38};a.velocity={60,0,0};p.eye={-400,0,358};
 bird.initialized=true;bird.anchored=true;bird.anchorActor=a.center;bird.anchorEye=p.eye;bird.target=p.eye;bird.focus=a.center;bird.holdUntil=9000;bird.nextPlan=9000;
 a.center.x=30;d=bird.update(4000,.016f,p,a,{},false,clear,clear,space);
 assert(!d.cut&&bird.mode==BirdMove::Track&&d.pose.eye.x>p.eye.x&&d.speedCap==50.f);
 auto before=d.pose.eye;p.eye.x+=8;a.center.x+=8;d=bird.update(4100,.016f,p,a,{},false,clear,clear,space);
 assert(bird.mode==BirdMove::Track&&d.pose.eye.x>before.x);
 // Running does not immediately accelerate a usable composition.
 a.velocity={260,0,0};a.center.x+=40;
 d=bird.update(4200,.016f,p,a,{},false,clear,clear,space);
 assert(!d.cut&&bird.mode==BirdMove::Track&&d.speedCap==50.f);
 // When the relative shot is genuinely lost, fast catch-up becomes eligible.
 a.center={500,0,38};
 d=bird.update(4300,.016f,p,a,{},false,clear,clear,space);
 assert(!d.cut&&bird.mode==BirdMove::Fast&&d.speedCap==650.f&&bird.cadence.streak==1);
 // Brief occlusion does not trigger emergency replan; persistent occlusion permits safe cut.
 bird.reset();a.center={0,0,38};a.velocity={};p.eye={-400,0,358};
 bird.initialized=true;bird.anchored=true;bird.anchorActor=a.center;bird.anchorEye=p.eye;bird.focus=a.center;bird.nextPlan=20000;
 auto onlyLanding=[](Vec3 x,Vec3 y){return x.x>-390&&y.x>-390;};
 d=bird.update(5000,.016f,p,a,{},false,onlyLanding,onlyLanding,space);assert(!d.cut);
 d=bird.update(6400,.016f,p,a,{},false,onlyLanding,onlyLanding,space);assert(!d.cut&&bird.hiddenSince==5000);
 d=bird.update(6500,.016f,p,a,{},false,onlyLanding,onlyLanding,space);assert(!d.cut&&bird.recovering);
 d=bird.update(10100,.016f,p,a,{},false,onlyLanding,onlyLanding,space);assert(d.cut);
 // Brief occlusion permits collision-checked normal follow and clears on visibility.
 bird.reset();a.center={0,0,38};a.velocity={60,0,0};p.eye={-400,0,358};
 bird.initialized=true;bird.anchored=true;bird.anchorActor=a.center;bird.anchorEye=p.eye;bird.target=p.eye;bird.focus=a.center;bird.nextPlan=20000;
 a.center.x=30;d=bird.update(11000,.016f,p,a,{},false,clear,sightBlocked,space);
 assert(!d.cut&&bird.mode==BirdMove::Track&&bird.hiddenSince==11000);
 d=bird.update(12100,.016f,p,a,{},false,clear,clear,space);assert(!bird.hiddenSince&&!bird.recovering);
 // A valid relocation must not be interrupted by ordinary reframe timers.
 bird.reset();bird.initialized=true;bird.mode=BirdMove::Slow;bird.moving=true;bird.target={-200,0,358};bird.nextPlan=0;bird.nextCheck=0;
 d=bird.update(13000,.016f,p,a,{},false,clear,clear,space);assert(bird.mode==BirdMove::Slow&&length(bird.target-Vec3{-200,0,358})<.001f);
 // Slow tracking respects 50 units/s and 90 units/s^2, even with a far waypoint.
 MotionController motor;Pose dest=p;dest.eye.x+=1000;
 for(int i=0;i<100;++i){auto old=p.eye;auto v=motor.linear;p=motor.step(p,dest,.016f,false,false,50.f,true);assert(length(p.eye-old)<=50.f*.016f+.001f&&length(motor.linear-v)<=90.f*.016f+.001f);}
 assert(length(motor.linear)>49.f);
 motor.linear={300,0,0};auto oldSpeed=length(motor.linear);p=motor.step(p,dest,.016f,false,false,50.f,true);assert(length(motor.linear)<oldSpeed&&length(motor.linear)>50.f);
 for(int i=0;i<60;++i)p=motor.step(p,dest,.016f,false,false,50.f,true);
 assert(length(motor.linear)<=50.001f);
 BirdCadence cadence;assert(cadence.fast(.999f));cadence.usedFast();assert(cadence.fast(.49f)&&!cadence.fast(.5f));cadence.usedFast();assert(cadence.fast(.249f)&&!cadence.fast(.25f));cadence.alternative();assert(cadence.fast(.999f));
 // Far subject: a safe landing can be selected without an old-to-new path.
 bird.reset();a.center={3000,0,38};a.velocity={0,0,0};p.eye={0,0,64};
 auto remote=[](Vec3 x,Vec3 y){return x.x>2000&&y.x>2000;};
 d=bird.update(5000,.016f,p,a,{},false,remote,remote,space);assert(d.cut&&length(d.pose.eye-a.center)<1000.f);
 bird.reset();d=bird.update(5000,.016f,p,a,{},false,sightBlocked,sightBlocked,space);assert(!d.cut&&!bird.moving);
 // Common recovery target persists across a new candidate geometry preference.
 Recovery recovery;a.center={300,0,38};p.eye={0,0,64};
 auto visible=[](Vec3,const Actor&){return true;};
 assert(recovery.probe(1000,p,a,false,clear,visible)==Recovery::Action::Follow);auto first=recovery.target;a.center.y+=12;
 assert(recovery.probe(1120,p,a,false,clear,visible)==Recovery::Action::Follow);assert(length(first-recovery.target)<.001f);
 recovery.reset();a.center={300,0,38};assert(recovery.probe(1000,p,a,false,sightBlocked,visible)==Recovery::Action::Hold);
 assert(recovery.probe(8000,p,a,false,sightBlocked,visible)==Recovery::Action::Hold);
 std::cout<<"PASS: co-op subjects, ghost filtering, combat hysteresis, frustum, low ceiling, locked relocation, continuous follow, occlusion grace, fast cadence, motion cap and safe cut\n";
}
