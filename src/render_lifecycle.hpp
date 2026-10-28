#pragma once
#include <array>
#include <atomic>
#include <cstdint>
#include <algorithm>
namespace dango {
// Draw at most once onto each canonical presentation target between scene writes. COM aliases, camera changes
// and additional Present calls do not grant a second overlay pass.
struct FrameGate {
 struct Stamp {uintptr_t surface=0;uint64_t scene=0;bool valid=false;};std::array<Stamp,16> stamps{};unsigned cursor=0;std::atomic<uint64_t> painted{0},duplicates{0};
 bool allow(uintptr_t surface,uint64_t scene){for(auto& s:stamps)if(s.valid&&s.surface==surface){if(s.scene==scene){++duplicates;return false;}s.scene=scene;++painted;return true;}stamps[cursor++%stamps.size()]={surface,scene,true};++painted;return true;}
 void reset(){stamps={};cursor=0;painted=0;duplicates=0;}
};
inline bool needsReanchor(double nowSeconds,double placedSeconds,int intervalMs){return intervalMs>0&&(nowSeconds-placedSeconds)*1000>=intervalMs;}
struct ExitFrame {float alpha=1,lift=0;bool done=false;};
inline ExitFrame exitFrame(double ageMs,int linger,int fade){double elapsed=ageMs-std::max(0,linger);if(elapsed<=0)return {};float t=float(elapsed/std::max(1,fade));if(t>=1)return {0,.16f,true};return {1-t,t*.16f,false};}
}
