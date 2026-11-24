#pragma once
#include "cinematic_camera.hpp"
#include "source_trace_contract.hpp"
#include <vector>
namespace dango::cinematic {
// Eight candidate directions, checked against actual world ground and view.
template<class Cast,class OnScreen> bool findGround(const Actor& actor,Vec3 eye,Cast cast,OnScreen onScreen,wall::Panel& out){
 if(!actor.ready)return false;
 const auto preferred=freePanel(Style::Bird,actor,eye);
 const float offsets[]={0,pi,pi*.5f,-pi*.5f,pi*.25f,-pi*.25f,pi*.75f,-pi*.75f};
 unsigned budget=120;
 auto ray=[&](Vec3 a,Vec3 b,trace::Result& hit){if(!budget)return false;--budget;return cast(a,b,hit);};
 for(float angle:offsets){
  const Vec3 side{preferred.right.x*std::cos(angle)-preferred.right.y*std::sin(angle),preferred.right.x*std::sin(angle)+preferred.right.y*std::cos(angle),0};
  const auto center=actor.center+side*104.f;
  trace::Result hit;
  if(!ray(center+Vec3{0,0,16},center-Vec3{0,0,96},hit)||!trace::wallHit(hit)||hit.plane.normal.z<.85f||std::abs(hit.end.z-(actor.center.z-38.f))>48.f)continue;
  wall::Panel panel;
  if(!wall::makeSurfacePanel(hit.end,hit.plane.normal,180,60,actor.epoch,panel,1,preferred.up))continue;
  if(!onScreen(panel)||!trace::supported(panel,ray)||!trace::visible(panel,eye,ray))continue;
  out=panel;return true;
 }
 return false;
}
struct GroundAnchor {uint64_t serial=0,visibleTick=0,expires=0;int index=-1;bool valid=false,visible=false;wall::Panel panel;};
struct GroundTrack {
 std::vector<GroundAnchor> anchors;uint64_t serial=0;int index=-1;bool attempted=false;
 void reset(){anchors.clear();index=-1;attempted=false;}
 template<class Find> void update(int next,uint64_t now,unsigned exitMs,bool bird,Find find){
  if(next!=index){
   for(auto& a:anchors)if(!a.expires)a.expires=now+exitMs;
   index=next;attempted=false;
  }
  anchors.erase(std::remove_if(anchors.begin(),anchors.end(),[&](const auto& a){return a.expires&&now>=a.expires;}),anchors.end());
  if(!bird||next<0||attempted)return;
  attempted=true;
  anchors.erase(std::remove_if(anchors.begin(),anchors.end(),[&](const auto& a){return a.index==next;}),anchors.end());
  if(anchors.size()>=32)anchors.erase(anchors.begin());
  GroundAnchor a;a.serial=++serial;a.index=next;a.valid=find(a.panel);a.visible=a.valid;a.visibleTick=now;anchors.push_back(a);
 }
 template<class Cast> void refresh(uint64_t now,Vec3 eye,Cast cast){
  for(auto& a:anchors)if(a.valid&&now>=a.visibleTick+100){a.visible=trace::visible(a.panel,eye,cast);a.visibleTick=now;}
 }
};
}
