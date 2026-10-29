#pragma once
namespace dango::hud {
struct Appearance {
 unsigned preset=0,step=8;float opacity=.48f,font=22,x=8,y=14;bool f10=false;
 bool operator==(const Appearance& other)const{return preset==other.preset&&step==other.step&&opacity==other.opacity&&font==other.font&&x==other.x&&y==other.y&&f10==other.f10;}
 bool operator!=(const Appearance& other)const{return !(*this==other);}
};
}
