#pragma once
#include "source_trace_contract.hpp"
#include <vector>
namespace dango::wallscene {
struct Options {
 bool automatic=true,live=true;int size=1,capacity=6,lingerMs=4000,fadeMs=1000;
 float range=600,spacing=140;
};
struct Ticket {uint64_t session=0,serial=0,tick=0;uint32_t epoch=0;bool active=false;};
struct Item {wall::Panel panel;unsigned id=0;bool visible=false,automatic=false;uint64_t token=0,session=0,born=0,visibleTick=0;int64_t retired=-1;};
inline bool fresh(const Ticket& t,uint64_t now,uint32_t epoch){return t.active&&t.serial&&t.epoch==epoch&&now>=t.tick&&now-t.tick<=1000;}
inline float alpha(const Item& item,uint64_t now,const Options& o){
 if(!item.token)return 0;if(item.retired<0)return 1;
 double age=double(now)-double(item.retired)-o.lingerMs;return float(std::clamp(1-age/std::max(1,o.fadeMs),0.,1.));
}
inline bool onScreen(const wall::Panel& panel,const GameCameraFrame& camera,float width,float height){
 std::array<wall::Vertex,4> corners;if(!wall::projectPanel(panel,camera,width,height,corners))return false;
 float left=corners[0].x,right=left,top=corners[0].y,bottom=top;for(const auto& p:corners){left=std::min(left,p.x);right=std::max(right,p.x);top=std::min(top,p.y);bottom=std::max(bottom,p.y);}
 return right>0&&left<width&&bottom>0&&top<height;
}
inline bool separated(const wall::Panel& candidate,const std::vector<Item>& items,float spacing){
 for(const auto& item:items){auto d=candidate.center-item.panel.center;
 if(length(d)<spacing)return false;
 // Coplanar rectangles must not intersect even when the requested spacing is small.
 if(wall::dot(candidate.normal,item.panel.normal)>.98f&&std::abs(wall::dot(d,candidate.normal))<8&&
 std::abs(wall::dot(d,candidate.right))<(candidate.width+item.panel.width)*.5f+12&&
 std::abs(wall::dot(d,candidate.up))<(candidate.height+item.panel.height)*.5f+8)return false;
 }return true;
}
struct Scene {
 std::vector<Item> items;uint64_t session=0,observed=0,handled=0;unsigned sequence=0,rotation=0;bool active=false;
 void reset(){items.clear();session=observed=handled=0;active=false;rotation=0;}
 void clear(const Ticket& t){items.clear();observed=handled=t.serial;}
 void observe(const Ticket& t,uint64_t now,uint32_t epoch,const Options& o){
 bool valid=fresh(t,now,epoch)&&o.live;
 if(!valid||session!=t.session){
 items.erase(std::remove_if(items.begin(),items.end(),[](const Item& i){return i.automatic;}),items.end());
 for(auto& i:items){i.token=0;i.retired=-1;}observed=handled=0;session=t.session;}
 active=valid;
 if(valid&&observed!=t.serial){for(auto& i:items)if(i.token&&i.retired<0)i.retired=int64_t(now);observed=t.serial;}
 for(auto& i:items)if(i.token&&i.retired>=0&&alpha(i,now,o)<=0&&!i.automatic){i.token=0;i.retired=-1;}
 items.erase(std::remove_if(items.begin(),items.end(),[&](const Item& i){return i.automatic&&i.retired>=0&&alpha(i,now,o)<=0;}),items.end());
 while(items.size()>size_t(o.capacity))items.erase(items.begin());
 }
 bool pending(const Ticket& t)const{return active&&handled!=t.serial;}
 void assign(Item& item,const Ticket& t,uint64_t now){item.token=t.serial;item.session=t.session;item.born=now;item.retired=-1;handled=t.serial;}
 void add(const wall::Panel& panel,bool automatic,const Ticket& t,uint64_t now,const Options& o){
 while(items.size()>=size_t(std::max(1,o.capacity)))items.erase(items.begin());
 Item item;item.panel=panel;item.id=++sequence;item.automatic=automatic;item.visible=true;item.visibleTick=now;
 if(pending(t)){assign(item,t,now);rotation=unsigned(items.size()+1);}items.push_back(item);
 }
 bool assignManual(const Ticket& t,uint64_t now){
 if(!pending(t)||items.empty())return false;
 for(size_t n=0;n<items.size();++n){size_t index=(rotation+n)%items.size();auto& i=items[index];
 if(!i.automatic&&i.visible&&now>=i.visibleTick&&now-i.visibleTick<=350){assign(i,t,now);rotation=unsigned((index+1)%items.size());return true;}}
 return false;
 }
};
// Horizontal search ignores looking down at the road. Alternate sides each
// sentence, then try the forward wall; rescan from the current moving camera.
template<class Cast> bool candidate(const GameCameraFrame& camera,const CameraPose& pose,unsigned attempt,uint64_t serial,const Options& options,const std::vector<Item>& existing,Cast cast,wall::Panel& panel){
 auto forward=normalize(Vec3{pose.forward.x,pose.forward.y,0});if(length(forward)<.9f)forward=normalize(Vec3{pose.up.x,pose.up.y,0});if(length(forward)<.9f)return false;
 auto right=normalize(wall::cross(forward,{0,0,1}));const float angles[]={35,-35,55,-55,0};
 float angle=angles[attempt%5]*(serial%2?1.f:-1.f)*.01745329252f;
 auto direction=forward*std::cos(angle)+right*std::sin(angle);trace::Result hit;
 if(!cast(pose.eye,pose.eye+direction*options.range,hit)||!trace::wallHit(hit))return false;
 const float widths[]={80,120,180},heights[]={28,40,60};int size=std::clamp(options.size,0,2);
 if(!wall::makePanel(hit.end,hit.plane.normal,widths[size],heights[size],camera.epoch,panel)||!separated(panel,existing,options.spacing))return false;
 // The proposed rectangle must be in front of the current camera and reachable
 // by the eye, rather than attached to the far side of a corridor corner.
 if(!onScreen(panel,camera,float(camera.width),float(camera.height)))return false;
 if(!trace::supported(panel,cast)||!trace::visible(panel,pose.eye,cast))return false;return true;
}
}
