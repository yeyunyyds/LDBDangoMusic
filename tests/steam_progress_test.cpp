#include "sync/session.hpp"
#include "sync/lx_control.hpp"
#include "sync/settings.hpp"
#include "sync/simulation.hpp"
#include "sync/source_policy.hpp"
#include <cassert>
#include <iostream>
using namespace dango;
constexpr uint64_t a=76561197960265729ULL,b=76561197960265730ULL,c=76561197960265731ULL;
State track(int64_t tick,int64_t position=10000,bool playing=true){State s;s.connected=true;s.playing=playing;s.key="song";s.title="Song";s.artist="Artist";s.duration=120000;s.position=position;s.sampled=tick;return s;}
int main(){
 sync::SourceContext lxSource{sync::Provider::LX,7,10},neteaseSource{sync::Provider::Netease,8,11};assert(sync::currentCommand(lxSource,7,10));assert(!sync::currentCommand(lxSource,7,9));assert(!sync::currentCommand(lxSource,6,10));assert(!sync::currentCommand(neteaseSource,8,11));assert(!sync::lxAllowed({sync::Provider::Windows,8,11})&&!sync::lxAllowed({sync::Provider::Demo,8,11})&&!sync::lxAllowed({}));
 assert(sync::virtualPlayersTest().rfind("PASS",0)==0);
 assert(sync::steamId(a));bool rejected=false;try{sync::peer("1");}catch(...){rejected=true;}assert(rejected);
 lx::Executor controls;std::vector<std::string> paths;auto request=[&](const std::string& p){paths.push_back(p);};auto local=track(1000);
 auto result=controls.begin({1,lx::Action::Seek,12001,"song"},local,1000,request);assert(result.accepted&&!result.verified&&paths.back()=="/seek?offset=12.001");assert(controls.observe(track(1100,12101),1100).verified);
 result=controls.begin({2,lx::Action::Stop,0,"song"},local,1000,request);assert(result.accepted&&paths[1]=="/pause"&&paths[2]=="/seek?offset=0.000");assert(controls.observe(track(1100,0,false),1100).verified);
 assert(!controls.begin({3,lx::Action::Seek,120001,"song"},local,1000,request).accepted);
 controls.begin({4,lx::Action::Pause,0,"song"},local,1000,request);auto pending=controls.observe(local,3100);assert(pending.accepted&&!pending.verified&&!controls.busy());
 State idle;idle.connected=true;idle.sampled=1000;controls.cancel();assert(controls.begin({5,lx::Action::Play,0,""},idle,1000,request).accepted);auto stillStarting=controls.observe(idle,1100);assert(controls.busy()&&!stillStarting.verified);assert(controls.observe(track(1500),1500).verified);
 sync::Bus bus;sync::Bus::Endpoint host(bus,a),follower(bus,b),outsider(bus,c);
 sync::Session leader(host,sync::Role::Host,{b},0,111),client(follower,sync::Role::Follower,{a},a,222);
 // Different OS uptimes: host clock is +50 seconds. Simulated one-way 20ms.
 for(int64_t now=1000;now<=2000;now+=20){client.tick(track(now,5000+now-1000),now);leader.tick(track(now+50020,10000+now-1000),now+50020);}
 auto correction=client.correction(track(2020,6020),2020);assert(correction.valid&&correction.seek&&correction.drift>4000&&client.timing().ready);assert(client.timing().rtt<=40);
 assert(!client.correction(track(2020,6020),2020,10000).seek);
 auto wrong=track(2020);wrong.title="Other";assert(!client.correction(wrong,2020).valid);
 assert(!client.correction(track(20000),20000).valid);
 sync::Message forged;forged.sender=a;forged.kind="state";forged.room=111;forged.seq=999;forged.track=sync::song(local);outsider.send(b,sync::encode(forged));client.tick(track(2040),2040);assert(client.rejected>0);
 auto bytes=sync::encode(forged);auto json=nlohmann::json::parse(bytes);json["audioURL"]="forbidden";rejected=false;try{sync::decode({a,json.dump()});}catch(...){rejected=true;}assert(rejected);
 sync::Message sample;sample.sender=a;sample.kind="state";sample.track=sync::song(local);sample.t2=1000;sample.position=10000;sample.playing=false;sync::Clock clock;assert(clock.sample(1000,51020,51020,1040));sample.t2=51020;auto paused=sync::plan(track(1040),sample,clock,1040,1040);assert(paused.valid&&paused.pause&&!paused.play);

 auto strict=nlohmann::json::parse(sync::encode(forged));strict["seq"]=-1;rejected=false;try{sync::decode({a,strict.dump()});}catch(...){rejected=true;}assert(rejected);
 strict=nlohmann::json::parse(sync::encode(forged));strict["source"]="netease";rejected=false;try{sync::decode({a,strict.dump()});}catch(...){rejected=true;}assert(rejected);
 strict=nlohmann::json::parse(sync::encode(forged));strict["title"]=nlohmann::json::array();rejected=false;try{sync::decode({a,strict.dump()});}catch(...){rejected=true;}assert(rejected);
 sync::Settings settings;settings.role=2;settings.host=a;settings.peers={a};assert(sync::readSettings(sync::settingsJson(settings)).host==a);assert(!sync::Settings{}.apply&&sync::Settings{}.role==0);
 sync::Message duplicate;duplicate.kind="state";duplicate.sender=a;duplicate.room=111;duplicate.seq=1;duplicate.track=sync::song(local);host.send(b,sync::encode(duplicate));auto rejectedBefore=client.rejected;client.tick(track(2100),2100);assert(client.rejected>rejectedBefore);
 client.tick(track(10000),10000);assert(!client.correction(track(10000),10000).valid); // expire and rejoin, no stale controls
 sync::Clock stale;assert(stale.sample(1,1,1,1));assert(!stale.fresh(16000));
 sync::Clock healthy;assert(healthy.sample(1000,51020,51020,1040));sample.playing=true;sample.position=10000;auto exact=sync::plan(track(1040,10020),sample,healthy,1040,1040);assert(exact.valid&&!exact.seek&&!exact.play&&!exact.pause);
 // Test-domain virtual IDs never authenticate a real Steam user. This bus only exercises protocol.
 sync::Bus selfBus;sync::Bus::Endpoint self(selfBus,a);forged.sender=a;assert(self.send(a,sync::encode(forged)));sync::Packet echoed;assert(self.receive(echoed)&&sync::decode(echoed).sender==a);
 std::cout<<"Passed: LX endpoint units/stop/readback/failure, virtual Steam-ID bus, host/follower handshake, skew/RTT, drift, track mismatch, heartbeat expiry, spoof/extra payload rejection and local protocol self loop\n";
}
