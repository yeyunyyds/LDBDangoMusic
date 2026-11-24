#pragma once
#include "entity_inventory.hpp"
namespace dango::cinematic {
// Only current, validated survivors; never reuse a recycled entity index.
inline bool validSubject(const inventory::Record& r){
 return (r.className=="CTerrorPlayer"||r.className=="SurvivorBot")&&r.team&&*r.team==2&&r.health&&*r.health>0&&r.lifeState&&*r.lifeState==0&&r.dormant&&!*r.dormant&&r.networkOrigin&&r.networkAngles;
}
inline const inventory::Record* subject(const inventory::Frame& f,uint32_t local,uint32_t previous,std::optional<int> character){
 const inventory::Record* self=nullptr;
 for(const auto& r:f.records)if(r.index==local){self=&r;break;}
 if(self&&validSubject(*self))return self;
 // Prefer the previous full handle. After a takeover changes the handle,
 // a unique survivor-character match is the continuity evidence.
 for(const auto& r:f.records)if(r.handle==previous&&validSubject(r))return &r;
 if(character){
  const inventory::Record* match=nullptr;bool ambiguous=false;
  for(const auto& r:f.records)if(validSubject(r)&&r.character==character){if(match)ambiguous=true;match=&r;}
  if(match&&!ambiguous)return match;
 }
 // An actual observer target is a validated fallback, not a guessed index.
 if(self&&self->observerTarget)for(const auto& r:f.records)
  if(r.handle==*self->observerTarget&&validSubject(r))return &r;
 return nullptr;
}
}
