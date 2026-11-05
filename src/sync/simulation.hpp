#pragma once
#include "session.hpp"
namespace dango::sync {
// In-memory test domain only. These logical IDs are never passed to a Steam API.
inline std::string virtualPlayersTest(){
 constexpr Peer hostId=76561197960265729ULL,clientId=76561197960265730ULL;
 Bus bus;Bus::Endpoint host(bus,hostId),client(bus,clientId);Session leader(host,Role::Host,{clientId},0,111),follower(client,Role::Follower,{hostId},hostId,222);
 auto sample=[](int64_t tick,int64_t position){State s;s.connected=s.playing=true;s.key="local-virtual-test";s.title="Local virtual progress test";s.artist="No music resources";s.duration=120000;s.position=position;s.sampled=tick;return s;};
 for(int64_t now=1000;now<=2000;now+=20){follower.tick(sample(now,now-1000),now);leader.tick(sample(now+50020,30000+now-1000),now+50020);}
 auto local=sample(2020,1020);auto decision=follower.correction(local,2020);if(!decision.valid||!decision.seek||std::llabs(decision.drift-30000)>100)throw std::runtime_error("Virtual player clock/progress test failed");
 auto wrong=local;wrong.title="Wrong local song";if(follower.correction(wrong,2020).valid||follower.correction(sample(20000,1000),20000).valid)throw std::runtime_error("Virtual player stale/track test failed");
 return "PASS: memory virtual players, +50s clock skew, drift="+std::to_string(decision.drift)+"ms; no real Steam identity test";
}
}
