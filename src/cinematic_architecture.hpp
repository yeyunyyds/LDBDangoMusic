#pragma once
#include "source_trace_contract.hpp"
namespace dango::cinematic {
// Approximate a roof edge using the facade plane and a bounded world trace
// search. This is a geometric heuristic, not a complete building mesh.
template<class Cast> bool roofPanel(const trace::Result& facade,uint32_t epoch,Cast cast,wall::Panel& out){
 if(!trace::wallHit(facade)||std::abs(facade.plane.normal.z)>.25f)return false;
 Vec3 normal=normalize({facade.plane.normal.x,facade.plane.normal.y,0}),right=normalize(wall::cross({0,0,1},normal));
 if(length(normal)<.9f||length(right)<.9f)return false;
 const Vec3 base=facade.end;float lower=0,upper=0;
 // Verify that the same facade continues upward and find its first free edge.
 for(float z=48;z<=672;z+=48){
  auto p=base+Vec3{0,0,z};trace::Result r;
  if(!cast(p+normal*20,p-normal*24,r)||r.startSolid||r.allSolid)return false;
  if(r.fraction>=.999f){upper=z;break;}
  if(!trace::wallHit(r)||wall::dot(normal,r.plane.normal)<.92f||length(r.end-p)>8)return false;
  lower=z;
 }
 if(upper==0)return false;
 for(int i=0;i<4;++i){
  float z=(lower+upper)*.5f;auto p=base+Vec3{0,0,z};trace::Result r;
  if(!cast(p+normal*20,p-normal*24,r)||r.startSolid||r.allSolid)return false;
  if(r.fraction>=.999f)upper=z;
  else{if(!trace::wallHit(r)||wall::dot(normal,r.plane.normal)<.92f||length(r.end-p)>8)return false;lower=z;}
 }
 // Require upward-facing support behind the edge, at center and both sides.
 // Reject open sky, tall ceilings, ambiguous corners or unrelated surfaces.
 float roof=0;for(float x:{0.f,-54.f,54.f}){
  auto p=base+right*x-normal*26+Vec3{0,0,upper};trace::Result r;
  if(!cast(p+Vec3{0,0,54},p-Vec3{0,0,42},r)||!trace::wallHit(r)||r.plane.normal.z<.65f||std::abs(r.end.z-p.z)>30)return false;
  if(x==0)roof=r.end.z;else if(std::abs(roof-r.end.z)>20)return false;else roof=std::max(roof,r.end.z);
 }
 Vec3 center=base-normal*18;center.z=roof+52;
 if(!wall::makePanel(center,normal,220,60,epoch,out,0))return false;
 // Text plane is above the roof, not on its wall. Verify open air across it.
 for(float y:{0.f,.5f,1.f})for(float x:{0.f,.5f,1.f}){
  auto p=wall::point(out,x,y);trace::Result r;
  if(!cast(p+normal*8,p-normal*8,r)||r.startSolid||r.allSolid||r.fraction<.999f)return false;
 }
 return true;
}
}
