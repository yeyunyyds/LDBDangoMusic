#pragma once
#include <array>
#include <atomic>
#include <cstdint>
#include <algorithm>
#include <string>
#include <cstdio>
namespace dango {
inline bool needsReanchor(double nowSeconds,double placedSeconds,int intervalMs){return intervalMs>0&&(nowSeconds-placedSeconds)*1000>=intervalMs;}
inline int adjustReanchor(int milliseconds,int choice){const int delta[]={-10000,-1000,-100,100,1000,10000};return choice>=0&&choice<6?std::clamp(milliseconds+delta[choice],0,300000):milliseconds;}
inline std::string reanchorSeconds(int milliseconds){char text[40];std::snprintf(text,sizeof(text),"%.3f",milliseconds/1000.);std::string value=text;while(!value.empty()&&value.back()=='0')value.pop_back();if(!value.empty()&&value.back()=='.')value.pop_back();return value;}
struct ExitFrame {float alpha=1,lift=0;bool done=false;};
inline ExitFrame exitFrame(double ageMs,int linger,int fade){double elapsed=ageMs-std::max(0,linger);if(elapsed<=0)return {};float t=float(elapsed/std::max(1,fade));if(t>=1)return {0,.16f,true};return {1-t,t*.16f,false};}
}
