#pragma once
#include "protocol.hpp"
namespace dango::sync {
enum class Role{Host,Follower};
class Session {
 Transport& io;Role role;std::set<Peer> allowed;Peer authority;uint64_t hostRoom,helloCookie,sequence=0,room=0,lastStateSequence=0;int64_t nextHello=0,nextClock=0,nextState=0,clockSentAt=-1,receivedAt=-1,lastHostSeen=-1;std::map<Peer,int64_t> joined;
 Message latest;Clock clock;
 void send(Peer to,Message m){m.sender=io.self();m.seq=++sequence;if(!io.send(to,encode(m)))++sendFailures;else ++sent;}
public:
 uint64_t sent=0,received=0,rejected=0,sendFailures=0;std::string status="Not connected";
 Session(Transport& transport,Role r,std::set<Peer> peers,Peer host,uint64_t nonce):io(transport),role(r),allowed(std::move(peers)),authority(host),hostRoom(nonce),helloCookie(nonce){if(!steamId(io.self())||!nonce||allowed.size()>8)throw std::runtime_error("Invalid sync session configuration");if(role==Role::Follower&&(!allowed.count(authority)||authority==io.self()))throw std::runtime_error("Follower requires one other allowed host");}
 void tick(const State& local,int64_t now){
  Packet packet;for(int budget=0;budget<32&&io.receive(packet);++budget){try{auto m=decode(packet);if(!allowed.count(m.sender)||m.sender==io.self())throw std::runtime_error("Peer not allowed");++received;
   if(role==Role::Host){
    if(m.kind=="hello"&&m.cookie){joined[m.sender]=now;Message reply;reply.kind="welcome";reply.room=hostRoom;reply.cookie=m.cookie;send(m.sender,reply);}
    else if(m.kind=="clock"&&m.room==hostRoom&&joined.count(m.sender)){joined[m.sender]=now;Message reply;reply.kind="pong";reply.room=hostRoom;reply.cookie=m.cookie;reply.t0=m.t0;reply.t1=reply.t2=now;send(m.sender,reply);}
    else ++rejected;
   }else{
    if(m.sender!=authority)throw std::runtime_error("Not the selected host");
    if(m.kind=="welcome"&&m.cookie==helloCookie&&m.room&&room==0){room=m.room;clock={};lastStateSequence=0;receivedAt=-1;lastHostSeen=now;nextClock=now;status="Host joined; estimating clock";}
    else if(m.room!=room||room==0)++rejected;
    else if(m.kind=="pong"&&m.cookie==helloCookie&&m.t0==clockSentAt&&clock.sample(m.t0,m.t1,m.t2,now)){lastHostSeen=now;clockSentAt=-1;}
    else if(m.kind=="state"&&m.seq>lastStateSequence){latest=std::move(m);lastStateSequence=latest.seq;receivedAt=lastHostSeen=now;}
    else ++rejected;
   }
  }catch(...){++rejected;}}
  if(role==Role::Host){
   for(auto it=joined.begin();it!=joined.end();){if(now-it->second>6000)it=joined.erase(it);else ++it;}
   if(now>=nextState){nextState=now+500;Message m;m.kind="state";m.room=hostRoom;m.t2=now;m.track=song(local);m.playing=local.playing;m.rate=local.rate;auto position=local.at(now);m.position=std::max<int64_t>(0,position);if(!local.connected||position<0)m.track={};for(auto& p:joined)send(p.first,m);}
   status="Host; joined peers="+std::to_string(joined.size());
  }else{
   if(room&&lastHostSeen>=0&&now-lastHostSeen>6000){room=0;clock={};receivedAt=-1;status="Host timeout; controls suspended";nextHello=now;}
   if(!room&&now>=nextHello){Message m;m.kind="hello";m.cookie=helloCookie;send(authority,m);nextHello=now+1500;}
   if(room&&now>=nextClock){Message m;m.kind="clock";m.room=room;m.cookie=helloCookie;m.t0=now;clockSentAt=now;send(authority,m);nextClock=now+2000;}
   if(room&&receivedAt>=0)status=correction(local,now).reason;
  }
 }
 Plan correction(const State& local,int64_t now)const{if(role!=Role::Follower||!room||receivedAt<0){Plan p;p.reason="Waiting for host handshake and playback";return p;}return plan(local,latest,clock,now,receivedAt);}
 const Clock& timing()const{return clock;}
};
}
