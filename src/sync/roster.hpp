#pragma once
#include "protocol.hpp"
#include <vector>
namespace dango::sync {
inline Peer playerSteamId(const unsigned char* data,size_t size){if(!data||size<144||data[116]||data[117])return 0;uint64_t xuid=0;uint32_t account=0;std::memcpy(&xuid,data,8);std::memcpy(&account,data+80,4);if(steamId(xuid))return !account||uint32_t(xuid)==account?xuid:0;if(account){auto id=76561197960265728ULL|uint64_t(account);if(steamId(id))return id;}return 0;}
struct Roster {bool active=false;uint64_t generation=0;int64_t tick=0;Peer local=0;std::vector<Peer> players;std::string status="Waiting for in-game player roster";
 bool fresh(int64_t now)const{return active&&steamId(local)&&generation&&now>=tick&&now-tick<=5000&&players.size()<=4&&std::all_of(players.begin(),players.end(),[](Peer p){return steamId(p);})&&std::set<Peer>(players.begin(),players.end()).size()==players.size()&&std::find(players.begin(),players.end(),local)!=players.end();}
 std::set<Peer> others()const{std::set<Peer> out;for(auto p:players)if(p!=local&&steamId(p)&&out.size()<3)out.insert(p);return out;}
 uint64_t cohort()const{auto sorted=players;std::sort(sorted.begin(),sorted.end());uint64_t h=1469598103934665603ULL;for(auto id:sorted)for(unsigned i=0;i<8;++i){h^=(id>>(i*8))&255;h*=1099511628211ULL;}return h?h:1;}
};
}
