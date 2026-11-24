#pragma once
#include <array>
namespace dango::autosource {
// Stable IDs from Config.musicSource. Prefer a playing source over an idle
// API; ties retain the current playing source to avoid needless lyric resets.
struct Probe {int id;bool connected=false,playing=false;};
inline int choose(const std::array<Probe,3>& probes,int current){
 for(const auto& p:probes)if(p.id==current&&p.connected&&p.playing)return p.id;
 for(const auto& p:probes)if(p.connected&&p.playing)return p.id;
 for(const auto& p:probes)if(p.id==current&&p.connected)return p.id;
 for(const auto& p:probes)if(p.connected)return p.id;
 return 0;
}
}
