#pragma once
#include "cinematic_camera.hpp"
#include "entity_inventory.hpp"
#include <vector>
namespace dango::cinematic {
inline Vec3 horizontal(Vec3 v){v.z=0;return v;}
inline Vec3 headingVector(float yaw){return {std::cos(yaw*pi/180.f),std::sin(yaw*pi/180.f),0};}
inline bool sceneAlive(const inventory::Record& r){
 return r.networkOrigin&&wall::finite(*r.networkOrigin)&&r.dormant&&!*r.dormant&&r.lifeState&&*r.lifeState==0&&r.health&&*r.health>0;
}
inline bool sceneSurvivor(const inventory::Record& r){return sceneAlive(r)&&r.team&&*r.team==2&&(r.className=="CTerrorPlayer"||r.className=="SurvivorBot");}
inline bool sceneSpecial(const inventory::Record& r){
 if(!sceneAlive(r)||r.ghost.value_or(false)||(r.team&&*r.team!=3))return false;
 if(r.className=="CTerrorPlayer")return r.team&&*r.team==3&&r.zombieClass&&*r.zombieClass>=1&&*r.zombieClass<=8&&*r.zombieClass!=7;
 return r.className=="Hunter"||r.className=="Smoker"||r.className=="Boomer"||r.className=="Spitter"||r.className=="Jockey"||r.className=="Charger"||r.className=="Tank"||r.className=="Witch";
}
struct ScenePoint {uint32_t handle=0;Vec3 center{},aim{},velocity{};int health=0;bool hurt=false,incap=false,aimVerified=false;};
struct BirdScene {
 uint32_t epoch=0,subject=0,combatHandle=0,pendingHandle=0;
 uint64_t sampled=0,combatAt=0,pendingAt=0;
 std::vector<ScenePoint> team,enemies;
 Vec3 center{},direction{},combat{};
 float engagement=0,advance=0;
 void reset(){*this={};}
 template<class Sight> void sample(uint64_t now,const Actor& actor,const inventory::Frame& f,Sight sight){
  if(epoch!=actor.epoch||subject!=actor.handle){reset();epoch=actor.epoch;subject=actor.handle;}
  auto oldTeam=std::move(team),oldEnemies=std::move(enemies);team.clear();enemies.clear();
  const float dt=sampled&&now>sampled&&now-sampled<1000?float(now-sampled)*.001f:0;
  sampled=now;
  auto make=[&](const inventory::Record& r,const std::vector<ScenePoint>& prior){
   ScenePoint p;p.handle=r.handle;p.center=*r.networkOrigin+Vec3{0,0,38};p.health=*r.health;p.incap=r.incapacitated.value_or(false);
   if(r.aimAngles){auto a=*r.aimAngles;float pitch=a.x*pi/180.f;p.aim=headingVector(a.y)*std::cos(pitch)+Vec3{0,0,-std::sin(pitch)};p.aimVerified=true;}
   else if(r.networkAngles&&wall::finite(*r.networkAngles))p.aim=headingVector(r.networkAngles->y);
   for(const auto& old:prior)if(old.handle==p.handle&&dt>0){p.velocity=limited((p.center-old.center)*(1/dt),450.f);p.hurt=p.health<old.health;break;}
   return p;
  };
  if(f.epoch==actor.epoch)for(const auto& r:f.records){
   if(sceneSurvivor(r)&&length(*r.networkOrigin+Vec3{0,0,38}-actor.center)<650.f&&team.size()<8)team.push_back(make(r,oldTeam));
   else if(sceneSpecial(r)&&length(*r.networkOrigin+Vec3{0,0,38}-actor.center)<1400.f&&enemies.size()<32)enemies.push_back(make(r,oldEnemies));
  }
  if(std::none_of(team.begin(),team.end(),[&](const auto& p){return p.handle==actor.handle;})){
   ScenePoint p;p.handle=actor.handle;p.center=actor.center;p.velocity=actor.velocity;p.aim=headingVector(actor.yaw);team.push_back(p);
  }
  center=actor.center*1.5f;float weights=1.5f;Vec3 movement=horizontal(actor.velocity)*1.5f;
  for(const auto& p:team){if(p.handle==actor.handle)continue;float w=std::max(.15f,1.f-length(p.center-actor.center)/650.f);center=center+p.center*w;weights+=w;movement=movement+horizontal(p.velocity)*w;}
  center=center*(1/weights);advance=mix(advance,std::clamp(length(movement)/weights/120.f,0.f,1.f),.2f);
  if(length(movement)>45.f){auto wanted=normalize(movement);direction=normalize(mix(direction,wanted,.20f));if(length(direction)<.5f)direction=wanted;}
  else if(length(direction)<.5f)direction=headingVector(actor.yaw);
  float best=0,incumbent=0;uint32_t selected=0;Vec3 selectedCenter{},incumbentCenter{};
  std::sort(enemies.begin(),enemies.end(),[&](const auto& a,const auto& b){return length(a.center-center)<length(b.center-center);});
  unsigned rays=0;
  for(const auto& e:enemies){
   float score=0;
   for(const auto& p:team){
    auto delta=e.center-p.center;float d=length(delta);if(d>950.f||d<1.f)continue;
    auto dir=normalize(delta);float aim=wall::dot(p.aim,dir),approach=wall::dot(horizontal(e.velocity),horizontal(dir)*-1.f);
    bool aimed=aim>(p.aimVerified?.82f:.94f);
    if(!(aimed||(d<230.f&&(approach>45.f||e.hurt||p.hurt||p.incap))||(e.hurt&&d<650.f)))continue;
    if(rays>=32)continue;
    ++rays;if(!sight(p.center,e.center))continue;
    float s=15.f+35.f*(1-d/950.f)+(aimed?(p.aimVerified?35.f:16.f):0.f)+(e.hurt?22.f:0.f)+(p.hurt?14.f:0.f)+(p.incap?12.f:0.f)+(approach>45.f?10.f:0.f);
    score=std::max(score,s);
   }
   if(e.handle==combatHandle){incumbent=score;incumbentCenter=e.center;}
   if(score>best){best=score;selected=e.handle;selectedCenter=e.center;}
  }
  if(incumbent>0&&best<incumbent+18.f){selected=combatHandle;selectedCenter=incumbentCenter;best=incumbent;}
  if(selected&&selected!=combatHandle){
   if(pendingHandle!=selected){pendingHandle=selected;pendingAt=now;}
   if(now-pendingAt<300){selected=0;best=0;}
  }else{pendingHandle=0;pendingAt=0;}
  if(selected&&best>=30.f){
   combatHandle=selected;combatAt=now;
   Vec3 sum=selectedCenter*2.f;float n=2;
   for(const auto& e:enemies)if(e.handle!=selected&&e.hurt&&length(e.center-selectedCenter)<240.f){sum=sum+e.center;n+=1;}
   combat=sum*(1/n);engagement=std::clamp(best/80.f,.45f,1.f);
  }else if(!combatAt||now-combatAt>900){combatHandle=0;engagement=0;}
 }
 Vec3 focus(const Actor& actor)const{
  auto c=sampled?center:actor.center;
  auto d=length(direction)>.5f?direction:headingVector(actor.yaw);
  auto ahead=c+d*(advance*std::clamp(length(horizontal(actor.velocity))*.8f,100.f,230.f));
  return engagement>0?mix(ahead,c+limited(combat-c,600.f)*.7f,engagement):ahead;
 }
};
struct BirdLens {float x=1.f,y=.65f;};
inline BirdLens birdLens(const GameCameraFrame& f){
 auto n=[&](unsigned i){return length({f.worldToClip[i],f.worldToClip[i+1],f.worldToClip[i+2]});};
 float w=n(12),x=n(0),y=n(4);
 if(w>1e-5f&&x>1e-5f&&y>1e-5f)return {std::clamp(w/x,.35f,2.f),std::clamp(w/y,.25f,1.5f)};
 return {};
}
inline bool birdFrame(Vec3 eye,Vec3 focus,Vec3 point,BirdLens lens,float margin=.9f){
 auto forward=normalize(focus-eye);auto right=normalize(wall::cross(forward,{0,0,1}));if(length(right)<.5f)right={0,1,0};
 auto up=normalize(wall::cross(right,forward));auto delta=point-eye;float depth=wall::dot(delta,forward);
 return depth>8.f&&std::abs(wall::dot(delta,right))<depth*lens.x*margin&&std::abs(wall::dot(delta,up))<depth*lens.y*margin;
}
struct BirdDecision {Pose pose{};bool cut=false;const char* event=nullptr;};
struct BirdDirector {
 BirdScene scene;
 Vec3 target{},focus{};
 uint64_t nextPlan=0,nextCheck=0,holdUntil=0,farSince=0,lastCut=0;
 unsigned failures=0;bool moving=false,initialized=false;
 void reset(){*this={};}
 template<class Sight> float score(Vec3 eye,Vec3 aim,Vec3 origin,const Actor& actor,BirdLens lens,Sight sight)const{
  auto center=scene.sampled?scene.center:actor.center;
  auto dir=length(scene.direction)>.5f?scene.direction:headingVector(actor.yaw);
  auto covered=[&](Vec3 p){return birdFrame(eye,aim,p,lens)&&sight(eye,p);};
  float value=0;
  if(covered(actor.center))value+=18.f;
  unsigned count=0;for(const auto& p:scene.team)if(p.handle!=actor.handle){++count;if(covered(p.center))value+=12.f/std::max(1.f,float(scene.team.size()-1));}
  if(!count)value+=8.f;
  if(covered(center+dir*150.f))value+=24.f*scene.advance;
  if(covered(center+dir*300.f))value+=10.f*scene.advance;
  if(scene.engagement>0&&covered(scene.combat))value+=38.f*scene.engagement;
  value+=6.f*std::clamp((eye.z-center.z)/540.f,0.f,1.f);
  value+=8.f*std::max(0.f,-wall::dot(normalize(horizontal(eye-center)),dir));
  // Avoid parking at extreme distance when the main subjects are tiny.
  value-=std::max(0.f,length(eye-center)-750.f)*.025f;
  value-=std::min(12.f,length(eye-origin)/100.f);
  return value;
 }
 template<class Clear,class Sight,class Space> BirdDecision update(uint64_t now,float dt,Pose current,const Actor& actor,BirdLens lens,bool blocked,Clear clear,Sight sight,Space space){
  BirdDecision out;out.pose=current;
  if(!initialized){initialized=true;target=current.eye;focus=scene.focus(actor);nextPlan=now;}
  Vec3 desiredFocus=scene.focus(actor);
  focus=mix(focus,desiredFocus,1.f-std::exp(-std::clamp(dt,0.f,.1f)*(scene.engagement>0?2.8f:1.4f)));
  const float distance=length(current.eye-actor.center);
  if(distance>1000.f){if(!farSince)farSince=now;}else farSince=0;
  const bool far=(distance>2200.f||(farSince&&now-farSince>=2500))&&(!lastCut||now-lastCut>=2000)&&now>=nextPlan;
  if(moving&&length(target-current.eye)<12.f){moving=false;target=current.eye;holdUntil=now+2000;out.event="arrived";}
  if(moving&&now>=nextCheck){
   nextCheck=now+200;
   if(blocked||!clear(current.eye,target)){moving=false;target=current.eye;nextPlan=now+600;holdUntil=now+600;out.event="path-invalid";}
  }
  if(far||(!moving&&now>=nextPlan&&now>=holdUntil)){
   const float old=score(current.eye,desiredFocus,current.eye,actor,lens,sight)+space(current.eye)*5.f;
   float best=far?-1000.f:old+12.f;
   Vec3 chosen=current.eye;bool found=false;
   auto center=scene.sampled?scene.center:actor.center;
   auto dir=length(scene.direction)>.5f?scene.direction:headingVector(actor.yaw);
   float heading=std::atan2(dir.y,dir.x);
   struct Offset {float r,h;};
   const Offset offsets[]={{400,540},{400,350},{400,220},{260,120},{260,60},{160,20},{100,20},{620,120}};
   struct Candidate {Vec3 eye;float value;};std::vector<Candidate> candidates;
   for(const auto& o:offsets)for(float a:{pi,pi*.66f,-pi*.66f,0.f}){
    Vec3 eye=center+Vec3{std::cos(heading+a)*o.r,std::sin(heading+a)*o.r,o.h};
    if(!far&&length(eye-current.eye)<80.f)continue;
    if(!sight(eye,actor.center))continue;
    float v=score(eye,desiredFocus,current.eye,actor,lens,sight);
    if(v+5.f>best)candidates.push_back({eye,v});
   }
   std::sort(candidates.begin(),candidates.end(),[](const auto& a,const auto& b){return a.value>b.value;});
   unsigned checked=0;
   for(const auto& c:candidates){
    if(++checked>8)break;
    if(!clear(c.eye,c.eye)||!clear(c.eye,actor.center))continue;
    const float v=c.value+space(c.eye)*5.f;
    if(v<=best||(!far&&!clear(current.eye,c.eye)))continue;
    chosen=c.eye;best=v;found=true;
   }
   if(found){failures=0;target=chosen;nextCheck=now+200;nextPlan=now+1000;
    if(far){lastCut=now;farSince=0;moving=false;focus=desiredFocus;holdUntil=now+2000;out.pose=look(chosen,focus,current.yaw);out.cut=true;out.event="distant-cut";return out;}
    moving=true;out.event="reposition";
   }else{failures=std::min(3u,failures+1);nextPlan=now+1000*failures;out.event="hold";}
  }
  out.pose=look(current.eye,focus,current.yaw);out.pose.eye=moving?target:current.eye;
  return out;
 }
};
}
