#pragma once
#include "entity_follow.hpp"
#include <set>
namespace dango::follow {
// Bridge the physical prop and carried weapon representations. No nearest-
// object fallback: each hop requires matching world-model IDs and a unique
// candidate, with a live holder relation for the pickup hop.
struct GnomeBridge {
 uint32_t source=0xffffffffu,current=0xffffffffu,epoch=0,holder=0xffffffffu;
 int model=-1;bool armed=false,gnome=false,havePosition=false;
 uint64_t missingSince=0;Vec3 position{};std::set<uint32_t> baseline;
 std::string status="direct";
 void begin(const inventory::FollowRequest& request){if(source!=request.handle||epoch!=request.epoch){*this={};source=current=request.handle;epoch=request.epoch;}}
 static bool physical(const inventory::Record& r){return (r.className=="CPhysicsProp"||r.className=="CPhysicsPropMultiplayer")&&r.isCarryable&&*r.isCarryable;}
 static bool supported(const inventory::Record& r){return r.className=="CGnome"||physical(r);}
 bool needsScan(const inventory::Frame& f,uint64_t tick)const{
  auto r=find(f,current);if(!armed)return r&&supported(*r);
  if(r&&r->className=="CGnome"&&carrier(f,*r))return false;
  return (!r||!pose(f,current).ready)&&(!missingSince||tick-missingSince<=5000);
 }
 void remember(const inventory::Frame& f){baseline.clear();for(const auto& r:f.records)if(supported(r))baseline.insert(r.handle);}
 const inventory::Record* carrier(const inventory::Frame& f,const inventory::Record& item)const{
  const inventory::Record* result=nullptr;
  for(auto h:{item.owner,item.weaponOwner}){if(!h||*h==0xffffffffu)continue;auto r=find(f,*h);
   if(!r||!r->dormant||*r->dormant||!r->activeWeapon||*r->activeWeapon!=item.handle)continue;
   if((r->health&&*r->health<=0)||(r->lifeState&&*r->lifeState!=0))continue;
   if(r->className!="CTerrorPlayer"&&r->className!="SurvivorBot")continue;
   if(result&&result->handle!=r->handle)return nullptr;result=r;
  }return result;
 }
 void step(const inventory::Frame& f,uint64_t tick,bool full){
  if(f.epoch!=epoch||tick<f.tick||tick-f.tick>250)return;
  auto r=find(f,current);
  if(!armed){if(!r||!supported(*r)||!full)return;
   auto id=r->className=="CGnome"?r->worldModelIndex:r->modelIndex;
   if(!id||*id<=0){status="gnome: world model unavailable";return;}
   model=*id;gnome=r->className=="CGnome";armed=true;remember(f);
  }
  if(r&&r->className=="CGnome"){
   gnome=true;if(r->worldModelIndex&&*r->worldModelIndex>0)model=*r->worldModelIndex;
   if(auto actor=carrier(f,*r)){holder=actor->handle;auto p=pose(f,holder);if(p.ready){position=scene::point(p.matrix,{});havePosition=true;}missingSince=0;status="gnome: carried; lock retained";return;}
  }
  if(r){auto p=pose(f,current);if(p.ready){position=scene::point(p.matrix,{});havePosition=true;missingSince=0;status=gnome?"gnome: released; tracking":"direct";return;}}
  if(!missingSince)missingSince=tick;
  if(tick-missingSince>5000){status="gnome: transition expired; lock retained";return;}
  if(!full||!havePosition)return;
  std::vector<const inventory::Record*> matches;
  // A new carried CGnome must have a matching world model and be explicitly
  // active on a nearby living player's full handle.
  for(const auto& candidate:f.records){if(candidate.className!="CGnome"||baseline.count(candidate.handle)||!candidate.worldModelIndex||*candidate.worldModelIndex!=model)continue;
   auto actor=carrier(f,candidate);if(!actor)continue;auto p=pose(f,actor->handle);if(!p.ready||length(scene::point(p.matrix,{})-position)>192)continue;matches.push_back(&candidate);
  }
  if(matches.size()>1){status="gnome: ambiguous pickup; waiting";return;}
  if(matches.size()==1){current=matches[0]->handle;holder=carrier(f,*matches[0])->handle;gnome=true;missingSince=0;remember(f);status="gnome: prop to carried weapon";return;}
  if(!gnome||holder==0xffffffffu)return;
  // After the held weapon vanishes, only a newly appearing free prop (or the
  // original exact prop handle) can resume the lock. Old lookalikes are ignored.
  auto actorPose=pose(f,holder);Vec3 releaseOrigin=actorPose.ready?scene::point(actorPose.matrix,{}):position;
  for(const auto& candidate:f.records){if(!physical(candidate)||!candidate.modelIndex||*candidate.modelIndex!=model||(baseline.count(candidate.handle)&&candidate.handle!=source))continue;
   auto p=pose(f,candidate.handle);if(!p.ready||length(scene::point(p.matrix,{})-releaseOrigin)>256)continue;matches.push_back(&candidate);
  }
  if(matches.size()!=1){status=matches.empty()?"gnome: waiting for released prop":"gnome: ambiguous release; waiting";return;}
  current=matches[0]->handle;missingSince=0;position=scene::point(pose(f,current).matrix,{});remember(f);status="gnome: carried weapon to prop";
 }
};
}
