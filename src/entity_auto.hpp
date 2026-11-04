#pragma once
#include "entity_follow.hpp"
#include "entity_auto_settings.hpp"
#include <set>
namespace dango::autofollow {
struct Type {std::string key,label;int group=3;};
inline bool alive(const inventory::Record& r){bool actor=r.character||r.zombieClass||r.className=="Infected"||r.className=="Tank"||r.className=="Witch"||r.className=="SurvivorBot"||r.className=="CTerrorPlayer"||r.className=="NextBotCombatCharacter";
 for(auto name:{"Smoker","Boomer","Hunter","Spitter","Jockey","Charger"})if(r.className==name)actor=true;
 return !actor||((!r.health||*r.health>0)&&(!r.lifeState||!*r.lifeState));}
inline int special(const inventory::Record& r){const char* names[]={"Smoker","Boomer","Hunter","Spitter","Jockey","Charger","Witch","Tank"};for(int i=0;i<8;++i)if(r.className==names[i])return i+1;return r.team&&*r.team==3&&r.zombieClass?*r.zombieClass:0;}
inline std::string semanticKey(const inventory::Record& r){if(r.character&&r.team&&*r.team==2)return "survivor:"+std::to_string(*r.character);
 const char* names[]={"Smoker","Boomer","Hunter","Spitter","Jockey","Charger","Witch","Tank"};int z=special(r);if(z>0&&z<=8)return std::string("class:")+names[z-1];
 if((r.className=="CPhysicsProp"||r.className=="CPhysicsPropMultiplayer")&&r.modelName.find("gnome")!=std::string::npos)return "class:CGnome";
 return "class:"+r.className;}
inline float headLift(const inventory::Record& r){if(special(r)==8)return 96;if(special(r)==7)return 32;if(special(r)||r.character)return 48;if(r.className=="Infected")return 40;return 0;}
inline std::vector<Type> catalogue(){std::vector<Type> out;
 for(auto name:{"Tank","Smoker","Boomer","Hunter","Spitter","Jockey","Charger","Witch","Infected"})out.push_back({std::string("class:")+name,name,0});
 const char* survivors[]={"Nick","Rochelle","Coach","Ellis","Bill","Zoey","Francis","Louis"};for(int i=0;i<8;++i)out.push_back({"survivor:"+std::to_string(i),survivors[i],1});
 for(auto name:{"CGnome","CColaBottles","CGasCan","COxygenTank","CPropaneTank","CFireworkCrate","CPistol","CMagnumPistol","CSubMachinegun","CSMG_Silenced","CSMG_MP5","CPumpShotgun","CShotgun_Chrome","CAutoShotgun","CShotgun_SPAS","CAssaultRifle","CRifle_Desert","CRifle_AK47","CRifle_SG552","CRifle_M60","CSniperRifle","CSniper_Military","CSniper_AWP","CSniper_Scout","CGrenadeLauncher","CChainsaw","CTerrorMeleeWeapon","CFirstAidKit","CPainPills","CItem_Adrenaline","CItemDefibrillator","CItemUpgradePackExplosive","CItemUpgradePackIncendiary","CItem_VomitJar","CPipeBomb","CMolotov","CWeaponSpawn","CWeaponAmmoSpawn","CWeaponScavengeItemSpawn","CBaseUpgradeItem"})out.push_back({std::string("class:")+name,std::string(name).substr(1),2});
 for(auto name:{"CPhysicsProp","CPhysicsPropMultiplayer","CDynamicProp","CBreakableProp","CFuncTrackTrain","CPropJeep","CPropVehicleDriveable","CPropVehicleChoreoGeneric","CPipeBombProjectile","CMolotovProjectile","CVomitJarProjectile","CGrenadeLauncher_Projectile"})out.push_back({std::string("class:")+name,name,3});return out;}
inline void discover(std::vector<Type>& types,const inventory::Frame& frame,const AutoFollowConfig& config){
 auto add=[&](const std::string& key,const std::string& label){if(types.size()>=512||std::any_of(types.begin(),types.end(),[&](const auto& t){return t.key==key;}))return;types.push_back({key,label,3});};
 for(const auto& r:frame.records)add(semanticKey(r),r.className);for(const auto& key:config.rules)add(key,key);
}
inline const inventory::Record* aimed(const inventory::Frame& frame,const GameCameraFrame& camera,const CameraPose& eye,uint64_t tick){
 if(!camera.active||frame.epoch!=camera.epoch||tick<frame.tick||tick-frame.tick>600)return nullptr;const inventory::Record* chosen=nullptr;float best=60.5f;
 for(const auto& r:frame.records){if(!alive(r))continue;auto p=follow::pose(frame,r.handle);if(!p.ready)continue;auto point=scene::point(p.matrix,{});auto ray=point-eye.eye;float distance=length(ray);if(distance<48||distance>1200)continue;Vec3 clip;if(!transform(camera.worldToClip,point,clip,true)||std::abs(clip.x)>.98f||std::abs(clip.y)>.98f||clip.z<0||clip.z>1)continue;float cosine=(ray.x*eye.forward.x+ray.y*eye.forward.y+ray.z*eye.forward.z)/distance;float score=(1-cosine)*1000+distance*.0001f;if(score<best){best=score;chosen=&r;}}
 return chosen;
}
struct Selector {
 uint32_t epoch=0;std::set<int> gnomeModels;
 inventory::FollowRequest choose(const inventory::Frame& f,const AutoFollowConfig& config,const CameraPose& view,uint32_t map,uint64_t tick){
  if(map!=epoch){epoch=map;gnomeModels.clear();}if(!config.enabled||config.rules.empty()||f.epoch!=map||tick<f.tick||tick-f.tick>600)return {};
  for(const auto& r:f.records)if(r.className=="CGnome"&&r.worldModelIndex)gnomeModels.insert(*r.worldModelIndex);
  float best=config.range;uint32_t handle=0xffffffffu;
  for(const auto& r:f.records){if(!alive(r))continue;bool matches=autoRule(config,semanticKey(r));
   if(!matches&&autoRule(config,"class:CGnome")&&(r.className=="CPhysicsProp"||r.className=="CPhysicsPropMultiplayer")&&r.isCarryable&&*r.isCarryable&&r.modelIndex)matches=gnomeModels.count(*r.modelIndex)!=0;
   if(!matches)continue;auto p=follow::pose(f,r.handle);if(!p.ready)continue;auto origin=scene::point(p.matrix,{});if(r.character&&r.team&&*r.team==2&&std::hypot(origin.x-view.eye.x,origin.y-view.eye.y)<16&&view.eye.z-origin.z>32&&view.eye.z-origin.z<96)continue;float distance=length(origin-view.eye);
   if(distance<best||(distance==best&&r.handle<handle)){best=distance;handle=r.handle;}
  }return handle==0xffffffffu?inventory::FollowRequest{}:inventory::FollowRequest{true,map,handle};
 }
};
}
