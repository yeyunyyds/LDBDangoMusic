#include "sync/discovery.hpp"
#include "sync/song_switch.hpp"
#include <cassert>
#include <iostream>
using namespace dango;
constexpr sync::Peer a=76561197960265729ULL,b=76561197960265730ULL,c=76561197960265731ULL,outsider=76561197960265732ULL;
int main(){
 unsigned char info[144]{};std::memcpy(info,&a,8);assert(sync::playerSteamId(info,144)==a);info[116]=1;assert(!sync::playerSteamId(info,144));info[116]=0;info[117]=1;assert(!sync::playerSteamId(info,144));info[117]=0;uint32_t account=2;std::memcpy(info+80,&account,4);assert(!sync::playerSteamId(info,144));std::memset(info,0,144);std::memcpy(info+80,&account,4);assert(sync::playerSteamId(info,144)==b&&!sync::playerSteamId(info,143));
 sync::Roster ra;ra.active=true;ra.local=a;ra.players={a,b,c};ra.generation=1;ra.tick=0;auto rb=ra;rb.local=b;auto rc=ra;rc.local=c;assert(ra.fresh(1000)&&!ra.fresh(5001));assert(ra.others().size()==2&&ra.cohort()==rb.cohort());
 auto invalid=ra;invalid.players={a,b,b};assert(!invalid.fresh(100));invalid.players={a,1};assert(!invalid.fresh(100));
 sync::Bus bus;sync::Bus::Endpoint ea(bus,a),eb(bus,b),ec(bus,c),evil(bus,outsider);sync::Discovery aa(ea,ra,11,0),bb(eb,rb,22,0),cc(ec,rc,33,0);
 auto tick=[&](int64_t t,bool activeC=true){aa.tick(true,true,t);bb.tick(true,true,t);cc.tick(activeC,true,t);};for(int64_t t=0;t<2500;t+=50)tick(t);
 assert(aa.directory().at(b).confirmed&&aa.directory().at(c).confirmed);assert(aa.leader().id&&aa.leader()==bb.leader()&&bb.leader()==cc.leader());
 assert(bb.claim(2500));for(int64_t t=2500;t<3000;t+=50)tick(t);assert(aa.leader().id==b&&cc.leader().id==b);
 assert(cc.claim(3000));for(int64_t t=3000;t<3500;t+=50)tick(t);assert(aa.leader().id==c&&bb.leader().id==c); // later declaration wins
 assert(aa.claim(3500)&&bb.claim(3500));for(int64_t t=3500;t<4000;t+=50)tick(t);assert(aa.leader()==bb.leader()&&bb.leader()==cc.leader()); // concurrent declarations converge
 assert(cc.claim(4000));for(int64_t t=4000;t<4300;t+=50)tick(t);assert(aa.leader().id==c);
 for(int64_t t=4300;t<5000;t+=50)tick(t,false);assert(aa.leader().id!=c&&bb.leader().id!=c);
 sync::RoomMessage bad;bad.kind="claim";bad.sender=outsider;bad.boot=99;bad.cohort=ra.cohort();bad.seq=1;bad.term=999;bad.enabled=bad.lx=true;evil.send(a,sync::roomEncode(bad));auto rejects=aa.rejected;aa.tick(true,true,5100);assert(aa.rejected>rejects&&aa.leader().id!=outsider);
 // A fresh plugin incarnation must handshake; replays from its old incarnation cannot take over.
 sync::Discovery restarted(eb,rb,222,5200);for(int64_t t=5200;t<6800;t+=50){aa.tick(true,true,t);restarted.tick(true,true,t);cc.tick(false,true,t);}assert(aa.directory().at(b).boot==222);
 // Wrong game scope and stale claims from a retired incarnation cannot seize ownership.
 sync::RoomMessage replay;replay.kind="claim";replay.sender=b;replay.boot=22;replay.cohort=ra.cohort();replay.seq=9999;replay.term=999;replay.enabled=replay.lx=true;eb.send(a,sync::roomEncode(replay));rejects=aa.rejected;aa.tick(true,true,6900);assert(aa.rejected>rejects&&aa.directory().at(b).boot==222);
 replay.boot=222;replay.cohort++;eb.send(a,sync::roomEncode(replay));rejects=aa.rejected;aa.tick(true,true,6950);assert(aa.rejected>rejects);
 sync::Bus silentBus;sync::Bus::Endpoint only(silentBus,a);sync::Discovery alone(only,ra,44,0);for(int t=0;t<130000;t+=500)alone.tick(false,false,t);assert(alone.sent+alone.sendFailures==16);for(auto& member:alone.directory())assert(!member.second.confirmed); // no synthetic installed status or endless probes
 sync::Song desired{"A / \"song\" & ? 中文","Singer",203989};auto uri=sync::searchPlayUri(desired);assert(uri.rfind("lxmusic://music/searchPlay?data=",0)==0&&uri.find("%26")!=std::string::npos&&uri.find("%22")!=std::string::npos&&uri.find("03%3A24")!=std::string::npos);
 // interval can have no leading zero in minutes, still mm:ss semantic.
 sync::SongSwitch search;State local;local.connected=true;local.sampled=1000;int dispatched=0;auto dispatch=[&](const std::string& u){assert(u.rfind("lxmusic://music/searchPlay?data=",0)==0);++dispatched;};search.tick(desired,true,local,1000,dispatch);assert(!dispatched);search.tick(desired,true,local,1800,dispatch);assert(dispatched==1);search.tick(desired,true,local,1900,dispatch);assert(dispatched==1);
 local.title=desired.title;local.artist=desired.artist;local.duration=desired.duration;search.tick(desired,true,local,2000,dispatch);assert(dispatched==1&&search.status.find("verified")!=std::string::npos);
 sync::SongSwitch paused;State empty;paused.tick(desired,false,empty,0,dispatch);paused.tick(desired,false,empty,10000,dispatch);assert(dispatched==1);
 sync::SongSwitch failed;int failures=0;auto sendMissing=[&](const std::string&){++failures;};for(int64_t t=0;t<=200000;t+=1000)failed.tick(desired,true,empty,t,sendMissing);assert(failures==3);failed.retry(200001);failed.tick(desired,true,empty,200001,sendMissing);assert(failures==4);
 bool overlong=false;try{sync::searchPlayUri({std::string(201,'a'),"b",1000});}catch(...){overlong=true;}assert(overlong);
 std::cout<<"Passed: verified player-info layout/bots, same-game census scope, discovery, last/concurrent host claims, withdrawal, outsider/restart rejection and Scheme escaping/debounce/selection verification\n";
}
