#include "lyric_lifecycle.hpp"
#include <iostream>
#include <stdexcept>
#include <vector>
using namespace dango;
void check(bool value){if(!value)throw std::runtime_error("Lyric lifetime/overlay regression");}
int main(){
 PlaybackCursor cursor;check(cursor.select("song",1000,true).position==1000);check(cursor.select("song",950,true).position==1000);check(cursor.select("song",1250,true).position==1250);check(cursor.select("song",500,true).seek);check(cursor.select("next",0,true).position==0);
 LyricLifecycle model;model.update("s",0,"old",0,1000,400);auto first=model.entries.back().id;model.update("s",1,"current",1,1000,400);check(model.entries.size()==2&&model.entries.front().expires==2.4);
 model.update("s",1,"current",2,1000,400);check(model.entries.front().expires==2.4);model.update("s",1,"current",2.5,1000,400);check(model.entries.size()==1&&model.entries.front().text=="current");
 model.update("s",0,"old",3,1000,400,true);check(model.entries.size()==1&&model.entries[0].id!=first);
 model.update("s",1,"old",4,1000,400);check(model.entries.size()==1&&model.entries[0].index==1);
 model.update("s",2,"third",5,1000,400);model.update("s",3,"fourth",5.1,1000,400);check(model.entries.size()==2);model.update("new",0,"new song",6,1000,400);check(model.entries.size()==1&&model.entries[0].text=="new song");model.update("new",-1,"",7,1000,400);check(model.entries.empty());
 // Entity attachment uses one slot even when ordinary lyrics retain several
 // outgoing lines. Rapid changes must replace the sentence in the same frame.
 LyricLifecycle attached;for(int i=0;i<12;++i){attached.update("entity",i,"line "+std::to_string(i),10+i*.01,0,0,false,1);check(attached.entries.size()==1&&attached.entries[0].index==i&&attached.entries[0].slot==0&&attached.entries[0].retired<0);}
 attached.update("entity",-1,"",11,0,0,false,1);check(attached.entries.empty());
 // A software backbuffer reproduces the old failure: scene generation can stay
 // constant while the next song line changes. Every Present must be fresh.
 std::vector<int> back{7},saved,presented;int draws=0;
 auto capture=[&](){saved=back;return true;};auto restore=[&](){back=saved;};auto present=[&](){presented=back;return 42;};
 check(overlayTransaction(capture,[&](){back.push_back(10);++draws;},present,restore)==42);check((presented==std::vector<int>{7,10})&&(back==std::vector<int>{7}));
 overlayTransaction(capture,[&](){back.push_back(20);++draws;},present,restore);check((presented==std::vector<int>{7,20})&&back.size()==1&&draws==2);
 overlayTransaction(capture,[&](){back.push_back(20);},present,restore);check((presented==std::vector<int>{7,20}));
 check(overlayTransaction(capture,[&](){back.push_back(30);},[](){return -1;},restore)==-1&&back.size()==1);
 bool threw=false;try{overlayTransaction(capture,[](){throw std::runtime_error("draw failed");},present,restore);}catch(...){threw=true;}check(threw&&back.size()==1);
 overlayTransaction([](){return false;},[&](){++draws;},present,restore);check(draws==2&&presented.size()==1);
 std::cout<<"Passed: no retired-line resurrection, bounded expiry, jitter/seek selection, fresh repeated Presents, pristine-buffer restoration/failure cleanup\n";
}
