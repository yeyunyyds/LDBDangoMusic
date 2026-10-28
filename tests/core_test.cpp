#include "core.hpp"
#include <iostream>
#include <stdexcept>
void check(bool b){if(!b)throw std::runtime_error("check failed");}
int main(){using namespace dango;
 auto t=parse("[offset:-100]\n[00:02.50]later\n[00:01.000][00:03.000]repeat\nmetadata\n");check(t.size()==3&&t[0].start==900&&t[1].start==2400);check(indexAt(t,899)==-1&&indexAt(t,900)==0&&indexAt(t,9999)==2);
 auto l=lyrics(nlohmann::json{{"lxlyric","[00:01.00]<1000,250>你<1250,250>好"},{"tlyric","[00:01.00]hello"}});check(l.size()==1&&l[0].text=="你好"&&l[0].words.size()==2&&l[0].translation=="hello");
 check(parse("[00:01.5]x",true)[0].start==1005);check(parse("[00:01.5]x")[0].start==1500);
 check(parse("[00:00]"+std::string(32768,'x')).size()==1);
 auto fallback=lyrics(nlohmann::json{{"lxlyric","invalid"},{"lyric","[00:00]fallback"}});check(fallback.size()==1);
 auto s=status(nlohmann::json{{"status","playing"},{"name","song"},{"singer","artist"},{"duration",20},{"progress",2},{"playbackRate",2}},100);check(s.at(600)==3000&&s.at(3200)==-1);s.playing=false;check(s.at(600)==2000);s.position=18000;s.playing=true;check(s.at(1600)==20000);
 bool rejected=false;try{status(nlohmann::json{{"status","playing"},{"name","song"},{"duration",20},{"progress",-1}},100);}catch(...){rejected=true;}check(rejected);
 std::cout<<"Passed: LRC ordering/offset/repeated tags, LX words/translation/fallback, clock pause/rate/staleness/clamp, invalid status\n";
}
