#pragma once
#include "cinematic_camera.hpp"
namespace dango::cinematic {
struct Recovery {
 bool active=false,hasTarget=false;
 uint64_t badSince=0,nextProbe=0,lastCut=0,settledSince=0;
 Vec3 target{};
 void reset(){*this={};}
 enum class Action { Normal, Follow, Hold, Cut, Native };
 template<class Clear,class Visible> Action probe(uint64_t now,Pose current,const Actor& actor,bool healthy,Clear clear,Visible visible){
  if(!actor.ready)return Action::Hold;
  if(now<nextProbe)return active?(hasTarget?Action::Follow:Action::Hold):Action::Normal;
  nextProbe=now+120;
  const float distance=length(current.eye-actor.center);
  if(healthy&&!active)return Action::Normal;
  if(healthy&&active&&distance<330.f){
   hasTarget=false;if(!settledSince)settledSince=now;
   if(now-settledSince>=600){active=false;badSince=settledSince=0;return Action::Normal;}
   return Action::Hold;
  }
  settledSince=0;
  if(!active){active=true;badSince=now;}
  hasTarget=false;
  const auto elapsed=now>=badSince?now-badSince:0;
  const bool cutAllowed=(!lastCut||now-lastCut>=2000)&&((elapsed>=2500&&distance>1000)||elapsed>=5000||distance>2200);
  Vec3 landing{};bool landingReady=false;
  const float heading=actor.yaw*pi/180.f;
  for(float radius:{180.f,112.f,76.f})for(float offset:{pi,pi*.5f,-pi*.5f,0.f,pi*.75f,-pi*.75f}){
   const float az=heading+offset;
   Vec3 eye=actor.center+Vec3{std::cos(az)*radius,std::sin(az)*radius,radius>120?65.f:30.f};
   if(!clear(eye,actor.center)||!visible(eye,actor))continue;
   if(!landingReady){landing=eye;landingReady=true;}
   if(distance>2200&&cutAllowed)break;
   if(clear(current.eye,eye)){target=eye;hasTarget=true;return Action::Follow;}
  }
  if(cutAllowed&&landingReady){target=landing;lastCut=now;active=hasTarget=false;badSince=0;return Action::Cut;}
  if(elapsed>=6500)return Action::Native;
  return Action::Hold;
 }
 Pose desired(Pose current,const Actor& actor) const {
  return look(hasTarget?target:current.eye,actor.center,current.yaw);
 }
 Pose step(Pose current,const Actor& actor,float dt) const {
  auto eye=current.eye;
  if(hasTarget){auto delta=target-eye;float n=length(delta);if(n>.001f)eye=eye+delta*std::min(1.f,720.f*std::clamp(dt,0.f,.1f)/n);}
  auto desired=look(eye,actor.center,current.yaw);float alpha=1-std::exp(-10.f*std::clamp(dt,0.f,.1f));
  return {eye,mix(current.pitch,desired.pitch,alpha),mixAngle(current.yaw,desired.yaw,alpha),mixAngle(current.roll,0,alpha)};
 }
};
}
