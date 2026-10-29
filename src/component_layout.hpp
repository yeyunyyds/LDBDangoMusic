#pragma once
#include "json.hpp"
#include "world_camera.hpp"
#include <stdexcept>
namespace dango {
constexpr int MaxLyricSlots=9;
struct ComponentPosition {float x=.5f,y=.44f,distance=200,size=1;};
struct ComponentLayout {
 int lyricCount=2;
 ComponentPosition title{.5f,.14f,420,1.5f},artist{.5f,.21f,420,1.1f};
 std::array<ComponentPosition,MaxLyricSlots> lyrics{};
 ComponentLayout(){for(int i=0;i<MaxLyricSlots;++i)lyrics[i]={i%2?.7f:.3f,.44f+(i/2)*.08f,200,1};}
};
inline ComponentPosition& component(ComponentLayout& l,int index){if(index==0)return l.title;if(index==1)return l.artist;return l.lyrics.at(size_t(index-2));}
inline const ComponentPosition& component(const ComponentLayout& l,int index){if(index==0)return l.title;if(index==1)return l.artist;return l.lyrics.at(size_t(index-2));}
inline float& positionAxis(ComponentPosition& p,int axis){switch(axis){case 0:return p.x;case 1:return p.y;case 2:return p.distance;default:return p.size;}}
inline void adjustPosition(ComponentLayout& l,int index,int axis,int choice){
 auto& p=component(l,index);auto defaults=component(ComponentLayout{},index);float& value=positionAxis(p,axis);
 if(choice==4){value=positionAxis(defaults,axis);return;}
 const float fine[]={.01f,.01f,10,.05f},coarse[]={.05f,.05f,50,.25f},lo[]={.02f,.02f,32,.25f},hi[]={.98f,.98f,1600,3};
 float delta=choice==0?-coarse[axis]:choice==1?-fine[axis]:choice==2?fine[axis]:coarse[axis];value=std::clamp(value+delta,lo[axis],hi[axis]);
}
inline void adjustLyricGroup(ComponentLayout& l,int axis,int choice){for(int i=0;i<MaxLyricSlots;++i)adjustPosition(l,i+2,axis,choice);}
inline void migrateDefaultHeights(ComponentLayout& l){auto defaults=ComponentLayout{};for(int i=0;i<MaxLyricSlots;++i)if(std::abs(l.lyrics[i].y-(.56f+(i/2)*.08f))<.0001f)l.lyrics[i].y=defaults.lyrics[i].y;}
inline nlohmann::json positionJson(const ComponentPosition& p){return {{"x",p.x},{"y",p.y},{"distance",p.distance},{"size",p.size}};}
inline ComponentPosition readPosition(const nlohmann::json& j,ComponentPosition p){
 if(!j.is_object())throw std::runtime_error("Component position must be an object");
 const char* keys[]={"x","y","distance","size"};const float lo[]={.02f,.02f,32,.25f},hi[]={.98f,.98f,1600,3};
 for(int axis=0;axis<4;++axis){auto& value=positionAxis(p,axis);value=j.value(keys[axis],value);if(!std::isfinite(value))throw std::runtime_error("Non-finite component position");value=std::clamp(value,lo[axis],hi[axis]);}return p;
}
inline ComponentLayout readLayout(const nlohmann::json& j,ComponentLayout l={}){
 if(!j.is_object())throw std::runtime_error("Layout must be an object");l.lyricCount=std::clamp(j.value("lyricCount",l.lyricCount),1,MaxLyricSlots);
 if(j.contains("title"))l.title=readPosition(j.at("title"),l.title);if(j.contains("artist"))l.artist=readPosition(j.at("artist"),l.artist);
 if(j.contains("lyrics")){const auto& a=j.at("lyrics");if(!a.is_array())throw std::runtime_error("Lyric positions must be an array");for(size_t i=0;i<std::min(a.size(),l.lyrics.size());++i)l.lyrics[i]=readPosition(a[i],l.lyrics[i]);}return l;
}
inline nlohmann::json layoutJson(const ComponentLayout& l){auto a=nlohmann::json::array();for(auto& p:l.lyrics)a.push_back(positionJson(p));return {{"lyricCount",l.lyricCount},{"title",positionJson(l.title)},{"artist",positionJson(l.artist)},{"lyrics",a}};}
// Coordinates describe the initial screen position. Live world lyrics keep this
// world anchor until reanchoring; previews deliberately follow the camera.
inline bool componentAnchor(const GameCameraFrame& camera,const CameraPose& pose,const ComponentPosition& p,Vec3& anchor){
 Matrix4 inv{};Vec3 point{};if(!inverse(camera.worldToClip,inv)||!transform(inv,{p.x*2-1,1-p.y*2,.5f},point))return false;
 auto ray=normalize(point-pose.eye);if(length(ray)<.9f)return false;anchor=pose.eye+ray*p.distance;return true;
}
// An anchor is reusable only while its center is inside the current view.
inline bool visibleComponentAnchor(const GameCameraFrame& camera,Vec3 anchor){
 Vec3 ndc{};return transform(camera.worldToClip,anchor,ndc,true)&&std::abs(ndc.x)<=.96f&&std::abs(ndc.y)<=.96f&&ndc.z>=0&&ndc.z<=1;
}
struct LyricAnchorSlots {
 std::array<Vec3,MaxLyricSlots> points{};std::array<bool,MaxLyricSlots> valid{};
 void reset(){valid.fill(false);}
 Vec3 select(int slot,bool reuse,const GameCameraFrame& camera,Vec3 fallback){
 if(!reuse||!valid.at(slot)||!visibleComponentAnchor(camera,points.at(slot)))points.at(slot)=fallback;
 valid.at(slot)=true;return points.at(slot);
 }
 void remember(int slot,Vec3 point){points.at(slot)=point;valid.at(slot)=true;}
};
inline Placement screenComponent(const ComponentPosition& p,float width,float height,int fontSize){return {p.x*width,p.y*height,height/1080.f*28/std::max(fontSize,1)*p.size*200/p.distance};}
inline bool worldComponent(const GameCameraFrame& camera,Vec3 anchor,const ComponentPosition& p,float width,float height,float worldHeight,int fontSize,Placement& place){return projectAnchor(camera,anchor,width,height,worldHeight*p.size,fontSize,place);}
}
