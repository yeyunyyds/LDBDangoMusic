#pragma once
#include "core.hpp"
#include <set>
#include <map>
#include <deque>
namespace dango::sync {
using Peer=uint64_t;
inline bool steamId(Peer id){return (id>>56)==1&&((id>>52)&15)==1&&((id>>32)&0xfffff)==1&&uint32_t(id)!=0;}
inline Peer peer(const std::string& value){if(value.empty()||value.size()>20||value.find_first_not_of("0123456789")!=std::string::npos)throw std::runtime_error("SteamID64 must be decimal text");auto p=std::stoull(value);if(!steamId(p))throw std::runtime_error("Not a public individual SteamID64");return p;}
struct Packet{Peer from=0;std::string bytes;};
struct Transport{virtual ~Transport()=default;virtual Peer self()const=0;virtual bool send(Peer,const std::string&)=0;virtual bool receive(Packet&)=0;};
struct Song{std::string title,artist;int64_t duration=0;};
inline Song song(const State& state){return {state.title,state.artist,state.duration};}
inline std::string normalized(std::string text){std::string out;bool gap=false;for(unsigned char c:text){if(c<=32){gap=!out.empty();continue;}if(gap){out+=' ';gap=false;}out+=char(c>='A'&&c<='Z'?c+32:c);}return out;}
inline bool matches(const Song& a,const Song& b){return !a.title.empty()&&!b.title.empty()&&normalized(a.title)==normalized(b.title)&&normalized(a.artist)==normalized(b.artist)&&a.duration>0&&std::llabs(a.duration-b.duration)<=1500;}
struct Message{
 std::string kind;Peer sender=0;uint64_t room=0,seq=0,cookie=0;int64_t t0=0,t1=0,t2=0,position=0;bool playing=false;double rate=1;Song track;uint64_t cohort=0;
};
inline std::string encode(const Message& m){nlohmann::json j={{"app","L4D2DangoMusic.lx-progress"},{"v",3},{"source","lx"},{"kind",m.kind},{"sender",std::to_string(m.sender)},{"room",std::to_string(m.room)},{"seq",m.seq},{"cookie",std::to_string(m.cookie)},{"t0",m.t0},{"t1",m.t1},{"t2",m.t2},{"positionMs",m.position},{"playing",m.playing},{"rate",m.rate},{"title",m.track.title},{"artist",m.track.artist},{"durationMs",m.track.duration},{"cohort",std::to_string(m.cohort)}};auto s=j.dump();if(s.size()>4096)throw std::runtime_error("Sync packet too large");return s;}
inline uint64_t unsignedText(const nlohmann::json& j,const char* field){auto s=j.at(field).get<std::string>();if(s.empty()||s.size()>20||s.find_first_not_of("0123456789")!=std::string::npos)throw std::runtime_error("Invalid sync identifier");return std::stoull(s);}
inline nlohmann::json flatObject(const std::string& bytes,size_t maximum){if(bytes.empty()||bytes.size()>maximum)throw std::runtime_error("Packet size rejected");bool quoted=false,escaped=false;int depth=0;for(char c:bytes){if(quoted){if(escaped)escaped=false;else if(c=='\\')escaped=true;else if(c=='"')quoted=false;continue;}if(c=='"')quoted=true;else if(c=='['||c==']'||(c=='{'&&++depth>1))throw std::runtime_error("Sync requires a flat metadata object");else if(c=='}')--depth;}return nlohmann::json::parse(bytes);}
inline Message decode(const Packet& packet){
 auto j=flatObject(packet.bytes,4096);if(!j.is_object()||j.size()!=18||j.at("app")!="L4D2DangoMusic.lx-progress"||(j.at("v")!=3||j.at("source")!="lx"))throw std::runtime_error("Unsupported sync protocol");
 Message m;m.kind=j.at("kind").get<std::string>();if(m.kind!="hello"&&m.kind!="welcome"&&m.kind!="clock"&&m.kind!="pong"&&m.kind!="state"&&m.kind!="probe"&&m.kind!="echo")throw std::runtime_error("Unknown sync message");m.sender=peer(j.at("sender").get<std::string>());if(m.sender!=packet.from)throw std::runtime_error("Transport identity mismatch");m.cohort=unsignedText(j,"cohort");m.room=unsignedText(j,"room");m.cookie=unsignedText(j,"cookie");if(!j.at("seq").is_number_unsigned())throw std::runtime_error("Invalid sync sequence");m.seq=j.at("seq").get<uint64_t>();m.t0=j.at("t0").get<int64_t>();m.t1=j.at("t1").get<int64_t>();m.t2=j.at("t2").get<int64_t>();m.position=j.at("positionMs").get<int64_t>();m.playing=j.at("playing").get<bool>();m.rate=j.at("rate").get<double>();m.track={j.at("title").get<std::string>(),j.at("artist").get<std::string>(),j.at("durationMs").get<int64_t>()};
 if(m.t0<0||m.t1<0||m.t2<0||m.t0>INT64_MAX/4||m.t1>INT64_MAX/4||m.t2>INT64_MAX/4||m.position<0||m.position>86400000||m.track.duration<0||m.track.duration>86400000||m.track.title.size()>512||m.track.artist.size()>512||!std::isfinite(m.rate)||m.rate<.1||m.rate>8)throw std::runtime_error("Sync field bounds rejected");return m;
}
struct Clock {
 bool ready=false;double offset=0;int64_t rtt=0,updated=0;
 bool sample(int64_t t0,int64_t t1,int64_t t2,int64_t t3){auto delay=(t3-t0)-(t2-t1);if(t3<t0||t2<t1||delay<0||delay>2000)return false;double next=(double(t1-t0)+double(t2-t3))/2;
 if(!ready||t3-updated>10000||delay<=rtt+20){offset=next;rtt=delay;updated=t3;ready=true;return true;}return false;}
 bool fresh(int64_t now)const{return ready&&now>=updated&&now-updated<15000;}
};
struct Plan {bool valid=false,seek=false,play=false,pause=false;int64_t target=0,drift=0;std::string reason;};
inline Plan plan(const State& local,const Message& remote,const Clock& clock,int64_t now,int64_t received,int threshold=350){Plan p;
 if(!clock.fresh(now)||now<received||now-received>3000){p.reason="Host clock or heartbeat expired";return p;}
 if(!local.connected||local.key.empty()||!matches(song(local),remote.track)){p.reason="Select the same song/version locally";return p;}
 double age=double(now)+clock.offset-double(remote.t2);if(age< -100||age>3000){p.reason="Stale host playback sample";return p;}p.target=std::clamp(remote.position+(remote.playing?int64_t(std::max(0.,age)*remote.rate):0),int64_t(0),remote.track.duration);auto actual=local.at(now);if(actual<0){p.reason="Local playback sample expired";return p;}
 if(std::abs(local.rate-remote.rate)>.001){p.reason="Playback rates differ; set the same rate locally";return p;}p.valid=true;p.drift=p.target-actual;p.seek=std::llabs(p.drift)>threshold;p.play=remote.playing&&!local.playing;p.pause=!remote.playing&&local.playing;p.reason="Same track; progress synchronization ready";return p;
}
struct Bus {
 std::map<Peer,std::deque<Packet>> queues;size_t drops=0;
 struct Endpoint:Transport {Bus& bus;Peer id;Endpoint(Bus& b,Peer p):bus(b),id(p){bus.queues[id];}Peer self()const override{return id;}bool send(Peer p,const std::string& bytes)override{auto it=bus.queues.find(p);if(it==bus.queues.end()||bytes.size()>4096||it->second.size()>=64){++bus.drops;return false;}it->second.push_back({id,bytes});return true;}bool receive(Packet& out)override{auto& q=bus.queues[id];if(q.empty())return false;out=std::move(q.front());q.pop_front();return true;}};
};
}
