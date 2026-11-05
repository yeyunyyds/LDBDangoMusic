#pragma once
#include "core.hpp"
#include <deque>
#include <functional>
namespace dango::lx {
enum class Action{Play,Pause,Seek,Stop};
struct Command{uint64_t id=0;Action action=Action::Pause;int64_t position=0;std::string expectedTrack;bool automatic=false;uint64_t sourceRevision=0,sourceEpoch=0;};
inline std::string path(const Command& c,int64_t duration){
 if(c.action==Action::Play)return "/play";if(c.action==Action::Pause||c.action==Action::Stop)return "/pause";
 if(c.position<0||duration<=0||c.position>duration)throw std::runtime_error("LX seek outside current track");
 auto ms=c.position%1000;return "/seek?offset="+std::to_string(c.position/1000)+"."+std::string(ms<100?ms<10?2:1:0,'0')+std::to_string(ms);
}
struct Result{uint64_t id=0;bool accepted=false,verified=false;std::string message;};
// One queued operation at a time. HTTP 200/OK is only an acknowledgement;
// verification uses a fresh status sample and is never performed on a render callback.
class Executor {
 Command current;bool waiting=false;int64_t deadline=0,started=0;int64_t target=0;
public:
 bool busy()const{return waiting;}
 template<class Request,class Clock> Result begin(Command c,const State& state,int64_t /*tick*/,Request request,Clock clock){
  Result result;result.id=c.id;if(waiting){result.message="Control busy";return result;}
  if(!state.connected||((c.action!=Action::Play||c.automatic)&&state.key.empty())||(!c.expectedTrack.empty()&&c.expectedTrack!=state.key)){result.message="No matching active local track";return result;}
  try{request(path(c,state.duration));if(c.action==Action::Stop){Command seek=c;seek.action=Action::Seek;seek.position=0;request(path(seek,state.duration));}current=std::move(c);target=current.action==Action::Stop?0:current.position;started=clock();deadline=started+2000;waiting=true;result.accepted=true;result.message="HTTP accepted; verifying player state";}catch(const std::exception& e){result.message=e.what();}return result;
 }
 template<class Request> Result begin(Command c,const State& s,int64_t tick,Request request){return begin(std::move(c),s,tick,request,[tick](){return tick;});}
 Result observe(const State& s,int64_t tick){Result r;r.id=current.id;if(!waiting)return r;
  bool track=s.connected&&!s.key.empty()&&(current.expectedTrack.empty()||current.expectedTrack==s.key);bool starting=s.connected&&current.expectedTrack.empty()&&current.action==Action::Play&&s.key.empty();
  bool success=false;if(track&&s.sampled>=started){if(current.action==Action::Play)success=s.playing;else if(current.action==Action::Pause)success=!s.playing;else if(current.action==Action::Stop)success=!s.playing&&s.position<=350;else{auto expected=target+(s.playing?int64_t((s.sampled-started)*s.rate):0);success=std::llabs(s.position-expected)<=650;}}
  if(success){waiting=false;r.accepted=r.verified=true;r.message="Player state verified";}else if((!track&&!starting)||tick>=deadline){waiting=false;r.accepted=true;r.message=track?"HTTP accepted but player state not verified":"Track changed or player disconnected";}return r;
 }
 void cancel(){waiting=false;}
};
}
