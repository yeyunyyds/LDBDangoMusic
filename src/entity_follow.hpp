#pragma once
#include "entity_inventory.hpp"
#include "scene_attachment.hpp"
namespace dango::follow {
enum class Filter {Objects,Physics,Dynamic,Players,Projectiles,GroundWeapons};
inline const char* filterName(Filter f){const char* n[]={u8"可挂载物体",u8"物理物体",u8"动态物体",u8"玩家",u8"投掷物",u8"地上武器"};return n[int(f)];}
inline bool weaponClass(const inventory::Record& r){return r.className=="CPistol"||r.className=="CPistolMagnum"||r.className=="CTerrorMeleeWeapon"||r.className.find("CRifle")==0||r.className.find("CShotgun")==0||r.className.find("CSMG")==0||r.className=="CFirstAidKit";}
inline bool classMatches(const inventory::Record& r,Filter f){
 bool player=r.className=="CTerrorPlayer"||r.className=="SurvivorBot";
 bool physics=r.className=="CPhysicsProp"||r.className=="CPhysicsPropMultiplayer";
 bool dynamic=r.className=="CDynamicProp"||r.className=="CBreakableProp"||r.className=="CFuncTrackTrain";
 bool projectile=r.className.find("Projectile")!=std::string::npos||r.className=="CBaseGrenade";
 bool weapon=weaponClass(r);
 if(weapon&&r.owner&&*r.owner!=0xffffffffu)return false;
 switch(f){case Filter::Physics:return physics;case Filter::Dynamic:return dynamic;case Filter::Players:return player;case Filter::Projectiles:return projectile;case Filter::GroundWeapons:return weapon;default:return physics||dynamic||player||projectile||weapon;}
}
inline scene::Transform transform(Vec3 origin,Vec3 angles){
 constexpr float rad=.01745329252f;float sp=std::sin(angles.x*rad),cp=std::cos(angles.x*rad),sy=std::sin(angles.y*rad),cy=std::cos(angles.y*rad),sr=std::sin(angles.z*rad),cr=std::cos(angles.z*rad);
 return {cp*cy,sr*sp*cy-cr*sy,cr*sp*cy+sr*sy,origin.x,cp*sy,sr*sp*sy+cr*cy,cr*sp*sy-sr*cy,origin.y,-sp,sr*cp,cr*cp,origin.z};
}
inline const inventory::Record* find(const inventory::Frame& frame,uint32_t handle){for(auto& r:frame.records)if(r.handle==handle)return &r;return nullptr;}
struct Pose {bool ready=false;scene::Transform matrix=scene::identity();std::string status=u8"目标已消失";};
inline Pose pose(const inventory::Frame& f,uint32_t handle,std::vector<uint32_t> chain={}){
 if(chain.size()>=16||std::find(chain.begin(),chain.end(),handle)!=chain.end())return {false,{},u8"父级关系无效"};chain.push_back(handle);
 auto r=find(f,handle);if(!r)return {};if(!r->dormant||*r->dormant)return {false,{},u8"目标休眠或状态未知"};
 if(!r->networkOrigin||!r->networkAngles||!r->parent)return {false,{},u8"网络姿态或父级信息缺失"};
 if(r->owner&&*r->owner!=0xffffffffu&&*r->parent==0xffffffffu&&(weaponClass(*r)||length(*r->networkOrigin)<.001f))return {false,{},u8"目标已被拿起，需要渲染挂点"};
 if(!scene::finite(*r->networkOrigin)||!scene::finite(*r->networkAngles))return {false,{},u8"网络姿态无效"};
 auto local=transform(*r->networkOrigin,*r->networkAngles);
 if(*r->parent!=0xffffffffu){if(!r->parentAttachment||*r->parentAttachment)return {false,{},u8"需要骨骼或附件姿态"};auto parent=pose(f,*r->parent,std::move(chain));if(!parent.ready)return parent;local=scene::multiply(parent.matrix,local);}
 if(!scene::valid(local))return {false,{},u8"目标变换无效"};return {true,local,u8"正在跟随（网络姿态）"};
}
inline Pose resolve(const inventory::Frame& f,const inventory::FollowRequest& request,uint32_t epoch,uint64_t tick){
 if(!request.enabled)return {false,{},u8"跟随未开启"};if(!epoch||request.epoch!=epoch||f.epoch!=epoch)return {false,{},u8"地图已变化，请重新锁定"};
 if(!f.tick||tick<f.tick||tick-f.tick>250)return {false,{},u8"等待目标新快照"};return pose(f,request.handle);
}
struct Candidate {uint32_t handle=0;float score=0;std::string name;};
inline std::vector<Candidate> candidates(const inventory::Frame& f,const GameCameraFrame& camera,const CameraPose& eye,Filter filter,uint64_t tick){
 std::vector<Candidate> out;if(!camera.active||f.epoch!=camera.epoch||tick<f.tick||tick-f.tick>500)return out;
 for(auto& r:f.records){if(!classMatches(r,filter))continue;auto p=pose(f,r.handle);if(!p.ready)continue;auto point=scene::point(p.matrix,{}),ray=point-eye.eye;float distance=length(ray);if(distance<48||distance>1200)continue;Vec3 clip;if(!dango::transform(camera.worldToClip,point,clip,true)||std::abs(clip.x)>.98f||std::abs(clip.y)>.98f||clip.z<0||clip.z>1)continue;
 float cosine=(ray.x*eye.forward.x+ray.y*eye.forward.y+ray.z*eye.forward.z)/distance;if(cosine<.5f)continue;out.push_back({r.handle,(1-cosine)*1000+distance*.0001f,r.className});}
 std::sort(out.begin(),out.end(),[](const Candidate& a,const Candidate& b){return a.score==b.score?a.handle<b.handle:a.score<b.score;});return out;
}
struct Settings {bool enabled=false,sample=true;Filter filter=Filter::Objects;Vec3 offset{0,0,32};inventory::FollowRequest target;uint64_t revision=0;};
inline Vec3 anchor(const Pose& p,const Settings& s){return scene::point(p.matrix,s.offset);}
}
