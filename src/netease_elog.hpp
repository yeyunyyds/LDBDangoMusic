#pragma once
// Decoder/event model adapted from dsh-pet-qqmusic/lib/netease-elog.js (MIT).
// Only playback metadata is retained. Signed music URLs are never stored or logged.
#include "core.hpp"
#include <ctime>
#include <iomanip>
#include <sstream>
namespace dango::netease {
inline unsigned char decodeByte(unsigned char b){unsigned hi=b>>4,lo=b&15;return static_cast<unsigned char>(((hi^((lo+8)%16))%16)*16+(b>>6)*4+((~hi)&3));}
inline std::string idText(const nlohmann::json& j){std::string s;if(j.is_string())s=j.get<std::string>();else if(j.is_number_unsigned())s=std::to_string(j.get<uint64_t>());else if(j.is_number_integer()&&j.get<int64_t>()>0)s=std::to_string(j.get<int64_t>());if(s.empty()||s.size()>20||s.find_first_not_of("0123456789")!=std::string::npos)return {};return s;}
struct Track{std::string id,title,artist;int64_t duration=0;};
inline Track track(const nlohmann::json& input){auto& j=input.contains("track")&&input["track"].is_object()?input["track"]:input;Track t;if(!j.is_object())return t;t.id=idText(j.value("id",nlohmann::json{}));t.title=j.value("name",std::string{});t.duration=j.value("duration",j.value("dt",int64_t(0)));auto artists=j.value("artists",j.value("ar",nlohmann::json::array()));if(artists.is_array())for(auto& a:artists){if(t.artist.size()>2048)break;auto name=a.value("name",std::string{});if(!name.empty()){if(!t.artist.empty())t.artist+='/';t.artist+=name;}}if(t.title.size()>2048||t.artist.size()>2048||t.duration<0||t.duration>86400000)return {};return t;}
struct Parser {
 Track song;uint32_t pid=0;bool playing=false,anchored=false;int64_t position=0,baseTick=0,eventTick=0;std::string carry;bool discard=false;
 void reset(){song={};pid=0;playing=anchored=false;position=baseTick=eventTick=0;carry.clear();discard=false;}
 int64_t at(int64_t tick)const{return std::max<int64_t>(0,position+(playing?std::max<int64_t>(0,tick-baseTick):0));}
 void set(Track t,uint32_t process,int64_t tick,bool play){song=std::move(t);pid=process;playing=play;position=0;baseTick=eventTick=tick;anchored=false;}
 void line(const std::string& raw,int64_t now,int64_t wallNow){try{
  if(raw.find("checkPlayPrivilege")==std::string::npos&&raw.find("playOneTrackInPlayingList")==std::string::npos&&raw.find("setPlayingPosition")==std::string::npos&&raw.find("\"native")==std::string::npos&&raw.find("exitApp")==std::string::npos&&raw.find("exited")==std::string::npos)return;
  static const std::regex head(R"(^\[(\d+):(\d+):(\d{4}/\d{6}):(\d+):([A-Z]+):([\w.\-]+)\((\d+)\)\](?:\s+\[(\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2})\])?)");std::smatch h;if(!std::regex_search(raw,h,head))return;
  auto process=std::stoull(h[1].str());if(!process||process>UINT32_MAX)return;int64_t tick=std::stoll(h[4].str());
  if(h[8].matched){std::tm tm{};std::istringstream stream(h[8].str());stream>>std::get_time(&tm,"%Y-%m-%d %H:%M:%S");tm.tm_isdst=-1;auto stamp=std::mktime(&tm);if(!stream.fail()&&stamp>=0&&std::llabs((wallNow-(now-tick))-int64_t(stamp)*1000)>5000)tick=now-(wallNow-int64_t(stamp)*1000);}
  if(tick<0||tick>now+1000)return;tick=std::min(tick,now);
  std::smatch m;
  if(raw.find("exitApp")!=std::string::npos){if(pid==process)reset();return;}
  if(std::regex_search(raw,m,std::regex(R"(Process\s+(\d+)\s+exited)"))){if(pid==std::stoull(m[1].str()))reset();return;}
  auto json=[&](){auto start=raw.find('{'),end=raw.rfind('}');if(start==std::string::npos||end<=start)throw std::runtime_error("No track object");return nlohmann::json::parse(raw.substr(start,end-start+1));};
  bool privilege=raw.find("\"checkPlayPrivilege\"")!=std::string::npos;
  if(privilege||raw.find("\"playOneTrackInPlayingList\"")!=std::string::npos){auto next=track(json());if(next.id.empty())return;if(next.id!=song.id||pid!=process)set(std::move(next),uint32_t(process),tick,!privilege);else{if(song.title.empty())song=std::move(next);if(!privilege&&!playing){playing=true;baseTick=eventTick=tick;}}return;}
  if(raw.find("\"native")!=std::string::npos&&raw.find("load")!=std::string::npos&&raw.find("\"songId\"")!=std::string::npos){auto j=json();auto id=idText(j.value("songId",nlohmann::json{}));if(id.empty())return;if(id!=song.id||pid!=process){Track t;t.id=id;set(std::move(t),uint32_t(process),tick,true);}else if(playing&&(!anchored||(song.duration>0&&at(tick)>=song.duration-250))){position=0;baseTick=tick;}eventTick=tick;return;}
  if(song.id.empty()||pid!=process)return;
  if(std::regex_search(raw,m,std::regex(R"re("setPlayingPosition"\s*,\s*(-?\d+(?:\.\d+)?))re"))){auto seconds=std::stod(m[1]);if(!std::isfinite(seconds)||seconds<0||seconds>86400)return;position=int64_t(std::llround(seconds*1000));baseTick=eventTick=tick;anchored=true;return;}
  if(std::regex_search(raw,m,std::regex(R"re("native[^"]*state"\s*,\s*(\d+))re"))){auto state=std::stoi(m[1]);if(state!=1&&state!=2)return;position=at(tick);playing=state==1;baseTick=eventTick=tick;anchored=true;}
 }catch(...){/* Ignore unsupported/malformed records, never dump client log data. */}}
 void feed(const char* bytes,size_t size,int64_t tick,int64_t wall){for(size_t i=0;i<size;++i){char c=char(decodeByte(static_cast<unsigned char>(bytes[i])));if(c=='\n'){if(!discard){auto complete=std::move(carry);carry.clear();line(complete,tick,wall);}carry.clear();discard=false;}else if(!discard){if(carry.size()<1024*1024)carry+=c;else{carry.clear();discard=true;}}}}
 State state(int64_t tick)const{State s;s.connected=true;s.sampled=tick;s.message="Netease elog: waiting for a playback event";if(song.id.empty())return s;s.title=song.title;s.artist=song.artist;s.duration=song.duration;s.position=std::min(at(tick),std::max<int64_t>(0,s.duration));s.playing=playing;if(!s.title.empty()&&s.duration>0){s.key="netease-elog\x1f"+std::to_string(pid)+'\x1f'+song.id;s.message="Netease native elog";}else s.message="Netease elog: resolving track metadata";return s;}
};
inline Timeline lyricResponse(const nlohmann::json& j){
 auto text=[&](const char* key){if(!j.contains(key)||!j[key].is_object())return std::string{};auto s=j[key].value("lyric",std::string{});if(s.size()>512*1024)throw std::runtime_error("Netease lyric too large");return s;};
 auto out=parse(text("lrc"));auto tr=parse(text("tlyric"));
 for(auto& line:out){const Line* nearest=nullptr;for(auto& t:tr)if(std::llabs(t.start-line.start)<500&&(!nearest||std::llabs(t.start-line.start)<std::llabs(nearest->start-line.start)))nearest=&t;if(nearest)line.translation=nearest->text;}
 auto yrc=text("yrc");std::istringstream input(yrc);std::string row;static const std::regex lineHeader(R"(^\[(\d+),(\d+)\])"),wordHeader(R"(\((\d+),(\d+),\d+\))");
 while(std::getline(input,row)){std::smatch header;if(!std::regex_search(row,header,lineHeader))continue;auto start=std::stoll(header[1]);Line* nearest=nullptr;for(auto& l:out)if(std::llabs(l.start-start)<=1200&&(!nearest||std::llabs(l.start-start)<std::llabs(nearest->start-start)))nearest=&l;if(!nearest)continue;auto body=row.substr(size_t(header.length()));std::vector<Word> words;for(auto it=std::sregex_iterator(body.begin(),body.end(),wordHeader);it!=std::sregex_iterator();++it){auto next=it;++next;auto at=size_t(it->position()+it->length()),end=next==std::sregex_iterator()?body.size():size_t(next->position());auto absolute=std::stoll((*it)[1]),duration=std::stoll((*it)[2]);if(absolute>86400000||duration>86400000)continue;auto token=body.substr(at,end-at);if(!token.empty())words.push_back({nearest->start+std::max<int64_t>(0,absolute-start),duration,token});}if(!words.empty())nearest->words=std::move(words);}
 return out;
}
}
