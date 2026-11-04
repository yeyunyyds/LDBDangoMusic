#pragma once
#include "world_camera.hpp"
namespace dango::movement {
enum class Curve {EaseOut,Linear,EaseIn,EaseInOut,Back};
inline const char* name(int n){const char* names[]={u8"先快后慢",u8"匀速",u8"先慢后快",u8"先慢后快再慢",u8"回弹"};return names[std::clamp(n,0,4)];}
inline float ease(float t,int curve){t=std::clamp(t,0.f,1.f);switch(std::clamp(curve,0,4)){case 1:return t;case 2:return t*t*t;case 3:return t*t*(3-2*t);case 4:{float a=t-1;return 1+2.70158f*a*a*a+1.70158f*a*a;}default:return 1-(1-t)*(1-t)*(1-t);}}
inline Vec3 blend(Vec3 a,Vec3 b,float t){return a+(b-a)*t;}
struct Travel {
 Vec3 from{},to{};double started=0;bool active=false;
 Vec3 at(double now,float duration,int curve)const{return active?blend(from,to,ease(float((now-started)/std::max(.001f,duration)),curve)):to;}
 void begin(Vec3 a,Vec3 b,double now){from=a;to=b;started=now;active=true;}
};
struct Tracker {
 Travel travel;Vec3 value{};uint32_t epoch=0,handle=0;double tick=0;bool ready=false;
 void reset(){*this={};}
 Vec3 select(Vec3 target,uint32_t map,uint32_t id,double now,bool enabled,float duration,int curve){
  if(!enabled||!ready||epoch!=map||handle!=id||now<tick||now-tick>.5||length(target-value)>1024){value=target;travel={};travel.to=target;ready=true;epoch=map;handle=id;tick=now;return value;}
  value=travel.at(now,duration,curve);if(length(target-travel.to)>.001f)travel.begin(value,target,now);
  value=travel.at(now,duration,curve);tick=now;return value;
 }
};
}
