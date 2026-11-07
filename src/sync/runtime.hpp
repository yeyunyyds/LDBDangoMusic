#pragma once
#include "../build_features.hpp"
#ifndef DANGO_SYNC_STEAM_HEADER
#define DANGO_SYNC_STEAM_HEADER "steam_transport.hpp"
#endif
#include DANGO_SYNC_STEAM_HEADER
#include "session.hpp"
#include "lx_control.hpp"
#include "settings.hpp"
#if DANGO_DIAGNOSTICS
#include "simulation.hpp"
#endif
#include "source_policy.hpp"
#include "discovery.hpp"
#include "song_switch.hpp"
#include <mutex>
namespace dango::sync {
struct Request {int action=0;lx::Command command;Song track;};
class Requests {std::mutex mutex;std::deque<Request> items;public:bool push(Request r){std::lock_guard<std::mutex> lock(mutex);if(items.size()>=16)return false;items.push_back(std::move(r));return true;}std::deque<Request> drain(){std::lock_guard<std::mutex> lock(mutex);std::deque<Request> result;result.swap(items);return result;}};
struct Status {
 std::string activeSource=u8"未连接",simulated="Not run",control="Not run",network="Disabled",self="Not run",create="Removed",pair="Not run",progress="Together listening disabled",search="No search requested",roster="Waiting for game roster",role="Off";
 bool lxActive=false;uint64_t sourceRevision=0,sourceEpoch=0;Peer identity=0,host=0;uint64_t term=0;int64_t drift=0,rtt=0;uint64_t sent=0,received=0,rejected=0;std::vector<std::string> members;
};
class Runtime {
 struct RetryPending:std::runtime_error{using std::runtime_error::runtime_error;};
 bool networkOpen=false;std::function<bool()> networkGuard;
 bool allowed()const{return networkOpen&&(!networkGuard||networkGuard());}
 Requests& requests;
 std::unique_ptr<steam::Transport> transport,roomTransport;
 std::unique_ptr<Discovery> discovery;std::unique_ptr<Session> session;
 std::string signature,scopeSignature,memberDigest;
 int64_t retryAt=0,roomRetryAt=0,nextCorrection=0,claimUntil=0;
 uint64_t commandId=0,lastSourceEpoch=0,lastSourceRevision=0;bool lastSourceLx=false;
 std::deque<lx::Command> commands;lx::Executor executor;SongSwitch songSwitch;Status report;
 void(*log)(const std::string&);bool enabledBefore=false,executorAutomatic=false;
 void ensure(int64_t now){if(transport)return;if(now<retryAt)throw RetryPending("Steam bind retry cooling down");retryAt=now+5000;auto next=std::make_unique<steam::Transport>(45631,[this](){return allowed();});next->connect();transport=std::move(next);if(log)log("Steam progress bound self="+std::to_string(transport->self())+" "+transport->binding);}
 void ensureRoom(int64_t now){if(roomTransport)return;if(now<roomRetryAt)throw RetryPending("Steam discovery bind retry cooling down");roomRetryAt=now+5000;auto next=std::make_unique<steam::Transport>(45630,[this](){return allowed();});next->connect();roomTransport=std::move(next);if(log)log("Steam discovery bound self="+std::to_string(roomTransport->self())+" channel=45630");}
 void clearAutomatic(){commands.erase(std::remove_if(commands.begin(),commands.end(),[](const lx::Command& c){return c.automatic;}),commands.end());if(executorAutomatic)executor.cancel();songSwitch.reset();nextCorrection=0;}
public:
 explicit Runtime(Requests& q,void(*logger)(const std::string&)):requests(q),log(logger){}
 void reset(){networkOpen=false;session.reset();discovery.reset();transport.reset();roomTransport.reset();networkGuard={};signature.clear();scopeSignature.clear();memberDigest.clear();commands.clear();executor.cancel();songSwitch.reset();claimUntil=retryAt=roomRetryAt=nextCorrection=0;enabledBefore=false;lastSourceEpoch=lastSourceRevision=0;lastSourceLx=false;report=Status{};}
 template<class Read,class Http> Status tick(const Settings& cfg,const State& state,int64_t now,Read readStatus,Http http,SourceContext source={},const Roster& roster={},const std::function<void(const std::string&)>& scheme={},const std::function<bool()>& permission={}){
  bool lxActive=lxAllowed(source);report.activeSource=providerName(source.provider);report.lxActive=lxActive;report.sourceRevision=source.revision;report.sourceEpoch=source.epoch;
  if(lastSourceEpoch!=source.epoch||lastSourceRevision!=source.revision||lastSourceLx!=lxActive){commands.clear();executor.cancel();session.reset();signature.clear();songSwitch.reset();lastSourceEpoch=source.epoch;lastSourceRevision=source.revision;lastSourceLx=lxActive;}
  networkOpen=cfg.enabled&&cfg.discover&&lxActive;networkGuard=permission;
  if(!allowed()){
   auto pending=requests.drain();bool blockedUser=false;for(auto& request:pending)if(request.action==2)blockedUser=true;
   session.reset();discovery.reset();transport.reset();roomTransport.reset();signature.clear();scopeSignature.clear();memberDigest.clear();commands.clear();executor.cancel();songSwitch.reset();claimUntil=0;retryAt=roomRetryAt=0;enabledBefore=false;
   report=Status{};report.activeSource=providerName(source.provider);report.lxActive=lxActive;report.sourceRevision=source.revision;report.sourceEpoch=source.epoch;report.progress="Networking disabled; no plugin Steam/P2P activity";report.search="Search blocked while networking is disabled";if(blockedUser)report.create="BLOCKED: Steam user lifecycle APIs removed";return report;
  }
  bool rescan=false,retrySearch=false;
  for(auto& request:requests.drain()){
   #if DANGO_DIAGNOSTICS
   if(request.action==0){if(!currentCommand(source,request.command.sourceRevision,request.command.sourceEpoch)){report.control="LX-only control blocked: source changed or is not LX";continue;}if(commands.size()<16)commands.push_back(std::move(request.command));continue;}
   if(request.action==2){report.create="BLOCKED: Steam user lifecycle APIs removed";if(log)log("Legacy user probe rejected before SDK binding");continue;}
   if(request.action==5){try{report.simulated=virtualPlayersTest();}catch(const std::exception& e){report.simulated=e.what();}if(log)log(report.simulated);continue;}
   #endif
   if(request.action==6){claimUntil=now+5000;continue;}if(request.action==8){rescan=true;continue;}if(request.action==9){retrySearch=true;continue;}
   #if DANGO_DIAGNOSTICS
   if(request.action==7){try{if(!currentCommand(source,request.command.sourceRevision,request.command.sourceEpoch)||request.command.expectedTrack!=state.key||state.at(now)<0||!scheme)throw std::runtime_error("LX Scheme unavailable for this source");scheme(searchPlayUri(request.track));report.search="Current song searchPlay dispatched; verify actual LX track";if(log)log("LX Scheme current-song test dispatched");}catch(const std::exception& e){report.search=e.what();}continue;}
   try{if(log)log("Steam diagnostic begin action="+std::to_string(request.action));ensure(now);if(request.action==1)transport->selfTest(now);if(request.action==3)transport->socketPairTest(now);}catch(const std::exception& e){report.network=e.what();if(log)log(report.network);}
   #endif
  }
  auto observed=executor.observe(state,now);if(observed.id&&!observed.message.empty()){report.control=observed.message;if(log)log("LX callback id="+std::to_string(observed.id)+" verified="+std::to_string(observed.verified)+" "+observed.message);}
  report.roster=roster.status;report.members.clear();Leader leader;std::set<Peer> peers;
  try{
   if(cfg.discover&&roster.fresh(now)){
    ensureRoom(now);report.identity=roomTransport->self();if(roster.local!=report.identity)throw std::runtime_error("Game roster/Steam local identity mismatch");
    auto key=std::to_string(roster.generation)+"/"+std::to_string(roster.cohort());
    if(key!=scopeSignature){discovery.reset();roomTransport->allow(roster.others());discovery=std::make_unique<Discovery>(*roomTransport,roster,steam::nonce(),now);scopeSignature=key;session.reset();signature.clear();clearAutomatic();if(log)log("Steam room census generation="+std::to_string(roster.generation)+" candidates="+std::to_string(roster.others().size()));}
    roomTransport->pump(now);if(rescan)discovery->rescan(now);discovery->tick(cfg.enabled,lxActive,now);
    if(claimUntil&&now<=claimUntil&&cfg.enabled&&lxActive){if(discovery->claim(now)){claimUntil=0;if(log)log("Steam host claim announced by local user");}}
    if(claimUntil&&now>claimUntil){claimUntil=0;report.network="Host claim expired while waiting for game/LX readiness";}
    leader=discovery->leader();peers=discovery->participants();
    std::string digest;for(auto& p:discovery->directory())digest+=std::to_string(p.first)+":"+std::to_string(p.second.confirmed)+"/"+std::to_string(p.second.enabled)+"/"+std::to_string(p.second.lx)+";";if(digest!=memberDigest){memberDigest=digest;if(log)log("Steam room installed/enabled/lx peers="+digest);}
    for(auto& item:discovery->directory()){auto& m=item.second;report.members.push_back(std::to_string(item.first)+(m.confirmed?(m.eligible()?u8" · 已安装，一起听已开":m.enabled?u8" · 已安装，当前非 LX":u8" · 已安装，一起听已关"):u8" · 未响应或版本不兼容"));}
   }else{
    if(discovery){discovery.reset();scopeSignature.clear();if(roomTransport)roomTransport->allow({});session.reset();signature.clear();clearAutomatic();}
   }
  }catch(const RetryPending& e){report.roster=e.what();}
  catch(const std::exception& e){report.roster=e.what();discovery.reset();scopeSignature.clear();session.reset();signature.clear();clearAutomatic();roomTransport.reset();roomRetryAt=now+5000;}
  bool participating=cfg.enabled&&cfg.discover&&lxActive&&roster.fresh(now)&&discovery&&leader.id;
  report.host=leader.id;report.term=leader.term;report.role=participating?(leader.id==report.identity?"Host":"Follower"):"Off";
  try{
   auto key=participating?std::to_string(source.epoch)+"/"+scopeSignature+"/"+std::to_string(leader.id)+"/"+std::to_string(leader.term)+"/"+std::to_string(leader.boot):"off";
   if(participating)ensure(now);
   for(auto peer:peers)key+="/"+std::to_string(peer);
   if(signature!=key){session.reset();clearAutomatic();if(transport)transport->allow(participating?peers:std::set<Peer>{});signature=key;if(participating){session=std::make_unique<Session>(*transport,leader.id==transport->self()?Role::Host:Role::Follower,peers,leader.id,steam::nonce(),roster.cohort());if(log)log("Steam room host="+std::to_string(leader.id)+" term="+std::to_string(leader.term)+" members="+std::to_string(peers.size()+1));}}
   if(transport){transport->pump(now);report.identity=transport->self();
 #if DANGO_DIAGNOSTICS
 if(log&&report.self!=transport->selfResult)log("Steam self test: "+transport->selfResult);if(log&&report.pair!=transport->pairResult)log("Steam CreateSocketPair: "+transport->pairResult);report.self=transport->selfResult;report.pair=transport->pairResult;
 #endif
 report.network=transport->binding;}
   if(session){
    session->tick(state,now);report.network=session->status+" sendFailures="+std::to_string(session->sendFailures);report.sent=session->sent;report.received=session->received;report.rejected=session->rejected;report.rtt=session->timing().rtt;
    auto decision=session->correction(state,now,cfg.threshold);report.drift=decision.drift;bool follower=leader.id!=report.identity;report.progress=follower?decision.reason+(cfg.apply?"; APPLY":"; DRY RUN"):"Publishing local LX progress";
    if(follower&&cfg.apply&&cfg.autoSearch){if(retrySearch)songSwitch.retry(now);if(auto remote=session->remote(now)){if(scheme)songSwitch.tick(remote->track,remote->playing,state,now,[&](const std::string& uri){scheme(uri);if(log)log("LX automatic searchPlay dispatched for host="+std::to_string(leader.id));});else report.search="LX Scheme dispatcher unavailable";}report.search=songSwitch.status;}
    if(follower&&cfg.apply&&decision.valid&&now>=nextCorrection&&!executor.busy()&&commands.empty()){
     if(decision.pause)commands.push_back({++commandId,lx::Action::Pause,0,state.key,true});if(decision.seek)commands.push_back({++commandId,lx::Action::Seek,decision.target,state.key,true});if(decision.play)commands.push_back({++commandId,lx::Action::Play,0,state.key,true});if(!commands.empty())nextCorrection=now+1200;
    }
   }else report.progress=lxActive?(cfg.enabled?"Waiting for same-game plugin discovery/host":"Together listening off; discovery can remain active"):"LX-only synchronization suspended for this source";
  }catch(const RetryPending& e){report.network=e.what();}
  catch(const std::exception& e){report.network=e.what();session.reset();signature.clear();clearAutomatic();transport.reset();retryAt=now+5000;}
  bool following=participating&&leader.id!=report.identity&&cfg.apply;if(enabledBefore&&!following)clearAutomatic();enabledBefore=following;
  if(!executor.busy()&&!commands.empty()){
   auto command=std::move(commands.front());commands.pop_front();if(!command.id)command.id=++commandId;if(command.automatic){command.sourceRevision=source.revision;command.sourceEpoch=source.epoch;}if(!currentCommand(source,command.sourceRevision,command.sourceEpoch)){commands.clear();return report;}
   try{auto fresh=readStatus();if(command.automatic){auto latest=session?session->correction(fresh,int64_t(GetTickCount64()),cfg.threshold):Plan{};if(!following||!latest.valid||(command.action==lx::Action::Play&&!latest.play)||(command.action==lx::Action::Pause&&!latest.pause)||(command.action==lx::Action::Seek&&!latest.seek)){clearAutomatic();return report;}if(command.action==lx::Action::Seek)command.position=latest.target;}
    executorAutomatic=command.automatic;auto result=executor.begin(command,fresh,now,[&](const std::string& path){auto verify=readStatus();if(verify.key!=command.expectedTrack||!verify.connected)throw std::runtime_error("Track changed before LX command");if(log)log("LX request "+path+" id="+std::to_string(command.id));http(path);},[](){return int64_t(GetTickCount64());});report.control=result.message;if(log)log("LX callback id="+std::to_string(result.id)+" accepted="+std::to_string(result.accepted)+" "+result.message);
   }catch(const std::exception& e){report.control=e.what();commands.clear();executor.cancel();if(log)log(report.control);}
  }
  return report;
 }
};
}
