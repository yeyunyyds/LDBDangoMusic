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
struct ScenePoint {uint32_t handle=0;Vec3 center{},aim{},velocity{};int health=0;uint32_t attacker=0;bool hurt=false,incap=false,aimVerified=false,tank=false;};
struct BirdScene {
 uint32_t epoch=0,subject=0,combatHandle=0,pendingHandle=0;
 uint64_t sampled=0,combatAt=0,pendingAt=0;
 std::vector<ScenePoint> team,enemies;
 Vec3 center{},direction{},combat{};
 float engagement=0,advance=0;bool tankFocus=false;
 void reset(){*this={};}
 template<class Sight> void sample(uint64_t now,const Actor& actor,const inventory::Frame& f,Sight sight){
  if(epoch!=actor.epoch||subject!=actor.handle){reset();epoch=actor.epoch;subject=actor.handle;}
  auto oldTeam=std::move(team),oldEnemies=std::move(enemies);team.clear();enemies.clear();
  const float dt=sampled&&now>sampled&&now-sampled<1000?float(now-sampled)*.001f:0;
  sampled=now;
  auto make=[&](const inventory::Record& r,const std::vector<ScenePoint>& prior){
   ScenePoint p;p.handle=r.handle;p.center=*r.networkOrigin+Vec3{0,0,38};p.health=*r.health;p.incap=r.incapacitated.value_or(false);p.attacker=r.controlAttacker.value_or(0);p.tank=r.className=="Tank"||r.zombieClass.value_or(0)==8;
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
  if(length(movement)>45.f){auto wanted=normalize(movement);if(length(direction)<.5f)direction=wanted;else direction=headingVector(mixAngle(std::atan2(direction.y,direction.x)*180.f/pi,std::atan2(wanted.y,wanted.x)*180.f/pi,.20f));}
  else if(length(direction)<.5f)direction=headingVector(actor.yaw);
  float best=0,incumbent=0;uint32_t selected=0;Vec3 selectedCenter{},incumbentCenter{};
  std::sort(enemies.begin(),enemies.end(),[&](const auto& a,const auto& b){return length(a.center-center)<length(b.center-center);});
  unsigned rays=0;
  for(const auto& e:enemies){
   float score=0;
   for(const auto& p:team){
    auto delta=e.center-p.center;float d=length(delta);if(d>950.f||d<1.f)continue;
    auto dir=normalize(delta);float aim=wall::dot(p.aim,dir),approach=wall::dot(horizontal(e.velocity),horizontal(dir)*-1.f);
    bool aimed=aim>(p.aimVerified?.82f:.94f);const bool pinned=p.attacker==e.handle&&p.attacker!=0;
    if(!(pinned||(aimed&&(p.aimVerified||d<450.f||e.hurt||p.hurt))||(d<230.f&&(approach>45.f||e.hurt||p.hurt||p.incap))||(e.hurt&&d<650.f)))continue;
    if(rays>=32)continue;
    ++rays;if(!sight(p.center,e.center)&&!pinned)continue;
    float s=15.f+35.f*(1-d/950.f)+(aimed?(p.aimVerified?35.f:16.f):0.f)+(e.hurt?22.f:0.f)+(p.hurt?14.f:0.f)+(p.incap?12.f:0.f)+(approach>45.f?10.f:0.f)+(pinned?70.f:0.f);
    score=std::max(score,s+(e.tank?24.f:0.f));
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
   combatHandle=selected;combatAt=now;tankFocus=false;for(const auto& e:enemies)if(e.handle==selected)tankFocus=e.tank;
   Vec3 sum=selectedCenter*2.f;float n=2;
   for(const auto& e:enemies)if(e.handle!=selected&&e.hurt&&length(e.center-selectedCenter)<240.f){sum=sum+e.center;n+=1;}
   combat=sum*(1/n);engagement=std::clamp(best/80.f,.45f,1.f);
  }else if(!combatAt||now-combatAt>900){combatHandle=0;engagement=0;tankFocus=false;}
 }
 Vec3 focus(const Actor& actor)const{
  auto c=sampled?center:actor.center;
  auto d=length(direction)>.5f?direction:headingVector(actor.yaw);
  auto ahead=c+d*(advance*std::clamp(length(horizontal(actor.velocity))*.8f,100.f,230.f));
  return engagement>0?mix(ahead,c+limited(combat-c,600.f)*(tankFocus?.85f:.7f),engagement):ahead;
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
enum class BirdMove {Hold,Track,Slow,Fast};
inline float birdFastProbability(unsigned streak){return std::ldexp(1.f,-int(std::min(streak,24u)));}
struct BirdCadence {
 unsigned streak=0;uint32_t random=0x6d2b79f5u;
 bool fast(float draw)const{return draw<birdFastProbability(streak);}
 float next(){random^=random<<13;random^=random>>17;random^=random<<5;return float(random>>8)*(1.f/16777216.f);}
 void usedFast(){streak=std::min(streak+1,24u);}
 void alternative(){streak=0;}
};
struct BirdDecision {Pose pose{};bool cut=false,native=false;const char* event=nullptr;float speedCap=180.f;};
struct BirdDirector {
 BirdScene scene;
 Vec3 target{},focus{},anchorActor{},anchorEye{};
 uint64_t nextPlan=0,nextCheck=0,holdUntil=0,farSince=0,lastCut=0,nextFollow=0,nextSight=0,hiddenSince=0,trackSince=0;
 unsigned failures=0;float farBest=0;bool moving=false,initialized=false,anchored=false,visible=true,recovering=false;
 BirdMove mode=BirdMove::Hold;BirdCadence cadence;
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
  value+=6.f*std::clamp((eye.z-center.z)/480.f,0.f,1.f);
  value+=8.f*std::max(0.f,-wall::dot(normalize(horizontal(eye-center)),dir));
  // Coverage alone can reward a camera in front looking backward. Prefer
  // a view looking along travel, or along the active ally-to-threat axis.
  auto view=normalize(horizontal(aim-eye));
  value+=20.f*scene.advance*(1.f-scene.engagement)*wall::dot(view,dir);
  if(scene.engagement>0)value+=14.f*scene.engagement*wall::dot(view,normalize(horizontal(scene.combat-center)));
  // Avoid parking at extreme distance when the main subjects are tiny.
  value-=std::max(0.f,length(eye-center)-750.f)*.025f;
  value-=std::min(12.f,length(eye-origin)/100.f);
  return value;
 }
 // Keep the combat composition independent of actor-follow translation.
 Vec3 framedFocus(Vec3 eye,Vec3 wanted,const Actor& actor,BirdLens lens)const{
  if(birdFrame(eye,wanted,actor.center,lens,.82f))return wanted;
  for(float t:{.75f,.5f,.25f}){auto p=mix(actor.center,wanted,t);if(birdFrame(eye,p,actor.center,lens,.82f))return p;}
  return actor.center;
 }
 template<class Clear,class Sight,class Space> BirdDecision update(uint64_t now,float dt,Pose current,const Actor& actor,BirdLens lens,bool blocked,Clear clear,Sight sight,Space space){
  BirdDecision out;out.pose=current;
  if(!initialized){initialized=true;target=current.eye;focus=scene.focus(actor);nextPlan=now;cadence.random^=uint32_t(now)^actor.handle;if(!cadence.random)cadence.random=1;}
  const auto desiredFocus=scene.focus(actor);
  focus=mix(focus,desiredFocus,1.f-std::exp(-std::clamp(dt,0.f,.1f)*(scene.engagement>0?2.8f:1.4f)));
  const float distance=length(current.eye-actor.center);
  if(now>=nextSight){
   nextSight=now+100;visible=sight(current.eye,actor.center);
   if(!visible&&scene.engagement>0&&sight(current.eye,scene.combat)&&birdFrame(current.eye,desiredFocus,scene.combat,lens))
    for(const auto& p:scene.team)if(p.handle!=actor.handle&&birdFrame(current.eye,desiredFocus,p.center,lens)&&sight(current.eye,p.center)){visible=true;break;}
   if(visible)hiddenSince=0;else if(!hiddenSince)hiddenSince=now;
  }
  const auto hidden=hiddenSince?now-hiddenSince:0;
  const bool recover=hidden>=1500;
  if(recover&&!recovering)nextPlan=now;
  recovering=recover;
  if(distance>1000.f){if(!farSince||distance<farBest-120.f){farSince=now;farBest=distance;}}else{farSince=0;farBest=0;}
  const bool cutReady=!lastCut||now-lastCut>=2000;
  const bool emergencyCut=cutReady&&(distance>2200.f||(farSince&&now-farSince>=2500)||hidden>=5000);
  const float actorSpeed=length(horizontal(actor.velocity));
  auto setAnchor=[&](Vec3 eye){anchorEye=eye;anchorActor=actor.center;anchored=true;};
  auto hold=[&](){mode=BirdMove::Hold;moving=false;target=current.eye;trackSince=0;};
  auto cutTo=[&](Vec3 eye,const char* event){lastCut=now;farSince=0;hiddenSince=0;nextSight=now;visible=true;recovering=false;setAnchor(eye);mode=BirdMove::Hold;moving=false;target=eye;focus=desiredFocus;nextFollow=now+100;nextPlan=now+750;holdUntil=now+750;cadence.alternative();trackSince=0;out.pose=look(eye,framedFocus(eye,focus,actor,lens),current.yaw);out.cut=true;out.event=event;};
  if(moving&&blocked){hold();anchored=false;nextPlan=now;out.event="path-invalid";}
  if(mode==BirdMove::Slow||mode==BirdMove::Fast){
   if(length(target-current.eye)<12.f){setAnchor(target);hold();holdUntil=now+750;out.event="arrived";}
   else if(now>=nextCheck){nextCheck=now+200;if(!clear(current.eye,target)||length(target-actor.center)>950.f||(recover&&!sight(target,actor.center))){hold();anchored=false;nextPlan=now;out.event="path-invalid";}}
  }
  // Translate the current relative composition by actor displacement, not by
  // combat gaze. Incremental waypoints preserve velocity through the spring.
  if(anchored&&!emergencyCut&&(mode==BirdMove::Hold||mode==BirdMove::Track)&&now>=nextFollow){
   nextFollow=now+100;
   auto delta=actor.center-anchorActor;
   auto original=look(anchorEye,anchorActor,current.yaw);
   auto drift=look(anchorEye,actor.center,current.yaw);
   const float angle=std::max(std::abs(wrap(drift.yaw-original.yaw)),std::abs(drift.pitch-original.pitch));
   if(mode==BirdMove::Track||length(delta)>=24.f||angle>=15.f){
    auto desired=anchorEye+delta+limited(actor.velocity*.24f,70.f);
    auto step=limited(desired-current.eye,96.f);
    if(length(step)>3.f){
     // Try forward translation, then shorter segments or tangential alternatives.
     auto side=normalize(wall::cross(horizontal(step),{0,0,1}));
     bool found=false;
     for(auto offset:{step,step*.5f,step*.25f,step*.5f+side*24.f,step*.5f-side*24.f,step*.5f+Vec3{0,0,24}}){
      auto q=current.eye+offset;
      if(length(offset)<4.f||wall::dot(offset,step)<=0||!clear(current.eye,q))continue;
      if((visible||recover)&&!sight(q,actor.center))continue;
      target=q;found=true;break;
     }
     if(found){if(mode!=BirdMove::Track){trackSince=now;out.event="continuous-track";}mode=BirdMove::Track;moving=true;
      if(trackSince&&now-trackSince>=1500)cadence.alternative();
     }else{hold();nextPlan=std::min(nextPlan,now);}
    }else if(actorSpeed<8.f){setAnchor(current.eye);hold();}
   }
  }
  const bool relocating=mode==BirdMove::Slow||mode==BirdMove::Fast;
  const bool planDue=(now>=nextPlan||(distance>2200.f&&!failures&&!relocating))&&(emergencyCut||(!relocating&&(recover||!anchored||now>=holdUntil)));

  if(planDue){
   const float old=score(current.eye,desiredFocus,current.eye,actor,lens,sight)+space(current.eye)*5.f;
   float best=(emergencyCut||recover||!anchored||distance<95.f)?-1000.f:old+12.f;
   Vec3 chosen=current.eye,landing=current.eye;bool found=false,landed=false,nearFound=false;
   auto center=scene.sampled?scene.center:actor.center;
   auto dir=length(scene.direction)>.5f?scene.direction:headingVector(actor.yaw);
   float heading=std::atan2(dir.y,dir.x);
   struct Offset {float r,h;};
   const Offset offsets[]={{400,480},{400,320},{400,220},{260,120},{260,60},{160,20},{100,20},{620,120}};
   struct Candidate {Vec3 eye;float value;};std::vector<Candidate> candidates;
   auto add=[&](Vec3 eye){
    if(length(eye-current.eye)<(recover?8.f:80.f)||!sight(eye,actor.center))return;
    float v=score(eye,desiredFocus,current.eye,actor,lens,sight);
    if(recover)v-=std::min(20.f,length(eye-current.eye)/20.f);
    if(v+5.f>best)candidates.push_back({eye,v});
   };
   // A nearby clear route is preferable to an unnecessary cut.
   if(recover)for(float d:{48.f,96.f,160.f})for(Vec3 axis:{Vec3{1,0,0},Vec3{-1,0,0},Vec3{0,1,0},Vec3{0,-1,0},Vec3{0,0,1}})add(current.eye+axis*d);
   for(const auto& o:offsets)for(float a:{pi,pi*.66f,-pi*.66f,0.f})add(center+Vec3{std::cos(heading+a)*o.r,std::sin(heading+a)*o.r,o.h});
   std::sort(candidates.begin(),candidates.end(),[&](const auto& a,const auto& b){
    if(recover){const bool an=length(a.eye-current.eye)<=180.f,bn=length(b.eye-current.eye)<=180.f;if(an!=bn)return an;}
    return a.value>b.value;
   });
   unsigned nearChecked=0,otherChecked=0;float landingScore=-1000.f;
   for(const auto& candidate:candidates){
    const bool nearCandidate=recover&&length(candidate.eye-current.eye)<=180.f;
    if(nearCandidate?(++nearChecked>6):(++otherChecked>8))continue;
    if(!clear(candidate.eye,candidate.eye)||!clear(candidate.eye,actor.center))continue;
    const float v=candidate.value+space(candidate.eye)*5.f;
    if(v>landingScore){landing=candidate.eye;landingScore=v;landed=true;}
    if(((!nearFound&&nearCandidate)||((nearCandidate||!nearFound)&&v>best))&&clear(current.eye,candidate.eye)){chosen=candidate.eye;best=v;found=true;nearFound=nearCandidate;}
   }
   if(emergencyCut&&landed&&(!found||length(chosen-current.eye)>320.f)){cutTo(landing,"recovery-cut");return out;}
   if(found){
    const float travel=length(chosen-current.eye);
    const bool longMove=travel>320.f;
    const bool allowFast=longMove&&cadence.fast(cadence.next());
    if(longMove&&!allowFast&&cutReady&&landed&&hidden==0&&length(landing-current.eye)>320.f){cutTo(landing,"cadence-cut");return out;}
    target=chosen;nextCheck=now+200;nextPlan=now+1000;failures=0;moving=true;trackSince=0;
    if(allowFast){mode=BirdMove::Fast;cadence.usedFast();out.event="fast-reposition";}
    else{mode=BirdMove::Slow;cadence.alternative();out.event="smooth-reposition";}
   }else{
    failures=std::min(3u,failures+1);nextPlan=now+(recover?500:1000*failures);
    if(mode==BirdMove::Hold)out.event="hold";
    if(distance>1000.f&&farSince&&now-farSince>=6500){out.native=true;out.event="distant-recovery-exhausted";}
   }
  }
  out.pose=look(current.eye,framedFocus(current.eye,focus,actor,lens),current.yaw);out.pose.eye=moving?target:current.eye;
  out.speedCap=mode==BirdMove::Fast?650.f:mode==BirdMove::Track?std::clamp(actorSpeed+60.f,90.f,330.f):180.f;
  return out;
 }
};
}
