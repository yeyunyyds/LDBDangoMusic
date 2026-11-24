#pragma once
#include "world_camera.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace dango::cinematic {
constexpr float pi=3.14159265358979323846f;
inline float wrap(float degrees){return std::remainder(degrees,360.f);}
inline float ease(float t){t=std::clamp(t,0.f,1.f);return t*t*t*(t*(t*6.f-15.f)+10.f);}
inline float mix(float a,float b,float t){return a+(b-a)*t;}
inline float mixAngle(float a,float b,float t){return wrap(a+wrap(b-a)*t);}
struct Pose {Vec3 eye{};float pitch=0,yaw=0,roll=0;};
inline bool valid(Pose p){return std::isfinite(p.eye.x)&&std::isfinite(p.eye.y)&&std::isfinite(p.eye.z)&&std::isfinite(p.pitch)&&std::isfinite(p.yaw)&&std::isfinite(p.roll)&&std::abs(p.pitch)<=89.f;}
inline Pose look(Vec3 eye,Vec3 focus,float fallbackYaw){
 const Vec3 delta=focus-eye;
 const float horizontal=std::hypot(delta.x,delta.y);
 return {eye,std::clamp(std::atan2(-delta.z,std::max(horizontal,.01f))*180.f/pi,-18.f,18.f),
  horizontal>.01f?wrap(std::atan2(delta.y,delta.x)*180.f/pi):fallbackYaw,0.f};
}
// Choose a phrase boundary, not a beat estimate. Group short lines into one
// continuous shot; otherwise use the ambient six-second cadence.
template<class Timeline>
unsigned duration(const Timeline& lines,std::int64_t song,double rate){
 if(song<0||!std::isfinite(rate)||rate<=0)return 6000;
 for(const auto& line:lines){
  if(line.start<=song)continue;
  const auto realMs=double(line.start-song)/rate;
  if(realMs>=4000&&realMs<=10000)return static_cast<unsigned>(realMs);
  if(realMs>10000)break;
 }
 return 6000;
}
struct Director {
 Pose from{},to{},last{};
 Vec3 anchor{},forward{},right{},focus{};
 std::uint64_t begin=0;
 unsigned span=6000,sequence=0;
 bool ready=false,planned=false;
 void reset(){ready=false;planned=false;sequence=0;}
 bool start(Pose pose){
  if(!valid(pose))return false;
  reset();ready=true;last=from=to=pose;anchor=pose.eye;
  // Derive heading from yaw only: downward-facing players do not aim the
  // cinematic at the floor. Start at the actual pose, then level smoothly.
  const float yaw=pose.yaw*pi/180.f;
  forward={std::cos(yaw),std::sin(yaw),0};right={std::sin(yaw),-std::cos(yaw),0};
  focus=anchor+forward*180.f+Vec3{0,0,5};
  return true;
 }
 template<class Clear> void plan(std::uint64_t now,unsigned milliseconds,Clear clear){
  from=last;span=std::clamp(milliseconds,4000u,10000u);begin=now;planned=true;
  // Fixed anchor and varied, small paths; no unbounded walking of the origin.
  static constexpr float offsets[][3]={{-24,30,10},{12,-34,0},{-36,-18,15},{10,24,-3},{0,0,5}};
  const unsigned index=sequence++;
  to=look(from.eye,focus,from.yaw);
  bool found=false;
  for(unsigned i=0;i<5;++i){
   const auto& o=offsets[(index+i)%5];
   const Vec3 candidate=anchor+forward*o[0]+right*o[1]+Vec3{0,0,o[2]};
   if(clear(from.eye,candidate)){to=look(candidate,focus,from.yaw);found=true;break;}
  }
  if(!found){
   // Unsupported/blocked geometry: do not move the camera through walls.
   const float base=std::atan2(forward.y,forward.x)*180.f/pi;
   to={from.eye,-3.f,wrap(base+(index%2?12.f:-12.f)),0.f};
  }
 }
 template<class Clear> Pose step(std::uint64_t now,unsigned milliseconds,Clear clear){
  if(!ready)return {};
  if(!planned||now<begin||now-begin>=span){
   if(planned&&now>=begin&&now-begin>=span)last=to;
   plan(now,milliseconds,clear);
  }
  const float elapsed=float(now-begin);
  const float t=ease(elapsed/float(span));
  const float rot=ease(elapsed/float(std::abs(from.pitch)>30.f?std::min(span,1600u):span));
  last={from.eye+(to.eye-from.eye)*t,mix(from.pitch,to.pitch,rot),mixAngle(from.yaw,to.yaw,rot),mixAngle(from.roll,to.roll,rot)};
  return last;
 }
};
} // namespace dango::cinematic
