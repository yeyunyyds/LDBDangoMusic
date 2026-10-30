#include "entity_follow.hpp"
#include <iostream>
#include <stdexcept>
using namespace dango;
void check(bool v){if(!v)throw std::runtime_error("Entity follow regression");}
bool near(float a,float b){return std::abs(a-b)<.01f;}
inventory::Record object(uint32_t handle,Vec3 at,const char* name="CPhysicsProp"){inventory::Record r;r.handle=handle;r.index=handle&4095;r.serial=handle>>12;r.className=name;r.networkOrigin=at;r.networkAngles=Vec3{};r.parent=0xffffffff;r.parentAttachment=0;r.dormant=false;return r;}
int main(){
 inventory::Frame f;f.epoch=7;f.tick=1000;f.records.push_back(object(0x9001,{100,0,0}));inventory::FollowRequest req{true,7,0x9001};follow::Settings settings;settings.target=req;
 auto p=follow::resolve(f,req,7,1100);check(p.ready);auto a=follow::anchor(p,settings);check(near(a.x,100)&&near(a.z,32));
 f.records[0].networkOrigin=Vec3{120,5,0};auto moved=follow::resolve(f,req,7,1100);check(moved.ready&&near(follow::anchor(moved,settings).x,120));
 f.records[0].handle=0xa001;check(!follow::resolve(f,req,7,1100).ready);f.records[0].handle=0x9001;
 check(!follow::resolve(f,req,8,1100).ready&&!follow::resolve(f,req,7,1251).ready&&!follow::resolve(f,req,7,999).ready);
 f.records[0].dormant=true;check(!follow::resolve(f,req,7,1100).ready);f.records[0].dormant.reset();check(!follow::resolve(f,req,7,1100).ready);f.records[0].dormant=false;
 auto child=object(0x9002,{10,0,0},"CDynamicProp");child.parent=0x9001;f.records[0].networkOrigin=Vec3{100,0,0};f.records[0].networkAngles=Vec3{0,90,0};f.records.push_back(child);
 auto combined=follow::pose(f,0x9002);check(combined.ready);auto point=scene::point(combined.matrix,{});check(near(point.x,100)&&near(point.y,10));
 f.records[1].parentAttachment=2;check(!follow::pose(f,0x9002).ready);f.records[1].parentAttachment=0;f.records[0].parent=0x9002;check(!follow::pose(f,0x9002).ready);f.records[0].parent=0xffffffff;
 f.records[1].parent=0xa001;check(!follow::pose(f,0x9002).ready);f.records[1].parent=0x9001;
 auto weapon=object(0x9003,{0,0,0},"CPistol");weapon.owner=0x9001;f.records.push_back(weapon);check(!follow::pose(f,0x9003).ready&&!follow::classMatches(weapon,follow::Filter::GroundWeapons));
 f.records[0].parent.reset();check(!follow::pose(f,0x9001).ready);f.records[0].parent=0xffffffff;
 GameCameraFrame camera;camera.active=1;camera.epoch=7;camera.width=1920;camera.height=1080;camera.tick=1000;
 camera.worldToView={0,-1,0,0,0,0,1,0,1,0,0,0,0,0,0,1};camera.worldToClip={0,-1,0,0,0,0,1,0,1.0001f,0,0,-1.0001f,1,0,0,0};CameraPose eye;check(cameraPose(camera,eye));
 auto choices=follow::candidates(f,camera,eye,follow::Filter::Objects,1100);check(!choices.empty()&&choices.front().handle==0x9001);check(follow::candidates(f,camera,eye,follow::Filter::Players,1100).empty());check(follow::candidates(f,camera,eye,follow::Filter::Objects,1600).empty());
 f.records[0].networkOrigin=Vec3{-100,0,0};check(follow::candidates(f,camera,eye,follow::Filter::Physics,1100).empty());f.records.clear();check(!follow::resolve(f,req,7,1100).ready);
 // Newly selectable actor/vehicle classes must reach the actual visible-candidate path,
 // not just pass a class-name predicate. Other categories must remain separate.
 uint32_t serial=0xb001;
 for(auto category:{follow::Filter::CommonInfected,follow::Filter::SpecialInfected,follow::Filter::Vehicles}){
  std::vector<const char*> names;
  if(category==follow::Filter::CommonInfected)names={"Infected"};
  else if(category==follow::Filter::SpecialInfected)names={"Smoker","Boomer","Hunter","Spitter","Jockey","Charger","Tank","Witch"};
  else names={"CPropJeep","CPropVehicleDriveable","CPropVehicleChoreoGeneric","CFuncTrackTrain"};
  for(auto name:names){
   auto target=object(serial++,{100,0,0},name);f.records={target};
   for(auto filter:{category,follow::Filter::Objects}){auto c=follow::candidates(f,camera,eye,filter,1100);check(c.size()==1&&c[0].handle==target.handle);}
   check(follow::candidates(f,camera,eye,follow::Filter::Players,1100).empty());
   check(follow::resolve(f,{true,7,target.handle},7,1100).ready);
   f.records[0].networkOrigin=Vec3{150,0,0};check(near(follow::anchor(follow::resolve(f,{true,7,target.handle},7,1100),settings).x,150));
   f.records[0].dormant=true;check(follow::candidates(f,camera,eye,category,1100).empty());
  }
 }
 auto human=object(serial++,{100,0,0},"CTerrorPlayer");human.team=3;
 check(follow::classMatches(human,follow::Filter::SpecialInfected)&&!follow::classMatches(human,follow::Filter::Players));
 human.team=2;check(!follow::classMatches(human,follow::Filter::SpecialInfected)&&follow::classMatches(human,follow::Filter::Players));
 auto generic=object(serial++,{100,0,0},"NextBotCombatCharacter");check(!follow::classMatches(generic,follow::Filter::SpecialInfected));generic.team=3;check(follow::classMatches(generic,follow::Filter::SpecialInfected));
 for(auto name:{"CWorld","MovieExplosion","CParticleSystem","CPropaneTank"})check(!follow::classMatches(object(serial++,{100,0,0},name),follow::Filter::Objects));
 std::cout<<"Passed: moving target, full-handle reuse, epoch/stale/delete/dormancy guards, parent-local composition/cycles/sockets, owned-weapon rejection and visible player/infected/vehicle target selection\n";
}
