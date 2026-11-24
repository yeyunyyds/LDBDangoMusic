#include "cinematic_camera.hpp"
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
 Composition roofComp;roofComp.ready=true;roofComp.first=roof;
 assert(roofPanel(facade,1,building,roofComp.first));
 Director arch;assert(arch.start({{-200,0,80},0,0,0}));
 assert(arch.plan(1000,6000,a,Style::Architecture,roofComp,clear));
 auto raised=arch.step(7000,a);assert(raised.eye.z>roofComp.first.center.z);
 Composition wallComp;wallComp.ready=true;assert(wall::makePanel(facade.end,facade.plane.normal,180,48,1,wallComp.first,2));
 Director wallCam;assert(wallCam.start({{-200,0,80},0,0,0}));
 assert(wallCam.plan(1000,6000,a,Style::Wall,wallComp,clear));
 auto low=wallCam.step(7000,a);assert(low.eye.z<=wallComp.first.center.z+70.01f&&raised.eye.z>low.eye.z+100);

 struct L{int64_t start;};std::vector<L> lines{{0},{1000},{2000},{5500},{9000}};
 assert(duration(lines,1000,1)==4500);
 assert(duration(lines,-1,1)==6000);
}
