#pragma once
#include "sync/protocol.hpp"
#include <functional>
#include <memory>
inline uint64_t GetTickCount64(){return 1000;}
namespace dango::sync::steam {
inline unsigned connects=0,pumps=0,sends=0,closes=0;
inline uint64_t nonce(){return 12345;}
class Transport:public sync::Transport {
 std::function<bool()> allowed;
public:
 std::string binding="mock",selfResult="Not run",pairResult="Not run";
 Transport(int,std::function<bool()> guard):allowed(std::move(guard)){}
 ~Transport(){++closes;}
 void connect(){assert(allowed());++connects;}
 Peer self()const override{return 76561197960265729ULL;}
 void allow(const std::set<Peer>&){}
 bool send(Peer,const std::string&)override{if(!allowed())return false;++sends;return true;}
 bool receive(Packet&)override{return false;}
 void pump(int64_t){if(allowed())++pumps;}
};
}
