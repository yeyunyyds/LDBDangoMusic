#include "media_protocol.hpp"
#include "lyrics_settings.hpp"
#include "player_session.hpp"
#include <cassert>
#include <iostream>
using namespace dango;
int main(){
 nlohmann::json j={{"schema",1},{"parent",123},{"tick",1000},{"status","playing"},{"name","Song"},{"singer","Artist"},{"duration",120},{"progress",2},{"sourceId","cloudmusic"},{"lines",nlohmann::json::array({{{"start",1000},{"text","你好"},{"translation","Hello"},{"words",nlohmann::json::array({{{"start",1000},{"duration",500},{"text","你"}},{{"start",1500},{"duration",500},{"text","好"}}})}}})}};
 auto s=media::readStatus(j,1100,123);assert(s.connected&&s.playing&&s.at(1100)==2100);assert(s.key.find("cloudmusic")!=std::string::npos);
 auto lines=media::readLyrics(j);assert(lines.size()==1&&lines[0].translation=="Hello"&&lines[0].words.size()==2&&lines[0].words[1].start==1500);
 auto reject=[&](auto fn){bool rejected=false;try{fn();}catch(...){rejected=true;}assert(rejected);};
 reject([&]{media::readStatus(j,4001,123);});reject([&]{media::readStatus(j,999,123);});reject([&]{media::readStatus(j,1100,124);});
 auto bad=j;bad["lines"][0]["words"][0]["duration"]=-1;reject([&]{media::readLyrics(bad);});
 auto missing=j;missing["status"]="stopped";missing["name"]="";missing["message"]="No Windows media sessions (SMTC)";PlayerSession diagnosticSession;assert(diagnosticSession.observe(media::readStatus(missing,1100,123)).message=="No Windows media sessions (SMTC)");
 auto changed=j;changed["sourceId"]="qqmusic";assert(media::readStatus(changed,1100,123).key!=s.key);
 PlayerSession session;session.observe(s);session.timeline=lines;auto paused=s;paused.playing=false;assert(session.observe(paused).timeline.empty());assert(session.disconnect(1200).timeline.empty());
 Config c;assert(c.musicSource==0);c.musicSource=3;assert(decodeConfig(encodeConfig(c)).musicSource==3);assert(decodeConfig(nlohmann::json::object()).musicSource==0);
 std::cout<<"Passed: SMTC snapshots, identity/staleness rejection, words/translation, source/pause/disconnect isolation and source settings\n";
}
