#pragma once
#include "wall_panel.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>

namespace dango::cinematic {
constexpr float pi=3.14159265358979323846f;
enum class Style {Mixed,Orbit,Architecture,Wide,Bird,Wall};
inline const char* styleName(int n){const char* names[]={u8"混合运镜",u8"人物环绕",u8"建筑歌词",u8"场景远景",u8"鸟瞰追踪",u8"墙面运镜"};return names[std::clamp(n,0,5)];}
inline float wrap(float degrees){return std::remainder(degrees,360.f);}
inline float ease(float t){t=std::clamp(t,0.f,1.f);return t*t*t*(t*(t*6.f-15.f)+10.f);}
inline float mix(float a,float b,float t){return a+(b-a)*t;}
inline float mixAngle(float a,float b,float t){return wrap(a+wrap(b-a)*t);}
inline Vec3 mix(Vec3 a,Vec3 b,float t){return a+(b-a)*t;}
struct Pose {Vec3 eye{};float pitch=0,yaw=0,roll=0;};
inline bool valid(Pose p){return wall::finite(p.eye)&&std::isfinite(p.pitch)&&std::isfinite(p.yaw)&&std::isfinite(p.roll)&&std::abs(p.pitch)<=89.f;}
inline Pose look(Vec3 eye,Vec3 focus,float fallbackYaw){
 const Vec3 d=focus-eye;float h=std::hypot(d.x,d.y);
 return {eye,std::clamp(std::atan2(-d.z,std::max(h,.01f))*180.f/pi,-85.f,85.f),h>.01f?wrap(std::atan2(d.y,d.x)*180.f/pi):fallbackYaw,0};
}
struct Actor {uint32_t epoch=0,handle=0;uint64_t tick=0;Vec3 center{},velocity{};float yaw=0;bool ready=false;};
struct Tracker {
 Actor value{};Vec3 previousRaw{};
 void reset(){value={};previousRaw={};}
 Actor sample(uint32_t epoch,uint32_t handle,uint64_t tick,Vec3 center,float yaw){
  if(!wall::finite(center)||!std::isfinite(yaw)){reset();return value;}
  Actor next{epoch,handle,tick,center,{},yaw,true};
  if(value.ready&&value.epoch==epoch&&value.handle==handle){
   if(tick<=value.tick)return value;
   float dt=float(tick-value.tick)*.001f;auto delta=center-previousRaw;
   if(dt<=.5f&&length(delta)<std::max(96.f,dt*1800.f)){
    auto raw=delta*(1/std::max(dt,.001f));
    next.velocity=mix(value.velocity,raw,1-std::exp(-dt*7.f));
    next.center=mix(value.center,center,1-std::exp(-dt*12.f));
   }
  }
  previousRaw=center;value=next;return value;
 }
};
struct Composition {uint32_t epoch=0;uint64_t tick=0,serial=0;bool ready=false;wall::Panel first{},second{};bool hasSecond=false;};
template<class Timeline> unsigned duration(const Timeline& lines,std::int64_t song,double rate){
 if(song<0||!std::isfinite(rate)||rate<=0)return 6000;
 for(const auto& line:lines){if(line.start<=song)continue;double ms=double(line.start-song)/rate;if(ms>=4000&&ms<=10000)return unsigned(ms);if(ms>10000)break;}
 return 6000;
}
inline bool surfaceStyle(Style s){return s==Style::Architecture||s==Style::Wall;}
constexpr unsigned allStyles=31u;
inline unsigned styleBit(Style style){const unsigned n=unsigned(style);return n>=1&&n<=5?1u<<(n-1):0u;}
inline bool allowedStyle(Style style,unsigned mask){return (styleBit(style)&mask)!=0;}
inline Style choose(Style requested,float speed,unsigned sequence,bool architecture,unsigned mask=allStyles){
 if(requested!=Style::Mixed)return requested;
 if(speed>190.f&&allowedStyle(Style::Bird,mask))return Style::Bird;
 const Style cycle[]={Style::Orbit,Style::Architecture,Style::Wide,Style::Wall,Style::Bird};
 for(unsigned i=0;i<5;++i){auto style=cycle[(sequence+i)%5];if(allowedStyle(style,mask)&&(architecture||!surfaceStyle(style)))return style;}
 return Style::Mixed;
}
inline unsigned nextSequence(unsigned sequence,Style chosen){
 const Style cycle[]={Style::Orbit,Style::Architecture,Style::Wide,Style::Wall,Style::Bird};
 for(unsigned i=0;i<5;++i)if(cycle[(sequence+i)%5]==chosen)return sequence+i+1;
 return sequence+1;
}
inline wall::Panel freePanel(Style style,const Actor& actor,Vec3 eye,bool alternate=false){
 auto normal=normalize(Vec3{eye.x-actor.center.x,eye.y-actor.center.y,0});
 if(length(normal)<.9f)normal={-std::cos(actor.yaw*pi/180.f),-std::sin(actor.yaw*pi/180.f),0};
 auto right=normalize(wall::cross(Vec3{0,0,1},normal));
 if(style==Style::Bird){
  Vec3 up=normal*-1.f;auto side=normalize(wall::cross(up,{0,0,1}));
  return {actor.center+side*104.f+Vec3{0,0,-22},{0,0,1},side,up,180,60,actor.epoch};
 }
 const bool wide=style==Style::Wide||style==Style::Architecture;
 return {actor.center+right*((wide?170.f:100.f)*(alternate?-1.f:1.f))+Vec3{0,0,wide?80.f:34.f},normal,right,{0,0,1},wide?270.f:150.f,wide?85.f:50.f,actor.epoch};
}
// The render clock interpolates sampled actor motion without adding engine reads.
struct RenderActor {
 Actor value{};double tick=0;
 void reset(){*this={};}
 Actor update(const Actor& sample,double now){
  if(!sample.ready){reset();return value;}
  if(!value.ready||value.epoch!=sample.epoch||value.handle!=sample.handle||now<tick||now-tick>500||length(sample.center-value.center)>128){
   value=sample;tick=now;return value;
  }
  const float dt=float(now-tick)*.001f;
  auto center=mix(value.center,sample.center,1-std::exp(-dt*24.f));
  const auto velocity=mix(value.velocity,sample.velocity,1-std::exp(-dt*12.f));
  const float yaw=mixAngle(value.yaw,sample.yaw,1-std::exp(-dt*18.f));
  value=sample;value.center=center;value.velocity=velocity;value.yaw=yaw;tick=now;return value;
 }
};
enum class LyricMode {Subtitle,First,Second};
// Select once when a new lyric lease begins, not on each render/visibility tick.
inline LyricMode chooseLyricMode(bool preferSecond,bool firstVisible,bool secondVisible){
 if(preferSecond&&secondVisible)return LyricMode::Second;
 if(firstVisible)return LyricMode::First;
 return secondVisible?LyricMode::Second:LyricMode::Subtitle;
}
inline Vec3 limited(Vec3 v,float maximum){const float n=length(v);return n>maximum&&n>0?v*(maximum/n):v;}
inline Vec3 leadPosition(const Actor& actor,float seconds){return actor.center+limited(Vec3{actor.velocity.x,actor.velocity.y,0}*seconds,420.f);}
// Exact critically damped integration for a held target; retain velocity across shots.
inline Vec3 spring(Vec3 current,Vec3 target,Vec3& velocity,float dt,float omega,float cap){
 if(dt<=0)return current;
 const auto error=current-target;const auto term=velocity+error*omega;
 const float decay=std::exp(-omega*dt);
 const auto next=target+(error+term*dt)*decay;
 velocity=limited((velocity-term*(omega*dt))*decay,cap);
 return current+limited(next-current,cap*dt);
}
struct MotionController {
 Vec3 linear{},angular{};
 void reset(){*this={};}
 void blocked(){linear={};}
 Pose step(Pose current,Pose target,float dt,bool rapid=false,bool interceptShot=false,float speedCap=900.f,bool cinematicTranslation=false){
  dt=std::clamp(dt,0.f,.05f);
  const float distance=length(target.eye-current.eye);
  const float omega=rapid?10.f:interceptShot?9.f:distance>220.f?7.f:5.f;
  Vec3 eye;
  if(cinematicTranslation){
   // Limit the actual velocity change as well as the requested cruising speed.
   // Decelerate from a fast shot instead of snapping directly to the slow cap.
   const float accel=speedCap<=50.f?90.f:speedCap<=180.f?240.f:550.f;
   const float brake=length(linear)>speedCap?350.f:accel;
   const float desiredSpeed=std::min(speedCap,std::sqrt(2.f*brake*distance));
   const auto wanted=limited((target.eye-current.eye)*(speedCap<=50.f?2.f:5.f),desiredSpeed);
   linear=linear+limited(wanted-linear,brake*dt);
   eye=current.eye+linear*dt;
  }else eye=spring(current.eye,target.eye,linear,dt,omega,std::min(speedCap,rapid?720.f:900.f));
  Vec3 angles{current.pitch,current.yaw,current.roll};
  Vec3 desired{target.pitch,current.yaw+wrap(target.yaw-current.yaw),current.roll+wrap(target.roll-current.roll)};
  auto out=spring(angles,desired,angular,dt,10.f,180.f);
  return {eye,std::clamp(out.x,-85.f,85.f),wrap(out.y),wrap(out.z)};
 }
};
inline bool healthyView(bool visible,float distance,float range){return visible&&distance<range;}
struct RecoveryGate {
 uint64_t unhealthySince=0;
 void reset(){unhealthySince=0;}
 bool healthy(uint64_t now,bool visible,float distance,float range,bool recovering){
  if(healthyView(visible,distance,range)){reset();return true;}
  if(recovering||distance>=range+250.f)return false;
  if(!unhealthySince||now<unhealthySince)unhealthySince=now;
  return now-unhealthySince<360;
 }
};
inline Vec3 quadratic(Vec3 a,Vec3 b,Vec3 c,float t){return mix(mix(a,b,t),mix(b,c,t),t);}
// Brief, independently planned departure from the initial survivor eye.
// Static world clearance is checked before translating; no blind teleport.
struct Departure {
 Pose from{},to{};double begin=0;bool active=false;
 static constexpr unsigned milliseconds=300;
 template<class Clear> bool plan(double now,Pose p,const Actor& actor,Clear clear){
  active=false;if(!actor.ready||!valid(p))return false;
  auto delta=p.eye-actor.center;
  if(std::hypot(delta.x,delta.y)>=48.f||std::abs(delta.z)>70.f)return false;
  const float yaw=p.yaw*pi/180.f;
  Vec3 f{std::cos(yaw),std::sin(yaw),0},side{-f.y,f.x,0};
  for(float distance:{80.f,64.f})for(Vec3 dir:{f,f*-1.f,side,side*-1.f}){
   auto eye=p.eye+dir*distance;auto radial=eye-actor.center;
   if(std::hypot(radial.x,radial.y)<60.f||!clear(p.eye,eye))continue;
   from=to=p;to.eye=eye;begin=now;active=true;return true;
  }
  return false;
 }
 Pose step(double now){
  const auto elapsed=now>=begin?now-begin:0;
  Pose p=from;p.eye=mix(from.eye,to.eye,ease(float(elapsed)/milliseconds));
  if(elapsed>=milliseconds){p=to;active=false;}return p;
 }
};
struct Director {
 Pose from{},to{},last{};
 Vec3 control{},anchor{},focus{},intercept{};
 double begin=0,lastTick=0;uint32_t actorEpoch=0,actorHandle=0;
 unsigned span=6000,sequence=0,selectedMask=allStyles;float travelMs=6000,plannedSpeed=0;
 Style active=Style::Orbit,selected=Style::Mixed;
 bool ready=false,planned=false,moving=false,departureChecked=false,lyricFocus=false,anticipating=false,fixedSurface=false;
 Departure departure;
 void reset(){*this={};}
 bool start(Pose pose){if(!valid(pose))return false;reset();ready=true;last=from=to=pose;return true;}
 bool rebase(Pose pose){const auto n=sequence;if(!start(pose))return false;sequence=n;departureChecked=true;return true;}
 void beginBird(double now,unsigned milliseconds,const Actor& actor,Style requested,unsigned mask){
  selected=requested;selectedMask=mask;active=Style::Bird;actorEpoch=actor.epoch;actorHandle=actor.handle;
  sequence=nextSequence(sequence,active);begin=now;span=std::clamp(milliseconds,4000u,10000u);planned=moving=true;anticipating=fixedSurface=false;
 }
 bool needsPlan(double now,const Actor& actor,Style requested,unsigned mask=allStyles)const{
  if(!planned||requested!=selected||(requested==Style::Mixed&&mask!=selectedMask)||actor.epoch!=actorEpoch||actor.handle!=actorHandle||now<begin||now-begin>=span)return true;
  if(!actor.ready||active==Style::Bird)return false;
  // Update world reference at every shot boundary; fast-moving players may
  // force an earlier continuous transition, not an instantaneous teleport.
  const float speed=std::hypot(actor.velocity.x,actor.velocity.y);
  if(!fixedSurface&&!anticipating&&plannedSpeed<100.f&&speed>130.f&&now-begin>=1200)return true;
  if(anticipating&&now-begin>=700){
   const auto direction=intercept-anchor;const Vec3 velocity{actor.velocity.x,actor.velocity.y,0};
   if(speed>100.f&&wall::dot(normalize(direction),normalize(velocity))<.25f)return true;
   if(now-begin>=travelMs+450){
    auto away=actor.center-intercept;
    if(length(away)>240.f||wall::dot(away,direction)>length(direction)*100.f)return true;
   }
  }
  if(!moving)return now-begin>=1200;
  return fixedSurface&&now-begin>=700&&length(actor.center-anchor)>160.f;
 }
 template<class Clear> bool plan(double now,unsigned milliseconds,const Actor& actor,Style requested,const Composition& comp,Clear clear,bool lyrics=false,unsigned mask=allStyles){
  if(!actor.ready)return false;
  lyricFocus=lyrics;from=last;anchor=actor.center;selected=requested;selectedMask=mask;active=choose(requested,length(actor.velocity),sequence,comp.ready,mask);actorEpoch=actor.epoch;actorHandle=actor.handle;
  if(surfaceStyle(active)&&!comp.ready&&requested!=Style::Mixed)active=Style::Wide;
  const unsigned n=sequence;sequence=nextSequence(sequence,active);begin=now;span=std::clamp(milliseconds,4000u,10000u);travelMs=float(span);plannedSpeed=std::hypot(actor.velocity.x,actor.velocity.y);planned=true;moving=false;anticipating=false;fixedSurface=surfaceStyle(active)&&comp.ready;
  if(active==Style::Mixed){focus=actor.center;to=look(from.eye,focus,from.yaw);control=from.eye;return false;}
  const float heading=(length(Vec3{actor.velocity.x,actor.velocity.y,0})>40?std::atan2(actor.velocity.y,actor.velocity.x):actor.yaw*pi/180.f);
  Vec3 predicted=actor.center;
  focus=predicted;
  if(surfaceStyle(active)&&comp.ready)focus=mix(actor.center,comp.hasSecond?mix(comp.first.center,comp.second.center,.5f):comp.first.center,active==Style::Architecture?.52f:.40f);
  const float radius=active==Style::Orbit?325.f:active==Style::Wide?620.f:active==Style::Bird?400.f:active==Style::Wall?275.f:620.f;
  const float height=active==Style::Orbit?100.f:active==Style::Wide?260.f:active==Style::Bird?480.f:active==Style::Wall?45.f:340.f;
  float initial=std::atan2(from.eye.y-anchor.y,from.eye.x-anchor.x);
  if(length(Vec3{from.eye.x-anchor.x,from.eye.y-anchor.y,0})<70)initial=heading+pi;
  if(active==Style::Bird)initial=heading+pi;
  if(surfaceStyle(active)&&comp.ready)initial=std::atan2(comp.first.normal.y,comp.first.normal.x);
  const float sign=active==Style::Orbit?1.f:(n%2?1.f:-1.f);
  // Bounded forward intercept: arrive early and hold rather than drag the camera along.
  if(!surfaceStyle(active)&&std::hypot(actor.velocity.x,actor.velocity.y)>=100.f){
   constexpr float leadSeconds=1.65f;
   auto future=leadPosition(actor,leadSeconds);
   if(length(future-actor.center)>=120.f&&clear(actor.center,future)){
    for(float side:{sign*.85f,-sign*.85f}){
     const float az=heading+side;Vec3 radial{std::cos(az),std::sin(az),0};
     auto candidate=future+radial*(radius*.65f)+Vec3{0,0,height*.65f};
     if(length(candidate-from.eye)>leadSeconds*400.f||length(candidate-actor.center)<100.f)continue;
     if(!clear(candidate,actor.center)||!clear(candidate,future))continue;
     auto midpoint=mix(from.eye,candidate,.5f)+radial*25.f;
     bool safe=true;auto prev=from.eye;
     for(int i=1;i<=4;++i){auto next=quadratic(from.eye,midpoint,candidate,float(i)/4);if(!clear(prev,next)){safe=false;break;}prev=next;}
     if(!safe)continue;
     control=midpoint;intercept=future;focus=future;to=look(candidate,future,from.yaw);
     travelMs=1050.f;span=4000;anticipating=moving=true;return true;
    }
   }
  }
  const float angles[]={sign*.95f,-sign*.75f,sign*1.65f,0.f};
  const float facadeAngles[]={sign*.62f,-sign*.62f,sign*.95f,-sign*.95f};
  for(float scale:{1.f,.65f,.4f})for(unsigned ai=0;ai<4;++ai){
   const float angle=surfaceStyle(active)&&comp.ready?facadeAngles[ai]:angles[ai];
   const float az=initial+angle;
   Vec3 radial{std::cos(az),std::sin(az),0};
   Vec3 candidate=predicted+radial*(radius*scale)+Vec3{0,0,height*scale};
   if(surfaceStyle(active)&&comp.ready){
    // Orbit around the joint subject/lyric composition, not around an arbitrary wall ray.
    const auto& panel=comp.first;
    candidate=focus+radial*(radius*scale);
    if(active==Style::Wall)candidate.z=std::clamp(actor.center.z+height*scale,panel.center.z-60.f,panel.center.z+70.f);
    else candidate.z=std::max(actor.center.z+height*scale,panel.center.z+55.f*scale);
   }
   if(length(candidate-from.eye)>1800.f||length(candidate-actor.center)<100.f)continue;
   // Rounded orbit/slide with a verified four-chord path; if curved travel
   // is blocked, try the straight segment to the same composition.
   Vec3 mid=mix(from.eye,candidate,.5f);
   Vec3 bend=mid+radial*(active==Style::Orbit?radius*scale*.22f:25.f*scale);
   if(!clear(candidate,actor.center))continue;
   if(surfaceStyle(active)&&comp.ready){
    auto readable=[&](const wall::Panel& p){
     auto toEye=normalize(candidate-p.center);auto view=normalize(focus-candidate);
     float incidence=wall::dot(p.normal,toEye);float distance=length(candidate-p.center);
     // Modest perspective, enough apparent size, and the whole panel in a conservative FOV.
     if(incidence<.55f||distance<75.f||p.width*incidence/distance<.12f)return false;
     for(float u:{0.f,1.f})for(float v:{0.f,1.f})if(wall::dot(normalize(wall::point(p,u,v)-candidate),view)<.84f)return false;
     return clear(candidate,p.center+p.normal*8.f);
    };
    if(!readable(comp.first)||(comp.hasSecond&&!readable(comp.second)))continue;
    if(wall::dot(normalize(actor.center-candidate),normalize(focus-candidate))<.84f)continue;
   }
   for(auto midPoint:{bend,mid}){
    Vec3 prev=from.eye;bool safe=true;
    for(int i=1;i<=4;++i){auto next=quadratic(from.eye,midPoint,candidate,float(i)/4);if(!clear(prev,next)){safe=false;break;}prev=next;}
    if(!safe)continue;
    control=midPoint;auto aim=lyricFocus&&!surfaceStyle(active)&&active!=Style::Wide?mix(actor.center,freePanel(active,actor,candidate).center,.38f):focus;to=look(candidate,aim,from.yaw);moving=true;return true;
   }
  }
  // Never fly through unsupported geometry. No fake free-space orbit.
  if(length(Vec3{focus.x-from.eye.x,focus.y-from.eye.y,0})<80.f){focus=from.eye+Vec3{std::cos(heading),std::sin(heading),0}*180.f;}
  to=look(from.eye,focus,from.yaw);control=from.eye;return false;
 }
 Pose step(double now,const Actor& actor){
  if(!ready||!planned)return last;
  float t=ease(float(now>=begin?now-begin:0)/travelMs);
  Vec3 eye=moving?quadratic(from.eye,control,to.eye,t):from.eye;
  Vec3 aim=focus;
  if(actor.ready){
   const auto delta=actor.center-anchor;
   if(!fixedSurface&&!anticipating&&moving)eye=eye+delta;
   if(!fixedSurface){
    aim=lyricFocus&&active!=Style::Wide&&active!=Style::Mixed?mix(actor.center,freePanel(active,actor,eye).center,.38f):actor.center;
    if(anticipating)aim=mix(aim,intercept,.35f*(1-t));
   }else aim=aim+delta*.55f;
  }
  auto target=look(eye,aim,from.yaw);
  // Desired pose only; the persistent MotionController smooths all applied poses.
  float rotation=ease(float(now>=begin?now-begin:0)/float(std::min(span,1800u)));
  last={eye,mix(from.pitch,target.pitch,rotation),mixAngle(from.yaw,target.yaw,rotation),mixAngle(from.roll,0,rotation)};
  lastTick=now;return last;
 }
};
}
