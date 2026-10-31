#pragma once
#include "entity_inventory.hpp"
#include "scene_attachment.hpp"
namespace dango::follow {
enum class Filter {Objects,Physics,Dynamic,Players,Projectiles,GroundWeapons,CommonInfected,SpecialInfected,Vehicles};
inline constexpr unsigned FilterCount=9;
inline const char* filterName(Filter f){const char* n[]={u8"可挂载物体",u8"物理物体",u8"动态物体",u8"玩家",u8"投掷物",u8"地上武器/物品",u8"普通感染者",u8"特殊感染者",u8"车辆实体"};return n[int(f)];}
// Exact network classes from the supported client DLL. Do not use a broad
// weapon-name prefix: projectiles, viewmodels and weapon effects are not pickups.
inline bool weaponClass(const inventory::Record& r){
 static constexpr const char* names[]={
  "CPistol","CMagnumPistol","CSubMachinegun","CSMG_Silenced","CSMG_MP5",
  "CPumpShotgun","CShotgun_Chrome","CAutoShotgun","CShotgun_SPAS",
  "CAssaultRifle","CRifle_Desert","CRifle_AK47","CRifle_SG552","CRifle_M60",
  "CSniperRifle","CSniper_Military","CSniper_AWP","CSniper_Scout",
  "CGrenadeLauncher","CChainsaw","CTerrorMeleeWeapon",
  "CFirstAidKit","CPainPills","CItem_Adrenaline","CItemDefibrillator",
  "CItemUpgradePackExplosive","CItemUpgradePackIncendiary","CItem_VomitJar","CPipeBomb","CMolotov",
  "CGasCan","CColaBottles","CGnome","COxygenTank","CPropaneTank","CFireworkCrate",
  "CWeaponSpawn","CWeaponAmmoSpawn","CWeaponScavengeItemSpawn","CBaseUpgradeItem"};
 for(auto name:names)if(r.className==name)return true;return false;
}
inline bool owned(const inventory::Record& r){return (r.owner&&*r.owner!=0xffffffffu)||(r.weaponOwner&&*r.weaponOwner!=0xffffffffu);}
inline bool carryable(const inventory::Record& r){
 return r.className=="CGnome"||r.className=="CGasCan"||r.className=="CColaBottles"||r.className=="COxygenTank"||r.className=="CPropaneTank"||r.className=="CFireworkCrate";
}
// Carry items can retain a thrower/owner after release. Ownership alone does
// not prove they are still held. Require a fresh owner snapshot and an active
// weapon different from this item before trusting its free-standing world pose.
inline bool held(const inventory::Frame& frame,const inventory::Record& r){
 if(!owned(r))return false;if(!carryable(r))return true;
 if(!r.parent||*r.parent!=0xffffffffu||!r.networkOrigin||!scene::finite(*r.networkOrigin)||length(*r.networkOrigin)<.001f)return true;
 bool checked=false;
 for(auto owner:{r.owner,r.weaponOwner}){if(!owner||*owner==0xffffffffu)continue;
 auto actor=std::find_if(frame.records.begin(),frame.records.end(),[&](const auto& v){return v.handle==*owner;});
 if(actor==frame.records.end()||!actor->dormant||*actor->dormant||!actor->activeWeapon)return true;
 if(actor->className!="CTerrorPlayer"&&actor->className!="SurvivorBot"&&actor->className!="NextBotCombatCharacter")return true;
 if(*actor->activeWeapon==r.handle)return true;checked=true;
 }return !checked;
}
inline bool classMatches(const inventory::Record& r,Filter f,const inventory::Frame* frame=nullptr){
 bool common=r.className=="Infected";
 bool special=r.className=="Smoker"||r.className=="Boomer"||r.className=="Hunter"||r.className=="Spitter"||r.className=="Jockey"||r.className=="Charger"||r.className=="Tank"||r.className=="Witch"||((r.className=="CTerrorPlayer"||r.className=="NextBotCombatCharacter")&&r.team&&*r.team==3);
 bool player=(r.className=="CTerrorPlayer"&&!special)||r.className=="SurvivorBot";
 bool vehicle=r.className=="CPropJeep"||r.className=="CPropVehicleChoreoGeneric"||r.className=="CPropVehicleDriveable"||r.className=="CFuncTrackTrain";
 bool physics=r.className=="CPhysicsProp"||r.className=="CPhysicsPropMultiplayer";
 bool dynamic=r.className=="CDynamicProp"||r.className=="CBreakableProp"||r.className=="CFuncTrackTrain";
 bool projectile=r.className.find("Projectile")!=std::string::npos||r.className=="CBaseGrenade";
 bool weapon=weaponClass(r);
 if(weapon&&owned(r)&&(!frame||held(*frame,r)))return false;
 switch(f){case Filter::Physics:return physics;case Filter::Dynamic:return dynamic;case Filter::Players:return player;case Filter::Projectiles:return projectile;case Filter::GroundWeapons:return weapon;case Filter::CommonInfected:return common;case Filter::SpecialInfected:return special;case Filter::Vehicles:return vehicle;default:return physics||dynamic||player||projectile||weapon||common||special||vehicle;}
}
inline scene::Transform transform(Vec3 origin,Vec3 angles){
 constexpr float rad=.01745329252f;float sp=std::sin(angles.x*rad),cp=std::cos(angles.x*rad),sy=std::sin(angles.y*rad),cy=std::cos(angles.y*rad),sr=std::sin(angles.z*rad),cr=std::cos(angles.z*rad);
 return {cp*cy,sr*sp*cy-cr*sy,cr*sp*cy+sr*sy,origin.x,cp*sy,sr*sp*sy+cr*cy,cr*sp*sy-sr*cy,origin.y,-sp,sr*cp,cr*cp,origin.z};
}
inline const inventory::Record* find(const inventory::Frame& frame,uint32_t handle){for(auto& r:frame.records)if(r.handle==handle)return &r;return nullptr;}
struct Pose {bool ready=false;scene::Transform matrix=scene::identity();std::string status=u8"目标已消失";};
inline Pose pose(const inventory::Frame& f,uint32_t handle,std::vector<uint32_t> chain={}){
 if(chain.size()>=16||std::find(chain.begin(),chain.end(),handle)!=chain.end())return {false,{},u8"父级关系无效"};chain.push_back(handle);
 auto r=find(f,handle);if(!r)return {};if(r->effects&&(*r->effects&32))return {false,{},u8"目标暂不可见"};if(!r->dormant||*r->dormant)return {false,{},u8"目标休眠或状态未知"};
 if(!r->networkOrigin||!r->networkAngles||!r->parent)return {false,{},u8"网络姿态或父级信息缺失"};
 if(held(f,*r)&&(weaponClass(*r)||(*r->parent==0xffffffffu&&length(*r->networkOrigin)<.001f)))return {false,{},u8"目标已被拿起，需要渲染挂点"};
 if(!scene::finite(*r->networkOrigin)||!scene::finite(*r->networkAngles))return {false,{},u8"网络姿态无效"};
 auto local=transform(*r->networkOrigin,*r->networkAngles);
 if(*r->parent!=0xffffffffu){if(!r->parentAttachment||*r->parentAttachment)return {false,{},u8"需要骨骼或附件姿态"};auto parent=pose(f,*r->parent,std::move(chain));if(!parent.ready)return parent;local=scene::multiply(parent.matrix,local);}
 if(!scene::valid(local))return {false,{},u8"目标变换无效"};return {true,local,u8"正在跟随（网络姿态）"};
}
inline uint32_t effectiveHandle(const inventory::Frame& f,const inventory::FollowRequest& request){return f.requestedFollowHandle==request.handle&&f.followedHandle!=0xffffffffu?f.followedHandle:request.handle;}
inline Pose resolve(const inventory::Frame& f,const inventory::FollowRequest& request,uint32_t epoch,uint64_t tick){
 if(!request.enabled)return {false,{},u8"跟随未开启"};if(!epoch||request.epoch!=epoch||f.epoch!=epoch)return {false,{},u8"地图已变化，请重新锁定"};
 if(!f.tick||tick<f.tick||tick-f.tick>250)return {false,{},u8"等待目标新快照"};return pose(f,effectiveHandle(f,request));
}
struct Candidate {uint32_t handle=0;float score=0;std::string name;};
inline std::vector<Candidate> candidates(const inventory::Frame& f,const GameCameraFrame& camera,const CameraPose& eye,Filter filter,uint64_t tick){
 std::vector<Candidate> out;if(!camera.active||f.epoch!=camera.epoch||tick<f.tick||tick-f.tick>500)return out;
 for(auto& r:f.records){if(!classMatches(r,filter,&f))continue;auto p=pose(f,r.handle);if(!p.ready)continue;auto point=scene::point(p.matrix,{}),ray=point-eye.eye;float distance=length(ray);if(distance<48||distance>1200)continue;Vec3 clip;if(!dango::transform(camera.worldToClip,point,clip,true)||std::abs(clip.x)>.98f||std::abs(clip.y)>.98f||clip.z<0||clip.z>1)continue;
 float cosine=(ray.x*eye.forward.x+ray.y*eye.forward.y+ray.z*eye.forward.z)/distance;if(cosine<.5f)continue;out.push_back({r.handle,(1-cosine)*1000+distance*.0001f,r.className});}
 std::sort(out.begin(),out.end(),[](const Candidate& a,const Candidate& b){return a.score==b.score?a.handle<b.handle:a.score<b.score;});return out;
}
struct Settings {bool enabled=false,sample=false;Filter filter=Filter::Objects;Vec3 offset{0,0,32};float size=1;inventory::FollowRequest target;uint64_t revision=0;};
inline float layoutAxis(const Settings& s,int axis){return axis==0?s.offset.y:axis==1?s.offset.z:axis==2?s.offset.x:s.size;}
inline void adjustLayout(Settings& s,int axis,int choice){
 if(axis<0||axis>3||choice<0||choice>4)return;float& value=axis==0?s.offset.y:axis==1?s.offset.z:axis==2?s.offset.x:s.size;
 if(choice==4){value=layoutAxis(Settings{},axis);return;}
 const float position[]={10,1,-1,-10},depth[]={-10,-1,1,10},scale[]={-.25f,-.05f,.05f,.25f};
 value=std::clamp(value+(axis==3?scale[choice]:axis==2?depth[choice]:position[choice]),axis==3?.25f:-512.f,axis==3?3.f:512.f);
}
inline bool useEntityRoute(const Settings& s,uint32_t epoch){return s.enabled&&s.target.enabled&&s.target.epoch==epoch;}
inline Vec3 anchor(const Pose& p,const Settings& s){return scene::point(p.matrix,s.offset);}
}
