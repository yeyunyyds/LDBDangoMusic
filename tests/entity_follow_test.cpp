#include "entity_follow.hpp"
#include <iostream>
#include <stdexcept>
using namespace dango;
void check(bool v){if(!v)throw std::runtime_error("Entity follow regression");}
bool near(float a,float b){return std::abs(a-b)<.01f;}
inventory::Record object(uint32_t handle,Vec3 at,const char* name="CPhysicsProp"){inventory::Record r;r.handle=handle;r.index=handle&4095;r.serial=handle>>12;r.className=name;r.networkOrigin=at;r.networkAngles=Vec3{};r.parent=0xffffffff;r.parentAttachment=0;r.dormant=false;return r;}
int main(){
 follow::Settings routing;check(!follow::useEntityRoute(routing,7));routing.enabled=true;check(!follow::useEntityRoute(routing,7));routing.target={true,7,0x9001};check(follow::useEntityRoute(routing,7)&&!follow::useEntityRoute(routing,8));routing.target={};check(!follow::useEntityRoute(routing,7));
 follow::Settings layout;follow::adjustLayout(layout,0,0);follow::adjustLayout(layout,1,1);follow::adjustLayout(layout,2,3);follow::adjustLayout(layout,3,2);check(near(layout.offset.y,10)&&near(layout.offset.z,33)&&near(layout.offset.x,10)&&near(layout.size,1.05f));
 for(int i=0;i<100;++i){follow::adjustLayout(layout,0,0);follow::adjustLayout(layout,3,3);}check(layout.offset.y==512&&layout.size==3);
 for(int axis=0;axis<4;++axis)follow::adjustLayout(layout,axis,4);check(near(layout.offset.x,0)&&near(layout.offset.y,0)&&near(layout.offset.z,32)&&near(layout.size,1));
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
 // All concrete pickup classes seen in this DLL, including previously omitted
 // basic guns, snipers and map spawn entities, must be selectable when unowned.
 for(auto name:{"CPistol","CMagnumPistol","CSubMachinegun","CSMG_Silenced","CSMG_MP5",
  "CPumpShotgun","CShotgun_Chrome","CAutoShotgun","CShotgun_SPAS","CAssaultRifle",
  "CRifle_Desert","CRifle_AK47","CRifle_SG552","CRifle_M60","CSniperRifle",
  "CSniper_Military","CSniper_AWP","CSniper_Scout","CGrenadeLauncher","CChainsaw","CTerrorMeleeWeapon",
  "CFirstAidKit","CPainPills","CItem_Adrenaline","CItemDefibrillator","CItemUpgradePackExplosive",
  "CItemUpgradePackIncendiary","CItem_VomitJar","CPipeBomb","CMolotov","CGasCan","CColaBottles",
  "CGnome","COxygenTank","CPropaneTank","CFireworkCrate","CWeaponSpawn","CWeaponAmmoSpawn",
  "CWeaponScavengeItemSpawn","CBaseUpgradeItem"}){
  auto gun=object(serial++,{100,0,0},name);gun.owner=0xffffffffu;gun.weaponOwner=0xffffffffu;
  f.records={gun};for(auto filter:{follow::Filter::GroundWeapons,follow::Filter::Objects}){
   auto targets=follow::candidates(f,camera,eye,filter,1100);check(targets.size()==1&&targets[0].handle==gun.handle);
  }
  inventory::FollowRequest lock{true,7,gun.handle};check(follow::resolve(f,lock,7,1100).ready);
  // Picking up a locked target must hide it as well as exclude new selections.
  for(bool combatOwner:{false,true}){f.records={gun};if(combatOwner)f.records[0].weaponOwner=0x9001;else f.records[0].owner=0x9001;
   check(follow::candidates(f,camera,eye,follow::Filter::GroundWeapons,1100).empty());
   check(!follow::resolve(f,lock,7,1100).ready);
   f.records[0].parent=0x9001;f.records.push_back(object(0x9001,{200,0,0}));check(!follow::resolve(f,lock,7,1100).ready);
  }
 }
 // Ground -> carried -> thrown with owner fields retained -> carried again.
 // The selected full handle never changes and other gnomes are not substituted.
 for(auto name:{"CGnome","CGasCan","CColaBottles","COxygenTank","CPropaneTank","CFireworkCrate"}){
 auto item=object(0x9003,{100,0,0},name);auto holder=object(0x9004,{90,0,0},"CTerrorPlayer");holder.activeWeapon=item.handle;
 inventory::FollowRequest locked{true,7,item.handle};f.records={item};check(follow::resolve(f,locked,7,1100).ready);
 item.owner=holder.handle;item.weaponOwner=holder.handle;f.records={item,holder};check(!follow::resolve(f,locked,7,1100).ready);
 holder.activeWeapon=0x9005;item.networkOrigin=Vec3{140,10,5};f.records={item,holder};check(follow::resolve(f,locked,7,1100).ready);
 check(follow::candidates(f,camera,eye,follow::Filter::GroundWeapons,1100).size()==1);
 holder.activeWeapon=item.handle;f.records={item,holder};check(!follow::resolve(f,locked,7,1100).ready);
 holder.activeWeapon=0x9005;item.parent=holder.handle;f.records={item,holder};check(!follow::resolve(f,locked,7,1100).ready);
 item.parent=0xffffffffu;item.networkOrigin=Vec3{};f.records={item,holder};check(!follow::resolve(f,locked,7,1100).ready);
 item.networkOrigin=Vec3{140,10,5};f.records={item};check(!follow::resolve(f,locked,7,1100).ready);
 holder.activeWeapon.reset();f.records={item,holder};check(!follow::resolve(f,locked,7,1100).ready);
 holder.activeWeapon=0x9005;holder.dormant=true;f.records={item,holder};check(!follow::resolve(f,locked,7,1100).ready);
 holder.dormant=false;f.records={item,holder};check(follow::resolve(f,locked,7,1100).ready);
 f.records[0].handle=0xa003;check(!follow::resolve(f,locked,7,1100).ready);
 }
 for(auto name:{"CGrenadeLauncher_Projectile","CMolotovProjectile","CPipeBombProjectile","CVomitJarProjectile"}){
  auto shot=object(serial++,{100,0,0},name);shot.owner=0x9001;f.records={shot};
  check(follow::candidates(f,camera,eye,follow::Filter::GroundWeapons,1100).empty());
  check(follow::candidates(f,camera,eye,follow::Filter::Projectiles,1100).size()==1);
 }
 for(auto name:{"CTerrorViewModel","CEnvWeaponFire","CWeaponCubemap","CWeaponCycler","CPropMountedGun","CRifle_Unknown"})
  check(!follow::classMatches(object(serial++,{100,0,0},name),follow::Filter::GroundWeapons));
 auto human=object(serial++,{100,0,0},"CTerrorPlayer");human.team=3;
 check(follow::classMatches(human,follow::Filter::SpecialInfected)&&!follow::classMatches(human,follow::Filter::Players));
 human.team=2;check(!follow::classMatches(human,follow::Filter::SpecialInfected)&&follow::classMatches(human,follow::Filter::Players));
 auto generic=object(serial++,{100,0,0},"NextBotCombatCharacter");check(!follow::classMatches(generic,follow::Filter::SpecialInfected));generic.team=3;check(follow::classMatches(generic,follow::Filter::SpecialInfected));
 for(auto name:{"CWorld","MovieExplosion","CParticleSystem"})check(!follow::classMatches(object(serial++,{100,0,0},name),follow::Filter::Objects));
 std::cout<<"Passed: moving target, full-handle reuse, epoch/stale/delete/dormancy guards, parent-local composition/cycles/sockets, owned-weapon rejection and visible weapon/item/player/infected/vehicle target selection\n";
}
