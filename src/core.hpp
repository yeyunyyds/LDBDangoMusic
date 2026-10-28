#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <regex>
#include <string>
#include <vector>
#include "json.hpp"
namespace dango {
struct Word { int64_t start=0,duration=0; std::string text; };
struct Line { int64_t start=0; std::string text,translation; std::vector<Word> words; };
using Timeline=std::vector<Line>;
inline Timeline parse(const std::string& raw, bool lx=false) {
 Timeline out; std::regex stamp(R"(\[(\d+):(\d{2})(?:\.(\d{1,3}))?\])"), word(R"(<(\d+),(\d+)>)");
 int64_t offset=0; std::smatch m; if(std::regex_search(raw,m,std::regex(R"(\[offset:([+-]?\d+)\])"))) offset=std::stoll(m[1]);
 size_t pos=0; while(pos<raw.size()) {auto end=raw.find('\n',pos); auto row=raw.substr(pos,end==std::string::npos?end:end-pos); pos=end==std::string::npos?raw.size():end+1;
 std::vector<int64_t> times; size_t body=0;
 for(auto it=std::sregex_iterator(row.begin(),row.end(),stamp);it!=std::sregex_iterator();++it){auto s=*it;auto frac=s[3].str();if(frac.empty())frac="0";if(!lx)while(frac.size()<3)frac+='0';if(std::stoi(s[2])>=60)continue; times.push_back(std::stoll(s[1])*60000+std::stoll(s[2])*1000+std::stoll(frac)+offset);body=s.position()+s.length();}
 if(times.empty())continue;auto text=row.substr(body);if(!text.empty()&&text.back()=='\r')text.pop_back();
 std::vector<Word> words;std::string plain;
 for(auto it=std::sregex_iterator(text.begin(),text.end(),word);it!=std::sregex_iterator();++it){auto s=*it;auto next=it;++next;size_t start=s.position()+s.length();size_t end=next==std::sregex_iterator()?text.size():size_t(next->position());auto token=text.substr(start,end-start);words.push_back({std::stoll(s[1])+offset,std::stoll(s[2]),token});plain+=token;}
 for(auto t:times)out.push_back({t,words.empty()?text:plain,"",words});
 }
 std::stable_sort(out.begin(),out.end(),[](auto&a,auto&b){return a.start<b.start;});return out;
}
inline Timeline lyrics(const nlohmann::json& j){
 auto field=[&](const char* key){auto it=j.find(key);if(it==j.end()||it->is_null())return std::string{};if(!it->is_string())throw std::runtime_error("Invalid lyric field");auto s=it->get<std::string>();if(s.size()>512*1024)throw std::runtime_error("Lyrics too large");return s;};
 auto out=parse(field("lxlyric"),true);if(out.empty())out=parse(field("lyric"));auto tr=parse(field("tlyric"));
 for(auto& l:out){auto it=std::lower_bound(tr.begin(),tr.end(),l.start,[](auto&a,int64_t t){return a.start<t;});if(it!=tr.end()&&it->start==l.start)l.translation=it->text;}return out;
}
inline int indexAt(const Timeline& t,int64_t ms){auto i=std::upper_bound(t.begin(),t.end(),ms,[](int64_t n,const Line&l){return n<l.start;});return int(i-t.begin())-1;}
struct State { bool connected=false,playing=false; std::string key,title,artist,message="Waiting for BLF / LX"; int64_t position=0,duration=0,sampled=0;double rate=1; Timeline timeline;
 int64_t at(int64_t now)const {auto age=now-sampled;if(!connected||age<0||age>3000)return -1;return std::clamp(position+(playing?int64_t(std::min<int64_t>(age,1500)*rate):0),int64_t(0),duration);}
};
inline State status(const nlohmann::json& j,int64_t now){State s;s.connected=true;s.sampled=now;s.message="Connected: no active track";
 auto st=j.value("status",std::string{});s.title=j.value("name",std::string{});s.artist=j.value("singer",std::string{});
 if((st!="playing"&&st!="paused")||s.title.empty())return s;
 double duration=j.at("duration").get<double>(),progress=j.at("progress").get<double>();double rate=j.value("playbackRate",1.0);
 if(!std::isfinite(duration)||!std::isfinite(progress)||duration<=0||duration>86400||progress<0||progress>duration+.1||!std::isfinite(rate)||rate<=0)throw std::runtime_error("Invalid playback state");
 s.duration=int64_t(std::llround(duration*1000));s.position=int64_t(std::llround(progress*1000));s.rate=std::clamp(rate,.1,8.0);s.playing=st=="playing";s.key=s.title+'\x1f'+s.artist+'\x1f'+std::to_string(s.duration);s.message="No synchronized lyrics";return s;}
}
