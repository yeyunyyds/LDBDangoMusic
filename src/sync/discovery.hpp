#pragma once
#include "roster.hpp"
namespace dango::sync {
struct Leader {Peer id=0;uint64_t term=0,boot=0;bool operator==(const Leader& b)const{return id==b.id&&term==b.term&&boot==b.boot;}bool operator!=(const Leader& b)const{return !(*this==b);}};
struct RoomMessage {std::string kind;Peer sender=0;uint64_t cohort=0,boot=0,echo=0,seq=0,term=0;bool enabled=false,lx=false;};
inline std::string roomEncode(const RoomMessage& m){return nlohmann::json{{"app","L4D2DangoMusic.room"},{"v",1},{"protocol",3},{"kind",m.kind},{"sender",std::to_string(m.sender)},{"scope",std::to_string(m.cohort)},{"boot",std::to_string(m.boot)},{"echo",std::to_string(m.echo)},{"seq",m.seq},{"term",std::to_string(m.term)},{"enabled",m.enabled},{"lx",m.lx}}.dump();}
inline RoomMessage roomDecode(const Packet& p){auto j=flatObject(p.bytes,2048);if(j.size()!=12||j.at("app")!="L4D2DangoMusic.room"||j.at("v")!=1||j.at("protocol")!=3)throw std::runtime_error("Room protocol mismatch");RoomMessage m;m.kind=j.at("kind").get<std::string>();if(m.kind!="discover"&&m.kind!="discovered"&&m.kind!="presence"&&m.kind!="claim")throw std::runtime_error("Unknown room message");m.sender=peer(j.at("sender").get<std::string>());if(m.sender!=p.from)throw std::runtime_error("Room identity mismatch");m.cohort=unsignedText(j,"scope");m.boot=unsignedText(j,"boot");m.echo=unsignedText(j,"echo");m.term=unsignedText(j,"term");if(!m.boot||!m.cohort||m.term>1000000000000ULL||!j.at("seq").is_number_unsigned())throw std::runtime_error("Invalid room fields");m.seq=j.at("seq").get<uint64_t>();m.enabled=j.at("enabled").get<bool>();m.lx=j.at("lx").get<bool>();return m;}
struct Member {bool confirmed=false,enabled=false,lx=false;uint64_t boot=0,seq=0,term=0;int64_t seen=0,nextProbe=0;unsigned attempts=0;std::set<uint64_t> retired;bool eligible()const{return confirmed&&enabled&&lx;}};
class Discovery {
 Transport& io;Roster roster;uint64_t boot=0,sequence=0,term=0,maxSeen=0;bool enabled=false,lx=false;int64_t started=0,nextPresence=0;std::map<Peer,Member> members;Leader current;
 bool eligible()const{return enabled&&lx;}
 void send(Peer id,const std::string& kind,uint64_t echo=0){RoomMessage m{kind,io.self(),roster.cohort(),boot,echo,++sequence,eligible()?term:0,enabled,lx};if(!io.send(id,roomEncode(m)))++sendFailures;else ++sent;}
 void announce(const std::string& kind){for(auto& p:members)if(p.second.confirmed)send(p.first,kind);}
 Leader select()const{Leader best;if(eligible()&&term)best={io.self(),term,boot};for(auto& p:members){auto& m=p.second;if(m.eligible()&&m.term&&(m.term>best.term||(m.term==best.term&&p.first>best.id)))best={p.first,m.term,m.boot};}return best;}
public:
 uint64_t sent=0,received=0,rejected=0,sendFailures=0;
 Discovery(Transport& transport,Roster scope,uint64_t incarnation,int64_t now):io(transport),roster(std::move(scope)),boot(incarnation),started(now){if(!boot||roster.local!=io.self()||roster.players.size()>4)throw std::runtime_error("Invalid discovery scope");for(auto id:roster.others())members[id]={};}
 void rescan(int64_t now){for(auto& p:members)if(!p.second.confirmed){p.second.attempts=0;p.second.nextProbe=now;}}
 bool claim(int64_t now){if(!eligible())return false;term=++maxSeen;announce("claim");nextPresence=now+1000;current=select();return true;}
 void tick(bool participating,bool lxReady,int64_t now){
  bool change=enabled!=participating||lx!=lxReady;enabled=participating;lx=lxReady;if(!eligible())term=0;if(change){announce("presence");nextPresence=now+1000;}
  Packet p;for(int budget=0;budget<24&&io.receive(p);++budget){try{auto m=roomDecode(p);auto it=members.find(m.sender);if(it==members.end()||m.cohort!=roster.cohort()||it->second.retired.count(m.boot))throw std::runtime_error("Room sender/scope rejected");auto& peer=it->second;++received;
   if(m.kind=="discover"){if(peer.boot!=m.boot){peer.confirmed=false;peer.term=0;peer.attempts=0;peer.nextProbe=now;}send(m.sender,"discovered",m.boot);continue;}
   if(m.kind=="discovered"){if(m.echo!=boot||(m.boot==peer.boot&&m.seq<=peer.seq))throw std::runtime_error("Old discovery response");if(peer.boot&&peer.boot!=m.boot){peer.retired.insert(peer.boot);if(peer.retired.size()>4)peer.retired.erase(peer.retired.begin());}peer.confirmed=true;peer.boot=m.boot;peer.attempts=0;}
   else if(!peer.confirmed||m.boot!=peer.boot||m.seq<=peer.seq)throw std::runtime_error("Unconfirmed or repeated room message");
   peer.seq=m.seq;peer.seen=now;peer.enabled=m.enabled;peer.lx=m.lx;peer.term=peer.eligible()?m.term:0;maxSeen=std::max(maxSeen,m.term);
  }catch(...){++rejected;}}
  static constexpr int delays[]={500,1000,2000,4000,8000,15000,30000,60000};
  for(auto& p:members){auto& member=p.second;if(member.confirmed&&now-member.seen>7000){member.confirmed=false;member.term=0;member.attempts=0;member.nextProbe=now;}
   if(!member.confirmed&&member.attempts<8&&now>=member.nextProbe){send(p.first,"discover");member.nextProbe=now+delays[member.attempts++];}}
  current=select();if(!current.id&&eligible()&&now-started>=1000){for(auto id:roster.players){bool candidate=id==io.self()?eligible():members.count(id)&&members.at(id).eligible();if(candidate){if(id==io.self())claim(now);break;}}}
  if(now>=nextPresence){announce("presence");nextPresence=now+1000;}current=select();
 }
 Leader leader()const{return current;}
 const std::map<Peer,Member>& directory()const{return members;}
 std::set<Peer> participants()const{std::set<Peer> out;for(auto& p:members)if(p.second.eligible())out.insert(p.first);return out;}
};
}
