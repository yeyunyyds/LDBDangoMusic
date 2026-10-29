#include "player_session.hpp"
#include "lyric_lifecycle.hpp"
#include <iostream>
#include <stdexcept>
using namespace dango;
void check(bool ok){if(!ok)throw std::runtime_error("Player restart/pause regression");}
State track(int64_t tick,int64_t position=2000,bool playing=true,std::string name="song"){
 return status({{"status",playing?"playing":"paused"},{"name",name},{"singer","artist"},{"duration",40},{"progress",position/1000.}},tick);
}
int main(){
 PlayerSession session;State published;int fetches=0;int64_t tick=1000;std::string content="old";State input=track(tick);
 auto readStatus=[&](){return input;};auto readLyrics=[&](){++fetches;return parse("[00:00]"+content);};auto clock=[&](){return tick;};auto emit=[&](State s){published=std::move(s);};
 session.poll(readStatus,readLyrics,clock,emit);check(fetches==1&&published.timeline[0].text=="old");auto initialGeneration=published.generation;
 PlaybackDisplay display;LyricLifecycle instances;
 auto render=[&](){auto f=display.select(published,tick);if(f.reset)instances.reset();if(f.lyrics){int at=indexAt(published.timeline,f.position);instances.update(published.key,at,at>=0?published.timeline[at].text:"",tick/1000.,1000,400);}return f;};
 check(render().lyrics&&instances.entries.size()==1);auto firstId=instances.entries[0].id;
 tick=1150;input=track(tick,2150,false);session.poll(readStatus,readLyrics,clock,emit);check(!render().lyrics&&instances.entries.empty()&&fetches==1&&session.timeline.empty());
 // Paused frames cannot reanchor an old active line indefinitely.
 tick=10000;input=track(tick,2150,false);session.poll(readStatus,readLyrics,clock,emit);check(!render().lyrics&&instances.entries.empty());
 tick=10150;content="resumed";input=track(tick,2150);session.poll(readStatus,readLyrics,clock,emit);check(fetches==2&&published.generation>initialGeneration&&render().lyrics&&instances.entries[0].text=="resumed"&&instances.entries[0].id!=firstId);
 // A renderer can miss pause/resume snapshots. Generation still forces a reset.
 auto before=published.generation;tick+=150;input=track(tick,2300,false);session.poll(readStatus,readLyrics,clock,emit);tick+=150;input=track(tick,2300);session.poll(readStatus,readLyrics,clock,emit);auto frame=render();check(frame.reset&&frame.lyrics&&published.generation>before&&fetches==3);
 // Closing the app invalidates key, cache and the active/outgoing instances now.
 published=session.disconnect(tick);check(!render().metadata&&instances.entries.empty()&&session.key.empty()&&session.timeline.empty());
 before=published.generation;tick+=150;content="same song after app restart";input=track(tick,0);session.poll(readStatus,readLyrics,clock,emit);check(fetches==4&&published.generation>before&&render().lyrics&&instances.entries[0].text==content);
 // Real seeks invalidate the cache; normal progress and sampling jitter do not.
 tick+=150;input=track(tick,150);session.poll(readStatus,readLyrics,clock,emit);check(fetches==4);tick+=150;input=track(tick,200);session.poll(readStatus,readLyrics,clock,emit);check(fetches==4);
 tick+=150;input=track(tick,20000);content="after seek";session.poll(readStatus,readLyrics,clock,emit);check(fetches==5&&render().reset&&instances.entries[0].text==content);
 // Song end / stopped status / stale samples hide text and stop its return cycle.
 tick+=150;input=track(tick,40000);session.poll(readStatus,readLyrics,clock,emit);check(!render().lyrics&&instances.entries.empty());
 tick+=150;input=track(tick,1000);session.poll(readStatus,readLyrics,clock,emit);check(render().lyrics);tick+=3100;check(!render().metadata&&instances.entries.empty());
 input=track(tick,4100);session.poll(readStatus,readLyrics,clock,emit);check(render().reset&&render().lyrics);
 input=status({{"status","stopped"}},tick);session.poll(readStatus,readLyrics,clock,emit);check(!render().metadata&&session.timeline.empty());
 // Failed lyric fetches stay online and retry quickly without old cached text.
 tick+=150;input=track(tick,1000);int failures=0;auto failingLyrics=[&]()->Timeline{++failures;throw std::runtime_error("API lyric not ready");};session.poll(readStatus,failingLyrics,clock,emit);check(published.connected&&published.timeline.empty()&&failures==1&&!render().lyrics);
 tick+=500;input=track(tick,1500);session.poll(readStatus,failingLyrics,clock,emit);check(failures==1);tick+=500;input=track(tick,2000);session.poll(readStatus,readLyrics,clock,emit);check(!published.timeline.empty()&&render().lyrics);
 // A track switch during lyric fetch must not attach the previous response.
 PlayerSession switching;int reads=0;auto switchStatus=[&](){return track(tick,2000,true,++reads==1?"first":"second");};switching.poll(switchStatus,readLyrics,clock,emit);check(published.title=="second"&&published.timeline.empty());
 switching.poll([&](){return track(tick,2000,true,"second");},readLyrics,clock,emit);check(published.title=="second"&&!published.timeline.empty());
 // Verification loses connection: propagate to the caller, which publishes disconnect.
 PlayerSession lost;reads=0;bool threw=false;try{lost.poll([&](){if(++reads>1)throw std::runtime_error("connection closed");return track(tick);},readLyrics,clock,emit);}catch(...){threw=true;published=lost.disconnect(tick);}check(threw&&!published.connected&&lost.timeline.empty());
 // Natural next-track transition: metadata switches while the lyric API still
 // returns A. Do not cache A permanently under B; recover without pause/resume.
 PlayerSession autoplay;State autoOut;int64_t autoTick=1000;State autoStatus=track(autoTick,39000,true,"A");Timeline autoLyrics=parse("[00:00]A words");int autoFetches=0;
 auto pollAuto=[&](){autoplay.poll([&](){return autoStatus;},[&](){++autoFetches;return autoLyrics;},[&](){return autoTick;},[&](State s){autoOut=std::move(s);});};
 pollAuto();check(autoOut.timeline[0].text=="A words");
 autoTick=2000;autoStatus=track(autoTick,40000,true,"A");pollAuto();check(autoFetches==1); // don't reload at end
 autoTick=2150;autoStatus=track(autoTick,0,true,"B");pollAuto();check(autoOut.title=="B"&&autoOut.timeline.empty());
 autoTick=3150;autoStatus=track(autoTick,1000,true,"B");autoLyrics=parse("[00:00]B words");pollAuto();check(autoOut.timeline[0].text=="B words"&&autoOut.playing);
 auto stableGeneration=autoOut.generation;
 autoTick=3650;autoStatus=track(autoTick,1500,true,"B");pollAuto();check(autoOut.generation==stableGeneration); // verification doesn't reset animations
 // A nonempty loading placeholder is corrected by warm-up refresh.
 autoTick=4000;autoStatus=track(autoTick,0,true,"C");autoLyrics=parse("[00:00]Loading lyrics");pollAuto();check(autoOut.timeline[0].text=="Loading lyrics");
 stableGeneration=autoOut.generation;autoTick=4500;autoStatus=track(autoTick,500,true,"C");autoLyrics=parse("[00:00]C real lyrics");pollAuto();check(autoOut.timeline[0].text=="C real lyrics"&&autoOut.generation>stableGeneration);
 // Lyrics that become ready late remain repairable by periodic refresh.
 autoplay.refreshStep=6;autoplay.scheduleRefresh(autoTick);check(autoplay.refreshAt==autoTick+30000);
 autoTick+=30000;autoStatus=track(autoTick,30500,true,"C");autoLyrics=parse("[00:00]C final lyrics");pollAuto();check(autoOut.timeline[0].text=="C final lyrics");
 std::cout<<"Passed: pause clears/reanchor suppression, same-song resume/restart reload, missed transitions, seeks, end/staleness, retry and in-flight track/disconnect verification\n";
}
