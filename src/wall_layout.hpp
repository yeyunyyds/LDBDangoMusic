#pragma once
#include "json.hpp"
#include "wall_panel.hpp"
#include <stdexcept>
namespace dango {
struct WallTextPosition {float x=.5f,y=.58f,size=1,depth=.1f;};
struct WallTextLayout {WallTextPosition title{.5f,.13f,1,.05f},artist{.5f,.26f,1,.05f},lyrics;};
inline WallTextPosition& wallComponent(WallTextLayout& l,int i){return i==0?l.title:i==1?l.artist:l.lyrics;}
inline const WallTextPosition& wallComponent(const WallTextLayout& l,int i){return i==0?l.title:i==1?l.artist:l.lyrics;}
inline float& wallAxis(WallTextPosition& p,int axis){return axis==0?p.x:axis==1?p.y:axis==2?p.size:p.depth;}
inline void adjustWallText(WallTextLayout& l,int i,int axis,int choice){
 auto& p=wallComponent(l,i);if(choice==4){auto defaults=wallComponent(WallTextLayout{},i);wallAxis(p,axis)=wallAxis(defaults,axis);return;}
 const float fine[]={.01f,.01f,.05f,.05f},coarse[]={.05f,.05f,.25f,.5f},low[]={0,0,.25f,.05f},high[]={1,1,3,16};
 float change=choice==0?-coarse[axis]:choice==1?-fine[axis]:choice==2?fine[axis]:coarse[axis];wallAxis(p,axis)=std::clamp(wallAxis(p,axis)+change,low[axis],high[axis]);
}
inline nlohmann::json wallPositionJson(const WallTextPosition& p){return {{"x",p.x},{"y",p.y},{"size",p.size},{"depth",p.depth}};}
inline nlohmann::json wallLayoutJson(const WallTextLayout& l){return {{"title",wallPositionJson(l.title)},{"artist",wallPositionJson(l.artist)},{"lyrics",wallPositionJson(l.lyrics)}};}
inline WallTextLayout readWallLayout(const nlohmann::json& j){
 if(!j.is_object())throw std::runtime_error("Wall layout must be an object");WallTextLayout l;
 const char* names[]={"title","artist","lyrics"},*keys[]={"x","y","size","depth"};const float low[]={0,0,.25f,.05f},high[]={1,1,3,16};
 for(int i=0;i<3;++i)if(j.contains(names[i])){auto& o=j.at(names[i]);if(!o.is_object())throw std::runtime_error("Wall component must be an object");auto& p=wallComponent(l,i);for(int a=0;a<4;++a){float v=o.value(keys[a],wallAxis(p,a));if(!std::isfinite(v))throw std::runtime_error("Non-finite wall layout");wallAxis(p,a)=std::clamp(v,low[a],high[a]);}}return l;
}
inline wall::Panel wallTextPanel(const wall::Panel& panel,const WallTextPosition& p){auto out=panel;out.center=wall::point(panel,p.x,p.y)+panel.normal*p.depth;return out;}
}
