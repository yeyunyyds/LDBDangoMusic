#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
namespace dango::chroma {
inline const char* name(int mode){const char* names[]={u8"纯色（原有效果）",u8"双色渐变",u8"逐字彩虹",u8"流动渐变",u8"双色呼吸"};return names[std::clamp(mode,0,4)];}
inline uint32_t mix(uint32_t a,uint32_t b,float amount){amount=std::clamp(amount,0.f,1.f);uint32_t out=0;for(int shift:{16,8,0}){float first=float((a>>shift)&255),last=float((b>>shift)&255);out|=uint32_t(std::lround(first+(last-first)*amount))<<shift;}return out;}
inline uint32_t rainbow(double hue){hue-=std::floor(hue);double h=hue*6;int sector=int(h);float f=float(h-sector),p=.45f,q=1-.55f*f,t=.45f+.55f*f;float r=1,g=t,b=p;switch(sector){case 1:r=q;g=1;break;case 2:r=p;g=1;b=t;break;case 3:r=p;g=q;b=1;break;case 4:r=t;g=p;b=1;break;case 5:r=1;g=p;b=q;break;default:break;}return (uint32_t(std::lround(r*255))<<16)|(uint32_t(std::lround(g*255))<<8)|uint32_t(std::lround(b*255));}
inline uint32_t sample(uint32_t first,uint32_t second,int mode,float position,double seconds,float speed,float amount){
 first&=0xffffff;second&=0xffffff;if(mode<=0||mode>4||!std::isfinite(position)||!std::isfinite(seconds)||!std::isfinite(speed)||!std::isfinite(amount))return first;
 position=std::clamp(position,0.f,1.f);amount=std::clamp(amount,0.f,1.f);double phase=seconds*std::clamp(speed,0.f,2.f);uint32_t effect=first;
 if(mode==1)effect=mix(first,second,position);else if(mode==2)effect=rainbow(phase+position*.85);else effect=mix(first,second,float(.5+.5*std::sin(6.283185307179586*phase+(mode==3?position*3.141592653589793:0))));
 return mix(first,effect,amount);
}
struct Palette {const char* name;uint32_t first,second;int mode;};
inline Palette palette(int index){const Palette values[]={{u8"海盐落日",0xffb7a5,0x8edbff,1},{u8"极光薄荷",0xa7f3d0,0xc4b5fd,1},{u8"樱桃汽水",0xff8fb3,0xffd68a,3},{u8"霓虹光谱",0xffffff,0xb6e3ff,2}};return values[std::clamp(index,0,3)];}
}
