#pragma once
#include <vector>
#include <string>
#include <algorithm>
#include <cstdint>
#include <cmath>
namespace dango {
struct ClockSelection {int64_t position=0;bool seek=false;};
struct PlaybackCursor {
 std::string key;int64_t last=-1;
 ClockSelection select(const std::string& song,int64_t position,bool playing){
 bool changed=key!=song;if(changed){key=song;last=-1;}
 bool seek=false;
 if(last>=0&&position>=0){int64_t delta=position-last;if(playing&&delta<0&&delta>=-300)position=last;else if(delta< -300||delta>1800)seek=true;}
 last=position;return {position,seek};
 }
};
struct LyricLease {uint64_t id=0;int index=-1;std::string text;double retired=-1,expires=-1;};
// One active sentence plus one bounded outgoing sentence. Once retired, an
// instance is never reactivated. A genuine seek clears both before selection.
struct LyricLifecycle {
 std::vector<LyricLease> entries;std::string key;int current=-1;uint64_t serial=0;
 void reset(){entries.clear();current=-1;key.clear();}
 void update(const std::string& song,int index,const std::string& text,double now,int lingerMs,int fadeMs,bool discontinuity=false){
 if(discontinuity||song!=key){reset();key=song;}
 entries.erase(std::remove_if(entries.begin(),entries.end(),[&](auto& e){return e.expires>=0&&now>=e.expires;}),entries.end());
 if(index==current)return;
 // Drop any older outgoing entry; only the immediately previous active line may linger.
 entries.erase(std::remove_if(entries.begin(),entries.end(),[](auto& e){return e.retired>=0;}),entries.end());
 for(auto& e:entries){e.retired=now;e.expires=now+(std::max(0,lingerMs)+std::max(1,fadeMs))/1000.;}
 current=index;
 if(index<0||text.empty()){entries.clear();return;}
 // Repeated chorus text is a new occurrence, but must not overlap its old copy.
 entries.erase(std::remove_if(entries.begin(),entries.end(),[&](auto& e){return e.index==index||e.text==text;}),entries.end());
 entries.push_back({++serial,index,text,-1,-1});
 }
};
// The application-owned buffer must be restored even on Present failure. The
// template is exercised with software surfaces in portable regression tests.
template<class Capture,class Draw,class Present,class Restore>
auto overlayTransaction(Capture capture,Draw draw,Present present,Restore restore)->decltype(present()){
 if(!capture())return present();
 try{draw();auto result=present();restore();return result;}catch(...){restore();throw;}
}
}
