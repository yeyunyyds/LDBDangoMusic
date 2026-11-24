#pragma once
#include "core.hpp"
#include <cctype>
namespace dango::folia {
using Json=nlohmann::json;
struct Connection {unsigned port=0;uint32_t pid=0;std::string token;};
inline Connection connection(const Json& j){
 if(!j.is_object()||j.at("version")!=1)throw std::runtime_error("Unsupported Folia connection file");
 auto port=j.at("port").get<int64_t>(),pid=j.at("pid").get<int64_t>();
 auto token=j.at("token").get<std::string>();
 if(port<1||port>65535||pid<1||pid>0xffffffffll||token.empty()||token.size()>512||
 token.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_.~+/=")!=std::string::npos||
 j.at("baseUrl")!="http://127.0.0.1:"+std::to_string(port))throw std::runtime_error("Invalid Folia local connection");
 return {unsigned(port),uint32_t(pid),std::move(token)};
}
inline std::string text(const Json& j,const char* key){
 auto it=j.find(key);if(it==j.end()||it->is_null())return {};
 auto value=it->get<std::string>();if(value.size()>16384)throw std::runtime_error("Folia text too large");return value;
}
inline std::string normalized(std::string s){
 s.erase(std::remove_if(s.begin(),s.end(),[](unsigned char c){return c<128&&std::isspace(c);}),s.end());
 for(char& c:s)if(c>='A'&&c<='Z')c=char(c-'A'+'a');return s;
}
inline State playback(const Json& j,int64_t now){
 State s;s.connected=true;s.sampled=now;s.message="Folia: no active track";
 auto state=j.at("playerState").get<std::string>();
 if(state=="IDLE"||j.at("current").is_null())return s;
 if(state!="PLAYING"&&state!="PAUSED")throw std::runtime_error("Invalid Folia playback state");
 const auto& current=j.at("current");s.title=text(current,"title");s.artist=text(current,"artist");
 s.duration=j.at("durationMs").get<int64_t>();s.position=j.at("positionMs").get<int64_t>();
 if(s.title.empty()||s.duration<=0||s.duration>86400000||s.position<0||s.position>s.duration)throw std::runtime_error("Invalid Folia playback timing");
 s.key="folia\x1f"+text(current,"source")+'\x1f'+text(current,"id")+'\x1f'+s.title+'\x1f'+s.artist;
 s.playing=state=="PLAYING";return s;
}
inline double number(const Json& j,const char* k,double fallback=0){
 auto it=j.find(k);if(it==j.end())return fallback;
 if(!it->is_number())throw std::runtime_error("Invalid Folia lyric time");auto n=it->get<double>();
 if(!std::isfinite(n)||std::abs(n)>86400000)throw std::runtime_error("Invalid Folia lyric time");return n;
}
inline int64_t timeMs(const Json& j,const char* k,double shift){
 auto n=number(j,k);if(n<0||n>86400)throw std::runtime_error("Invalid Folia lyric seconds");
 return std::llround(n*1000+shift);
}
inline Timeline lyrics(const Json& j,const State& track){
 if(j.is_null())return {};
 if(!j.is_object())throw std::runtime_error("Invalid Folia lyric snapshot");
 const auto title=text(j,"title"),artist=text(j,"artist");
 if((!title.empty()&&normalized(title)!=normalized(track.title))||
    (!artist.empty()&&!track.artist.empty()&&normalized(artist)!=normalized(track.artist)))return {};
 const auto shift=number(j,"offset")-number(j,"clipStartMs");
 if(std::abs(shift)>86400000)throw std::runtime_error("Invalid Folia lyric offset");
 const auto& rows=j.at("lines");if(!rows.is_array()||rows.size()>10000)throw std::runtime_error("Invalid Folia lines");
 Timeline result;const bool exact=j.value("wordByWord",false);
 for(const auto& row:rows){
  const auto start=timeMs(row,"startTime",shift),end=timeMs(row,"endTime",shift);
  if(end<=0||end<start||start>86400000)continue;
  Line line;line.start=std::max<int64_t>(0,start);line.text=text(row,"text");line.translation=text(row,"translation");
  if(line.text.empty())continue;
  if(exact&&row.contains("words")){
   const auto& words=row.at("words");if(!words.is_array()||words.size()>4096)throw std::runtime_error("Invalid Folia word count");
   for(const auto& word:words){auto a=timeMs(word,"startTime",shift),b=timeMs(word,"endTime",shift);auto t=text(word,"text");if(t.empty()||b<=0||b<a||a>86400000)continue;a=std::max<int64_t>(0,a);line.words.push_back({a,b-a,std::move(t)});}
  }
  result.push_back(std::move(line));
 }
 std::stable_sort(result.begin(),result.end(),[](const Line& a,const Line& b){return a.start<b.start;});return result;
}
}
