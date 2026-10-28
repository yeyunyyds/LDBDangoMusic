#pragma once
#include <array>
#include <atomic>
#include <cstdint>
#include <algorithm>
namespace dango {
inline bool needsReanchor(double nowSeconds,double placedSeconds,int intervalMs){return intervalMs>0&&(nowSeconds-placedSeconds)*1000>=intervalMs;}
struct ExitFrame {float alpha=1,lift=0;bool done=false;};
inline ExitFrame exitFrame(double ageMs,int linger,int fade){double elapsed=ageMs-std::max(0,linger);if(elapsed<=0)return {};float t=float(elapsed/std::max(1,fade));if(t>=1)return {0,.16f,true};return {1-t,t*.16f,false};}
}
