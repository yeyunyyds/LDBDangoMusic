#pragma once
#include "protocol.hpp"
namespace dango::sync {
inline std::string urlEncode(const std::string& value){const char* digits="0123456789ABCDEF";std::string out;for(unsigned char c:value){if((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='-'||c=='_'||c=='.'||c=='~')out+=char(c);else{out+='%';out+=digits[c>>4];out+=digits[c&15];}}return out;}
inline size_t utf16Units(const std::string& text){size_t count=0;for(unsigned char c:text){if((c&0xc0)==0x80)continue;count+=(c&0xf8)==0xf0?2:1;}return count;}
inline std::string searchPlayUri(const Song& song){if(song.title.empty()||utf16Units(song.title)>200||utf16Units(song.artist)>200||song.duration<=0||song.duration>86400000)throw std::runtime_error("Song metadata cannot be passed to LX Scheme");auto seconds=(song.duration+500)/1000;auto interval=(seconds/60<10?"0":"")+std::to_string(seconds/60)+":"+(seconds%60<10?"0":"")+std::to_string(seconds%60);nlohmann::json data={{"name",song.title},{"singer",song.artist},{"interval",interval},{"playLater",false}};return "lxmusic://music/searchPlay?data="+urlEncode(data.dump());}
class SongSwitch {
 Song target;bool haveTarget=false,pending=false;int64_t stableAt=0,deadline=0,retryAt=0,lastDispatch=-100000;unsigned attempts=0;
public:
 std::string status="No automatic song request";
 void reset(){haveTarget=pending=false;attempts=0;status="Waiting for host track";}
 void retry(int64_t now){pending=false;attempts=0;retryAt=now;stableAt=now-750;}
 template<class Dispatch>void tick(const Song& desired,bool hostPlaying,const State& local,int64_t now,Dispatch dispatch){
  if(!haveTarget||target.title!=desired.title||target.artist!=desired.artist||target.duration!=desired.duration){target=desired;haveTarget=true;pending=false;attempts=0;stableAt=now;retryAt=now;}
  if(matches(song(local),desired)){pending=false;attempts=0;status="Selected local song verified; ready to align progress";return;}
  if(!hostPlaying){status="Host paused; wait before starting a different song";return;}
  if(pending&&now<deadline){status="LX searchPlay dispatched; waiting for title/artist/duration verification";return;}
  if(pending){pending=false;retryAt=now+30000;status="Selected song not verified; retry after cooldown";}
  if(attempts>=3){status="No matching version after 3 attempts; select locally or retry";return;}
  if(now-stableAt<750||now<retryAt||now-lastDispatch<2000)return;
  ++attempts;lastDispatch=now;try{dispatch(searchPlayUri(desired));pending=true;deadline=now+20000;status="LX searchPlay accepted by OS; awaiting actual player state";}catch(const std::exception& e){status=e.what();retryAt=now+30000;}
 }
};
}
