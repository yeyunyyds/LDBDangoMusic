#pragma once
#include "json.hpp"
#include "color_effects.hpp"
#include <string>
#include <stdexcept>
namespace dango::visual {
struct Style {
 bool softEntrance=false,tone=false,dynamicGlow=false,smoothScale=false,bob=false,translationFollow=true,wallEffects=false;
 float metadataSize=1;
 float pending=.45f,sung=.85f,translationSize=.72f,translationOpacity=1,translationGap=8,originalOpacity=1;
 float bobPeriod=4,bobAmount=.006f,popStrength=1;int exit=0,smoothMs=180;
 uint32_t highlight=0xffffff,translationColor=0xf7e1d7;
};
inline uint32_t rgb(const std::string& text){if(text.size()!=7||text[0]!='#'||text.find_first_not_of("0123456789abcdefABCDEF",1)!=std::string::npos)throw std::runtime_error("Invalid visual RGB");return uint32_t(std::stoul(text.substr(1),nullptr,16));}
inline std::string hex(uint32_t c){const char* digits="0123456789ABCDEF";std::string s="#000000";for(int i=0;i<6;++i)s[6-i]=digits[(c>>(i*4))&15];return s;}
inline Style read(const nlohmann::json& j){if(!j.is_object())throw std::runtime_error("visualStyle must be an object");Style s;
 auto f=[&](const char* k,float fallback,float lo,float hi){float v=j.value(k,fallback);if(!std::isfinite(v))throw std::runtime_error("Non-finite visual setting");return std::clamp(v,lo,hi);};
 s.softEntrance=j.value("softEntrance",false);s.wallEffects=j.value("wallEffects",false);s.tone=j.value("tone",false);s.dynamicGlow=j.value("dynamicGlow",false);s.smoothScale=j.value("smoothScale",false);s.bob=j.value("bob",false);s.translationFollow=j.value("translationFollow",true);
 s.metadataSize=f("metadataSize",1,.4f,1.5f);s.pending=f("pending",.45f,.1f,1);s.sung=f("sung",.85f,.1f,1);s.originalOpacity=f("originalOpacity",1,.1f,1);s.translationSize=f("translationSize",.72f,.35f,1.5f);s.translationOpacity=f("translationOpacity",1,.1f,1);s.translationGap=f("translationGap",8,0,64);
 s.bobPeriod=f("bobPeriod",4,.2f,20);s.bobAmount=f("bobAmount",.006f,0,.05f);s.popStrength=f("popStrength",1,0,1.5f);s.exit=std::clamp(j.value("exit",0),0,4);s.smoothMs=std::clamp(j.value("smoothMs",180),50,1000);
 s.highlight=rgb(j.value("highlight",std::string("#FFFFFF")));s.translationColor=rgb(j.value("translationColor",std::string("#F7E1D7")));return s;
}
inline nlohmann::json json(const Style& s){return {{"softEntrance",s.softEntrance},{"metadataSize",s.metadataSize},{"wallEffects",s.wallEffects},{"tone",s.tone},{"dynamicGlow",s.dynamicGlow},{"smoothScale",s.smoothScale},{"bob",s.bob},{"translationFollow",s.translationFollow},{"pending",s.pending},{"sung",s.sung},{"originalOpacity",s.originalOpacity},{"translationSize",s.translationSize},{"translationOpacity",s.translationOpacity},{"translationGap",s.translationGap},{"bobPeriod",s.bobPeriod},{"bobAmount",s.bobAmount},{"popStrength",s.popStrength},{"exit",s.exit},{"smoothMs",s.smoothMs},{"highlight",hex(s.highlight)},{"translationColor",hex(s.translationColor)}};}
inline const char* exitName(int i){const char* names[]={u8"原有退场",u8"柔和淡出",u8"轻盈上浮",u8"缩小淡出",u8"逐字散去"};return names[std::clamp(i,0,4)];}
struct Smooth {float value=0;double last=-1;float sample(float target,double clock,bool enabled,int ms){if(!std::isfinite(target)||target<=0)return value;double dt=clock-last;if(!enabled||last<0||dt<0||dt>1)value=target;else value+=(target-value)*float(1-std::exp(-std::clamp(dt,0.,.1)/(std::max(50,ms)/1000.)));last=clock;return value;}};
struct Exit {float alpha=1,scale=1,x=0,y=0;};
inline Exit retire(int mode,float alpha,float fraction,uint32_t seed){Exit e;e.alpha=std::clamp(alpha,0.f,1.f);float t=1-e.alpha;if(mode==2)e.y=-12*t;else if(mode==3)e.scale=1-.35f*t;else if(mode==4){e.alpha=std::clamp(e.alpha*(1+.4f*fraction)-.4f*fraction,0.f,1.f);e.x=(float(seed%101)/50.f-1)*18*t;e.y=-(8+float((seed>>8)%17))*t;e.scale=1-.15f*t;}return e;}
inline float bobOffset(const Style& s,double seconds,uint32_t phase){return s.bob?float(std::sin(seconds*6.283185307179586/std::max(.2f,s.bobPeriod)+(phase%100)*.06283185))*s.bobAmount:0;}
}
