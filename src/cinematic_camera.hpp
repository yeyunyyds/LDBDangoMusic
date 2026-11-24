#pragma once
#include "wall_panel.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>

namespace dango::cinematic {
constexpr float pi=3.14159265358979323846f;
enum class Style {Mixed,Orbit,Architecture,Wide,Bird};
inline const char* styleName(int n){const char* names[]={u8"混合运镜",u8"人物环绕",u8"建筑歌词",u8"场景远景",u8"鸟瞰追踪"};return names[std::clamp(n,0,4)];}
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
 Actor value{};
 void reset(){value={};}
 Actor sample(uint32_t epoch,uint32_t handle,uint64_t tick,Vec3 center,float yaw){
  if(!wall::finite(center)||!std::isfinite(yaw)){reset();return value;}
  Actor next{epoch,handle,tick,center,{},yaw,true};
  if(value.ready&&value.epoch==epoch&&value.handle==handle){
   if(tick<=value.tick)return value;
   float dt=float(tick-value.tick)*.001f;auto delta=center-value.center;
   if(dt<=.5f&&length(delta)<std::max(96.f,dt*1800.f)){
    auto raw=delta*(1/std::max(dt,.001f));
    next.velocity=mix(value.velocity,raw,1-std::exp(-dt*7.f));
   }
  }
  value=next;return value;
 }
};
struct Composition {uint32_t epoch=0;uint64_t tick=0,serial=0;bool ready=false;wall::Panel first{},second{};bool hasSecond=false;};
template<class Timeline> unsigned duration(const Timeline& lines,std::int64_t song,double rate){
 if(song<0||!std::isfinite(rate)||rate<=0)return 6000;
 for(const auto& line:lines){if(line.start<=song)continue;double ms=double(line.start-song)/rate;if(ms>=4000&&ms<=10000)return unsigned(ms);if(ms>10000)break;}
 return 6000;
}
inline Style choose(Style requested,float speed,unsigned sequence,bool architecture){
 if(requested!=Style::Mixed)return requested;
 if(speed>190.f)return Style::Bird;
 const Style cycle[]={Style::Orbit,Style::Architecture,Style::Wide,Style::Orbit,Style::Bird};
 auto style=cycle[sequence%5];return style==Style::Architecture&&!architecture?Style::Wide:style;
}
inline Vec3 quadratic(Vec3 a,Vec3 b,Vec3 c,float t){return mix(mix(a,b,t),mix(b,c,t),t);}
struct Director {
 Pose from{},to{},last{};
 Vec3 control{},anchor{},focus{};
 uint64_t begin=0,lastTick=0;
 unsigned span=6000,sequence=0;
 Style active=Style::Orbit,selected=Style::Mixed;
 bool ready=false,planned=false,moving=false;
 void reset(){*this={};}
 bool start(Pose pose){if(!valid(pose))return false;reset();ready=true;last=from=to=pose;return true;}
 bool needsPlan(uint64_t now,const Actor& actor,Style requested)const{
  if(!planned||requested!=selected||now<begin||now-begin>=span)return true;
  if(!actor.ready)return false;
  // Update world reference at every shot boundary; fast-moving players may
  // force an earlier continuous transition, not an instantaneous teleport.
  return now-begin>1400&&length(actor.center-anchor)>(active==Style::Bird?240.f:320.f);
 }
 template<class Clear> bool plan(uint64_t now,unsigned milliseconds,const Actor& actor,Style requested,const Composition& comp,Clear clear){
  if(!actor.ready)return false;
  from=last;anchor=actor.center;selected=requested;active=choose(requested,length(actor.velocity),sequence,comp.ready);
  const unsigned n=sequence++;begin=now;span=std::clamp(milliseconds,4000u,10000u);planned=true;moving=false;
  const float heading=(length(Vec3{actor.velocity.x,actor.velocity.y,0})>40?std::atan2(actor.velocity.y,actor.velocity.x):actor.yaw*pi/180.f);
  Vec3 direction{std::cos(heading),std::sin(heading),0};
  Vec3 predicted=actor.center;
  if(active==Style::Bird){auto v=actor.velocity;float speed=length(v);if(speed>600)v=v*(600/speed);predicted=predicted+v*.6f;}
  focus=predicted;
  if(active==Style::Architecture&&comp.ready)focus=mix(actor.center,comp.first.center,.32f);
  const float radius=active==Style::Orbit?325.f:active==Style::Wide?620.f:active==Style::Bird?400.f:390.f;
  const float height=active==Style::Orbit?100.f:active==Style::Wide?260.f:active==Style::Bird?540.f:115.f;
  float initial=std::atan2(from.eye.y-anchor.y,from.eye.x-anchor.x);
  if(length(Vec3{from.eye.x-anchor.x,from.eye.y-anchor.y,0})<70)initial=heading+pi;
  if(active==Style::Bird)initial=heading+pi;
  if(active==Style::Architecture&&comp.ready)initial=std::atan2(comp.first.normal.y,comp.first.normal.x);
  const float sign=n%2?1.f:-1.f;
  const float angles[]={sign*.95f,-sign*.75f,sign*1.65f,0.f};
  for(float scale:{1.f,.65f,.4f})for(float angle:angles){
   const float az=initial+angle;
   Vec3 radial{std::cos(az),std::sin(az),0};
   Vec3 candidate=predicted+radial*(radius*scale)+Vec3{0,0,height*scale};
   if(length(candidate-from.eye)>1800.f||length(candidate-actor.center)<100.f)continue;
   // Rounded orbit/slide with a verified four-chord path; if curved travel
   // is blocked, try the straight segment to the same composition.
   Vec3 mid=mix(from.eye,candidate,.5f);
   Vec3 bend=mid+radial*(active==Style::Orbit?radius*scale*.22f:25.f*scale);
   if(!clear(candidate,actor.center))continue;
   if(active==Style::Architecture&&comp.ready){
    if(wall::dot(comp.first.normal,candidate-comp.first.center)<12.f||!clear(candidate,comp.first.center))continue;
    if(comp.hasSecond&&(wall::dot(comp.second.normal,candidate-comp.second.center)<12.f||!clear(candidate,comp.second.center)))continue;
    auto a=normalize(actor.center-candidate),b=normalize(comp.first.center-candidate);
    if(wall::dot(a,b)<.68f)continue;
   }
   for(auto midPoint:{bend,mid}){
    Vec3 prev=from.eye;bool safe=true;
    for(int i=1;i<=4;++i){auto next=quadratic(from.eye,midPoint,candidate,float(i)/4);if(!clear(prev,next)){safe=false;break;}prev=next;}
    if(!safe)continue;
    control=midPoint;to=look(candidate,focus,from.yaw);moving=true;return true;
   }
  }
  // Never fly through unsupported geometry. No fake free-space orbit.
  to=look(from.eye,focus,from.yaw);control=from.eye;return false;
 }
 Pose step(uint64_t now,const Actor& actor){
  if(!ready||!planned)return last;
  float t=ease(float(now>=begin?now-begin:0)/float(span));
  Vec3 eye=moving?quadratic(from.eye,control,to.eye,t):from.eye;
  Vec3 aim=focus;
  if(actor.ready&&(active==Style::Orbit||active==Style::Bird)){
   Vec3 delta=actor.center-anchor;float distance=length(delta);if(distance>300)delta=delta*(300/distance);
   aim=aim+delta;
  }
  auto target=look(eye,aim,from.yaw);
  // Blend from actual orientation; dynamic tracking remains smooth.
  float rotation=ease(float(now>=begin?now-begin:0)/float(std::min(span,1800u)));
  last={eye,mix(from.pitch,target.pitch,rotation),mixAngle(from.yaw,target.yaw,rotation),mixAngle(from.roll,0,rotation)};
  lastTick=now;return last;
 }
};
}
