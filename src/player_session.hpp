#pragma once
#include "core.hpp"
namespace dango {
// Owns one player connection/playback generation. Metadata and lyrics from
// different generations must never be combined, even for the same song key.
inline bool sameTimeline(const Timeline& a,const Timeline& b){
 if(a.size()!=b.size())return false;for(size_t i=0;i<a.size();++i){auto& x=a[i];auto& y=b[i];if(x.start!=y.start||x.text!=y.text||x.translation!=y.translation||x.words.size()!=y.words.size())return false;
 for(size_t j=0;j<x.words.size();++j){auto& u=x.words[j];auto& v=y.words[j];if(u.start!=v.start||u.duration!=v.duration||u.text!=v.text)return false;}}return true;
}
struct PlayerSession {
 uint64_t generation=0;bool online=false,playing=false;std::string key;
 Timeline timeline,lastAccepted;std::string lastAcceptedKey;int64_t retryAt=0,refreshAt=0,changedAt=0;unsigned refreshStep=0;State previous;
 void invalidate(){++generation;timeline.clear();retryAt=refreshAt=0;refreshStep=0;}
 State attach(State state)const{
 state.generation=generation;
 state.timeline=state.connected&&state.playing&&state.key==key?timeline:Timeline{};
 if(!state.playing)state.message=state.key.empty()?"No active track":"Paused";
 else state.message=state.timeline.empty()?"Waiting for synchronized lyrics":"Connected to BLF / LX";
 return state;
 }
 State observe(State state){
 int64_t elapsed=state.sampled-previous.sampled;
 int64_t expected=previous.position+(previous.playing?int64_t(std::max<int64_t>(0,elapsed)*previous.rate):0);
 expected=std::min(expected,previous.duration);
 bool seek=online&&key==state.key&&!key.empty()&&elapsed>=0&&(state.position-expected< -300||state.position-expected>1800);
 if(!online||key!=state.key||playing!=state.playing||seek){invalidate();changedAt=state.sampled;}
 online=state.connected;playing=state.playing;key=state.key;
 previous=state;previous.timeline.clear();return attach(std::move(state));
 }
 State disconnect(int64_t tick){
 if(online||!key.empty()||!timeline.empty())invalidate();
 online=false;playing=false;key.clear();previous={};State state;state.sampled=tick;state.generation=generation;state.message="Disconnected: enable BLF / LX local API";return state;
 }
 bool needsLyrics(int64_t tick)const{return online&&playing&&!key.empty()&&previous.position<previous.duration&&tick>=retryAt&&(timeline.empty()||(refreshAt>0&&tick>=refreshAt));}
 void scheduleRefresh(int64_t tick){const int delays[]={500,1000,2000,4000,8000,14000};refreshAt=tick+(refreshStep<6?delays[refreshStep++]:30000);}
 template<class ReadStatus,class ReadLyrics,class Clock,class Publish>
 void poll(ReadStatus readStatus,ReadLyrics readLyrics,Clock clock,Publish publish){
 State state=observe(readStatus());publish(state);
 if(!needsLyrics(clock()))return;
 auto requestedGeneration=generation;auto requestedKey=key;retryAt=clock()+1000;
 Timeline next;
 try{next=readLyrics();}catch(...){state.message="Lyric request failed; retrying";publish(state);return;}
 // The second status request is deliberately outside the lyric-only catch:
 // losing the status API invalidates the whole connection in the caller.
 state=observe(readStatus());
 if(generation==requestedGeneration&&key==requestedKey&&playing){
 // /status can switch before /lyric-all. Briefly reject a byte-for-byte
 // previous-track response, then allow legitimate duplicate-lyrics tracks.
 if(!next.empty()&&lastAcceptedKey!=key&&sameTimeline(next,lastAccepted)&&clock()-changedAt<2000){state.message="Waiting for new track lyrics";publish(attach(std::move(state)));return;}
 bool changed=!sameTimeline(timeline,next);timeline=std::move(next);if(changed)++generation;
 if(!timeline.empty()){lastAccepted=timeline;lastAcceptedKey=key;scheduleRefresh(clock());retryAt=0;}
 else retryAt=clock()+1000;
 }
 publish(attach(std::move(state)));
 }
};
struct PlaybackDisplayFrame {bool metadata=false,lyrics=false,reset=false;int64_t position=-1;};
struct PlaybackDisplay {
 std::string key;uint64_t generation=0;bool metadata=false,lyrics=false,initialized=false;
 PlaybackDisplayFrame select(const State& state,int64_t tick){
 auto position=state.at(tick);
 bool valid=state.connected&&state.playing&&!state.key.empty()&&position>=0&&position<state.duration;
 bool ready=valid&&!state.timeline.empty();
 bool reset=!initialized||generation!=state.generation||key!=state.key||metadata!=valid||lyrics!=ready;
 initialized=true;generation=state.generation;key=state.key;metadata=valid;lyrics=ready;
 return {valid,ready,reset,ready?position:-1};
 }
};
}
