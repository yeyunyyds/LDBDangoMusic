#include "netease_elog.hpp"
#include <cassert>
#include <iostream>
using namespace dango;
std::string encoded(const std::string& input){unsigned char inverse[256]{};for(int i=0;i<256;++i)inverse[netease::decodeByte(static_cast<unsigned char>(i))]=static_cast<unsigned char>(i);std::string out;for(unsigned char c:input)out+=char(inverse[c]);return out;}
std::string line(int pid,int tick,const std::string& body){return "["+std::to_string(pid)+":7:1104/123456:"+std::to_string(tick)+":INFO:play.cpp(1)] "+body+"\n";}
int main(){
 netease::Parser p;auto add=[&](int tick,const std::string& body,int pid=42){auto bytes=encoded(line(pid,tick,body));p.feed(bytes.data(),bytes.size(),1000000,0);};
 auto track=R"({"id":123,"name":"测试歌","duration":100000,"artists":[{"name":"歌手"}],"musicurl":"not-retained"})";
 auto event=encoded(line(42,1000,std::string("\"playOneTrackInPlayingList\", ")+track));p.feed(event.data(),event.size()-2,1000000,0);assert(p.song.id.empty());p.feed(event.data()+event.size()-2,2,1000000,0);assert(p.song.id=="123"&&p.song.title==u8"测试歌"&&p.state(3000).position==2000);
 add(1200,"\"native播放资源load完成\", {\"songId\":123}");assert(p.state(3200).position==2000);
 add(4000,"\"native播放state\", 2");assert(!p.playing&&p.at(8000)==2800);
 add(5000,"\"setPlayingPosition\", 20");assert(p.at(8000)==20000);
 add(6000,"\"native播放state\", 1");assert(p.at(8000)==22000);
 add(7000,"Process 99 exited.",99);assert(!p.song.id.empty());
 add(7100,"\"setPlayingPosition\", 80",99);assert(p.at(8000)==22000);
 add(9000,"Process 42 exited.",99);assert(p.song.id.empty());
 add(10000,std::string("\"checkPlayPrivilege\", ")+track);assert(!p.playing);
 add(10500,std::string("\"playOneTrackInPlayingList\", ")+track);assert(p.playing&&p.at(11000)==500);
 add(11000,"\"native播放state\", 1");assert(p.at(12000)==1500);
 add(12000,"\"exitApp\"");assert(p.song.id.empty());
 add(14000,"\"native播放资源load完成\", {\"songId\":456}");assert(p.song.id=="456"&&p.song.title.empty());
 auto junk=encoded(std::string(1024*1024+1,'a')+line(42,15000,std::string("\"playOneTrackInPlayingList\", ")+track));p.feed(junk.data(),junk.size(),1000000,0);assert(p.song.id=="456");
 netease::Parser fragmented;auto encodedTrack=encoded(line(42,16000,std::string("\"playOneTrackInPlayingList\", ")+track));for(char c:encodedTrack)fragmented.feed(&c,1,1000000,0);assert(fragmented.song.title==u8"测试歌");
 std::tm wallTm{};wallTm.tm_year=126;wallTm.tm_mon=10;wallTm.tm_mday=4;wallTm.tm_hour=12;wallTm.tm_isdst=-1;auto wall=int64_t(std::mktime(&wallTm))*1000;netease::Parser fallbackClock;auto dated=encoded("[42:7:1104/120000:9900000:INFO:play.cpp(1)] [2026-11-04 12:00:00] "+std::string("\"playOneTrackInPlayingList\", ")+track+"\n");fallbackClock.feed(dated.data(),dated.size(),1000000,wall+1000);assert(fallbackClock.eventTick==999000&&fallbackClock.at(1000000)==1000);
 auto response=nlohmann::json{{"lrc",{{"lyric","[00:01.00]Hello world\n[00:05.00]Second"}}},{"tlyric",{{"lyric","[00:01.10]你好 世界"}}},{"yrc",{{"lyric","[1000,1000](1000,500,0)Hello (1500,500,0)world"}}}};
 auto lines=netease::lyricResponse(response);assert(lines.size()==2&&lines[0].translation==u8"你好 世界"&&lines[0].words.size()==2&&lines[0].words[1].start==1500);
 std::cout<<"Passed: native encoded elog streaming/UTF-8, preload, load anchor, pause/resume/seek, process isolation/exit, metadata-only load, oversize rejection and ID lyric/YRC/translation parsing\n";
}
