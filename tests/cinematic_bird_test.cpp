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
 // Actual arrival: hold position despite further scene samples.
 p.eye=target;d=bird.update(3200,.016f,p,a,{},false,indoor,indoor,space);assert(!bird.moving);
 d=bird.update(3300,.016f,p,a,{},false,indoor,indoor,space);assert(length(d.pose.eye-p.eye)<.001f);
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
 std::cout<<"PASS: co-op subjects, ghost filtering, combat hysteresis, frustum, low ceiling, locked movement, arrival hold, safe distant cut and common recovery lock\n";
}
