#include "gnome_follow.hpp"
#include <stdexcept>
#include <iostream>
using namespace dango;
void check(bool b){if(!b)throw std::runtime_error("Gnome representation bridge regression");}
inventory::Record item(uint32_t h,const char* cls,float x=100){inventory::Record r;r.handle=h;r.className=cls;r.networkOrigin=Vec3{x,0,0};r.networkAngles=Vec3{};r.parent=0xffffffffu;r.parentAttachment=0;r.dormant=false;r.modelIndex=42;r.worldModelIndex=42;r.isCarryable=true;return r;}
int main(){
 inventory::FollowRequest request{true,7,0x9001};inventory::Frame f;f.epoch=7;f.tick=1000;
 auto prop=item(request.handle,"CPhysicsProp");auto actor=item(0x9002,"CTerrorPlayer",110);actor.activeWeapon=0x9003;
 auto weapon=item(0x9003,"CGnome");weapon.owner=actor.handle;weapon.weaponOwner=actor.handle;
 follow::GnomeBridge bridge;bridge.begin(request);f.records={prop};bridge.step(f,1000,true);check(bridge.armed&&bridge.current==request.handle);
 f.records={weapon,actor};f.tick=1100;bridge.step(f,1100,true);check(bridge.current==weapon.handle&&bridge.holder==actor.handle);
 // The owner remains after release; the world representation is a new handle.
 auto dropped=item(0xa001,"CPhysicsProp",140);actor.activeWeapon=0x9004;f.records={dropped,actor};f.tick=1200;bridge.step(f,1200,true);check(bridge.current==dropped.handle);
 f.requestedFollowHandle=request.handle;f.followedHandle=bridge.current;check(follow::resolve(f,request,7,1200).ready);check(!follow::resolve(f,request,8,1200).ready);
 inventory::FollowRequest other{true,7,0x9005};check(!follow::resolve(f,other,7,1200).ready);
 // Pick up / throw repeatedly without changing the user's original request.
 weapon.handle=0xb003;actor.activeWeapon=weapon.handle;f.records={weapon,actor};f.tick=1300;bridge.step(f,1300,true);check(bridge.current==weapon.handle);
 dropped.handle=0xc001;actor.activeWeapon=0x9004;f.records={dropped,actor};f.tick=1400;bridge.step(f,1400,true);check(bridge.current==dropped.handle);
 // A pre-existing lookalike must not steal the target, even if closer.
 bridge={};bridge.begin(request);auto old=item(0xf001,"CPhysicsProp",111);f.records={prop,old};f.tick=2000;bridge.step(f,2000,true);
 actor.activeWeapon=weapon.handle;f.records={weapon,actor,old};f.tick=2100;bridge.step(f,2100,true);check(bridge.current==weapon.handle);
 actor.activeWeapon=0x9004;f.records={old,actor};f.tick=2200;bridge.step(f,2200,true);check(bridge.current==weapon.handle);
 f.records={old,dropped,actor};f.tick=2300;bridge.step(f,2300,true);check(bridge.current==dropped.handle);
 // Two newly dropped lookalikes are ambiguous; wait instead of choosing one.
 bridge={};bridge.begin(request);f.tick=2400;f.records={prop};bridge.step(f,2400,true);actor.activeWeapon=weapon.handle;f.tick=2500;f.records={weapon,actor};bridge.step(f,2500,true);
 auto second=dropped;second.handle=0xe001;actor.activeWeapon=0x9004;f.tick=2600;f.records={dropped,second,actor};bridge.step(f,2600,true);check(bridge.current==weapon.handle);
 f.tick=2700;f.records={dropped,actor};bridge.step(f,2700,true);check(bridge.current==dropped.handle);
 // A hidden original may still exist in the list during a representation swap.
 bridge={};bridge.begin(request);f.tick=2800;f.records={prop};bridge.step(f,2800,true);auto hidden=prop;hidden.effects=32;actor.activeWeapon=weapon.handle;f.tick=2900;f.records={hidden,weapon,actor};bridge.step(f,2900,true);check(bridge.current==weapon.handle&&!bridge.needsScan(f,2900));
 // Matching model IDs alone are insufficient: require active owner + uniqueness.
 auto reset=[&](){bridge={};bridge.begin(request);f.tick=3000;f.records={prop};bridge.step(f,3000,true);};
 reset();actor.activeWeapon=0x9004;f.records={weapon,actor};f.tick=3100;bridge.step(f,3100,true);check(bridge.current==request.handle);
 reset();actor.activeWeapon=weapon.handle;weapon.worldModelIndex=43;f.records={weapon,actor};f.tick=3100;bridge.step(f,3100,true);check(bridge.current==request.handle);weapon.worldModelIndex=42;
 reset();auto actor2=actor;actor2.handle=0x9006;auto weapon2=weapon;weapon2.handle=0xd003;weapon2.owner=weapon2.weaponOwner=actor2.handle;actor2.activeWeapon=weapon2.handle;f.records={weapon,actor,weapon2,actor2};f.tick=3100;bridge.step(f,3100,true);check(bridge.current==request.handle);
 reset();actor.networkOrigin=Vec3{1000,0,0};f.records={weapon,actor};f.tick=3100;bridge.step(f,3100,true);check(bridge.current==request.handle);actor.networkOrigin=Vec3{110,0,0};
 reset();f.records.clear();f.tick=3100;bridge.step(f,3100,true);f.records={weapon,actor};f.tick=8200;bridge.step(f,8200,true);check(bridge.current==request.handle&&!bridge.needsScan(f,8200));
 reset();f.records={weapon,actor};f.epoch=8;f.tick=3100;bridge.step(f,3100,true);check(bridge.current==request.handle);f.epoch=7;
 bridge.begin(other);check(!bridge.armed&&bridge.current==other.handle);
 // Hidden original props cannot keep rendering stale coordinates.
 prop.effects=32;f.records={prop};check(!follow::pose(f,prop.handle).ready);
 std::cout<<"Passed: gnome prop/weapon/prop repeated transfers, full-handle aliases, active holder/model proof, pre-existing lookalikes, ambiguity/range/timeout/map guards and hidden prop rejection\n";
}
