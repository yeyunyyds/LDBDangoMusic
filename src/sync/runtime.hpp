#pragma once
#include "steam_transport.hpp"
#include "session.hpp"
#include "lx_control.hpp"
#include "settings.hpp"
#include "simulation.hpp"
#include <mutex>
namespace dango::sync {
struct Request {int action=0;lx::Command command;};
class Requests {std::mutex mutex;std::deque<Request> items;public:bool push(Request r){std::lock_guard<std::mutex> lock(mutex);if(items.size()>=16)return false;items.push_back(std::move(r));return true;}std::deque<Request> drain(){std::lock_guard<std::mutex> lock(mutex);std::deque<Request> result;result.swap(items);return result;}};
struct Status {std::string simulated="Not run",control="Not run",network="Disabled",self="Not run",create="Not run",pair="Not run",progress="Dry run; no remote control applied";Peer identity=0;int64_t drift=0,rtt=0;uint64_t sent=0,received=0,rejected=0;};
class Runtime {
 Requests& requests;std::unique_ptr<steam::Transport> transport;std::unique_ptr<Session> session;std::string signature;int64_t retryAt=0,nextCorrection=0;uint64_t commandId=0;std::deque<lx::Command> commands;lx::Executor executor;Status report;void(*log)(const std::string&);bool enabledBefore=false,executorAutomatic=false;
 void ensure(int64_t now){if(transport)return;if(now<retryAt)throw std::runtime_error("Steam bind retry cooling down");retryAt=now+5000;auto next=std::make_unique<steam::Transport>();next->connect();transport=std::move(next);if(log)log("Steam sync bound self="+std::to_string(transport->self())+" "+transport->binding);}
public:
 explicit Runtime(Requests& q,void(*logger)(const std::string&)):requests(q),log(logger){}
 template<class Read,class Http> Status tick(const Settings& cfg,const State& state,int64_t now,Read readStatus,Http http){
  for(auto& r:requests.drain()){
   if(r.action==0){if(commands.size()<16)commands.push_back(std::move(r.command));continue;}
   if(r.action==2){report.create="BLOCKED: CreateLocalUser is a non-thread-safe server-user API; disabled inside the game";if(log)log("Steam user lifecycle probe blocked before SDK binding");continue;}
   if(r.action==5){try{report.simulated=virtualPlayersTest();}catch(const std::exception& e){report.simulated=e.what();}if(log)log(report.simulated);continue;}
   try{if(log)log("Steam diagnostic begin action="+std::to_string(r.action));ensure(now);if(r.action==1)transport->selfTest(now);if(r.action==3)transport->socketPairTest(now);if(log)log("Steam diagnostic action="+std::to_string(r.action));}catch(const std::exception& e){report.network=e.what();if(log)log("Steam diagnostic: "+report.network);}
  }
  auto observed=executor.observe(state,now);if(observed.id&&!observed.message.empty()){report.control=observed.message;if(log)log("LX callback id="+std::to_string(observed.id)+" verified="+std::to_string(observed.verified)+" "+observed.message);}
  try{
   if(cfg.role)ensure(now);
   if(transport){auto next=std::to_string(cfg.role)+"/"+std::to_string(cfg.host);for(auto p:cfg.peers)next+="/"+std::to_string(p);
    if(signature!=next){session.reset();transport->allow(cfg.role?cfg.peers:std::set<Peer>{});signature=next;if(cfg.role){session=std::make_unique<Session>(*transport,cfg.role==1?Role::Host:Role::Follower,cfg.peers,cfg.host,steam::nonce());}nextCorrection=0;}
    transport->pump(now);report.identity=transport->self();if(log&&report.self!=transport->selfResult)log("Steam self test: "+transport->selfResult);if(log&&report.pair!=transport->pairResult)log("Steam CreateSocketPair: "+transport->pairResult);report.self=transport->selfResult;report.pair=transport->pairResult;report.network=transport->binding;
   }
   if(session){session->tick(state,now);report.network=session->status+" sendFailures="+std::to_string(session->sendFailures);report.sent=session->sent;report.received=session->received;report.rejected=session->rejected;report.rtt=session->timing().rtt;
    auto decision=session->correction(state,now);report.drift=decision.drift;report.progress=cfg.role==1?"Publishing local playback; no remote controls on host":decision.reason+(cfg.apply?"; APPLY":"; DRY RUN");
    if(cfg.role==2&&cfg.apply&&decision.valid&&now>=nextCorrection&&!executor.busy()&&commands.empty()){
     if(decision.pause)commands.push_back({++commandId,lx::Action::Pause,0,state.key,true});
     if(decision.seek)commands.push_back({++commandId,lx::Action::Seek,decision.target,state.key,true});
     if(decision.play)commands.push_back({++commandId,lx::Action::Play,0,state.key,true});
     if(!commands.empty())nextCorrection=now+1200;
    }
   }else report.progress="No sync session; local controls only";
  }catch(const std::exception& e){report.network=e.what();session.reset();signature.clear();transport.reset();retryAt=now+5000;}
  // Turning follow/apply off cancels unsent automatic controls, but not explicit local test actions.
  bool following=cfg.role==2&&cfg.apply;if(enabledBefore&&!following){commands.erase(std::remove_if(commands.begin(),commands.end(),[](const lx::Command& command){return command.automatic;}),commands.end());if(executorAutomatic)executor.cancel();}enabledBefore=following;
  if(!executor.busy()&&!commands.empty()){
   auto c=std::move(commands.front());commands.pop_front();if(!c.id)c.id=++commandId;
   try{auto fresh=readStatus();if(c.automatic){auto latest=session?session->correction(fresh,int64_t(GetTickCount64())):Plan{};if(!following||!latest.valid||(c.action==lx::Action::Play&&!latest.play)||(c.action==lx::Action::Pause&&!latest.pause)||(c.action==lx::Action::Seek&&!latest.seek)){commands.clear();return report;}if(c.action==lx::Action::Seek)c.position=latest.target;}executorAutomatic=c.automatic;auto result=executor.begin(c,fresh,now,[&](const std::string& path){auto verify=readStatus();if(verify.key!=c.expectedTrack||!verify.connected)throw std::runtime_error("Local track changed before LX command");if(log)log("LX request "+path+" id="+std::to_string(c.id));http(path);},[](){return int64_t(GetTickCount64());});report.control=result.message;if(log)log("LX callback id="+std::to_string(result.id)+" accepted="+std::to_string(result.accepted)+" "+result.message);}catch(const std::exception& e){report.control=e.what();commands.clear();executor.cancel();if(log)log("LX callback failed: "+report.control);}
  }
  return report;
 }
};
}
