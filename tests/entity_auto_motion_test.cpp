#include "entity_auto.hpp"
#include "lyric_motion.hpp"
#include "lyrics_settings.hpp"
#include "lyric_lifecycle.hpp"
#include "wall_scene.hpp"
#include <iostream>
#include <stdexcept>
using namespace dango;
void check(bool b){if(!b)throw std::runtime_error("Auto-follow / motion / fade isolation regression");}
inventory::Record actor(uint32_t h,const char* cls,Vec3 p){inventory::Record r;r.handle=h;r.className=cls;r.networkOrigin=p;r.networkAngles=Vec3{};r.dormant=false;r.parent=0xffffffffu;r.parentAttachment=0;r.health=100;r.lifeState=0;return r;}
int main(){
 Config c;check(c.entitySmooth&&!c.lyricMove&&!c.autoFollow.enabled&&c.autoFollow.rules.empty()&&!c.wallIndependentTiming);
 c.autoFollow.enabled=true;setAutoRule(c.autoFollow,"class:Tank",true);setAutoRule(c.autoFollow,"survivor:5",true);c.autoFollow.range=500;
 auto encoded=encodeConfig(c);auto round=decodeConfig(encoded);check(round.autoFollow.enabled&&autoRule(round.autoFollow,"class:Tank")&&round.autoFollow.range==500);
 auto off=decodeConfig(nlohmann::json::object());check(!off.autoFollow.enabled&&off.autoFollow.rules.empty()&&off.entitySmooth&&!off.lyricMove);
 auto types=autofollow::catalogue();check(types[0].label=="Tank"&&types[0].group==0);bool zoey=false,gnome=false;for(auto& t:types){zoey|=t.key=="survivor:5";gnome|=t.key=="class:CGnome";}check(zoey&&gnome);
 inventory::Frame f;f.epoch=7;f.tick=1000;f.records={actor(0x9001,"Tank",{200,0,0}),actor(0x9002,"Tank",{100,0,0})};CameraPose view;view.eye={};autofollow::Selector selector;
 check(selector.choose(f,c.autoFollow,view,7,1100).handle==0x9002);f.records[1].health=0;check(selector.choose(f,c.autoFollow,view,7,1100).handle==0x9001);f.records[0].dormant=true;check(!selector.choose(f,c.autoFollow,view,7,1100).enabled);
 f.records={actor(0xa003,"CTerrorPlayer",{80,0,0})};f.records[0].team=3;f.records[0].zombieClass=8;check(selector.choose(f,c.autoFollow,view,7,1100).handle==0xa003);
 f.records[0].team=2;f.records[0].character=5;check(selector.choose(f,c.autoFollow,view,7,1100).handle==0xa003);f.records[0].character=0;check(!selector.choose(f,c.autoFollow,view,7,1100).enabled);
 f.records={actor(0xb003,"Tank",{501,0,0})};check(!selector.choose(f,c.autoFollow,view,7,1100).enabled);f.records[0].networkOrigin=Vec3{100,0,0};check(!selector.choose(f,c.autoFollow,view,8,1100).enabled&&!selector.choose(f,c.autoFollow,view,7,1700).enabled);
 c.autoFollow.enabled=false;check(!selector.choose(f,c.autoFollow,view,7,1100).enabled);
 // Dynamic classes can be aimed at and added without a hard-coded whitelist.
 f.epoch=7;f.tick=2000;f.records={actor(0xc001,"CAddonProp",{100,0,0})};GameCameraFrame aim;aim.active=1;aim.epoch=7;aim.worldToClip={0,-.001f,0,0,0,0,.001f,0,0,0,0,.5f,0,0,0,1};view.forward={1,0,0};
 auto picked=autofollow::aimed(f,aim,view,2100);check(picked&&picked->handle==0xc001);c.autoFollow.enabled=true;setAutoRule(c.autoFollow,"class:CAddonProp",true);check(selector.choose(f,c.autoFollow,view,7,2100).handle==0xc001);autofollow::discover(types,f,c.autoFollow);check(std::any_of(types.begin(),types.end(),[](const auto& t){return t.key=="class:CAddonProp"&&t.group==3;}));
 setAutoRule(c.autoFollow,"class:CAddonProp",false);check(!selector.choose(f,c.autoFollow,view,7,2100).enabled);
 // Every easing curve reaches the destination; entity identity changes never fly across the map.
 for(int curve=0;curve<5;++curve){check(std::abs(movement::ease(0,curve))<.001f&&std::abs(movement::ease(1,curve)-1)<.001f);movement::Travel t;t.begin({0,0,0},{10,20,30},0);auto p=t.at(.35,.35f,curve);check(length(p-Vec3{10,20,30})<.001f);}
 check(movement::ease(.5f,0)>.5f&&movement::ease(.5f,2)<.5f&&movement::ease(.5f,1)==.5f);
 movement::Tracker tracker;check(tracker.select({0,0,0},7,1,0,true,.35f,0).x==0);check(tracker.select({100,0,0},7,1,.033,true,.35f,0).x==0);auto smoothed=tracker.select({100,0,0},7,1,.1,true,.35f,0);check(smoothed.x>0&&smoothed.x<100);check(tracker.select({500,0,0},7,2,.11,true,.35f,0).x==500);check(tracker.select({800,0,0},7,2,.12,false,.35f,0).x==800);
 // One configured world slot still fades out, entity mode replaces immediately.
 LyricLifecycle world,entity;world.update("s",0,"A",0,1000,400,false,1);world.update("s",1,"B",.1,1000,400,false,1);check(world.entries.size()==2&&world.entries[0].retired==.1);
 entity.update("s",0,"A",0,0,0,false,1,true);entity.update("s",1,"B",.1,0,0,false,1,true);check(entity.entries.size()==1&&world.entries.size()==2);
 world.retire(.2,1000,400);world.expire(.5);check(world.entries.size()==2);world.expire(2);check(world.entries.empty());
 // Route handoff retires the wall instead of destroying its sentence immediately.
 wallscene::Scene wall;wallscene::Options options;options.lingerMs=100;options.fadeMs=400;wallscene::Ticket ticket{1,1,1000,7,true};wall.observe(ticket,1000,7,options);wall::Panel panel;panel.epoch=7;wall.add(panel,true,ticket,1000,options);
 ticket.active=false;ticket.retire=true;ticket.tick=1100;wall.observe(ticket,1100,7,options);check(wall.items.size()==1&&wall.items[0].retired==1100);check(wallscene::alpha(wall.items[0],1400,options)>.4f&&wallscene::alpha(wall.items[0],1400,options)<.6f);wall.observe(ticket,1700,7,options);check(wall.items.empty());
 std::cout<<"Passed: persistent default-off type rules, nearest live target/roles/zombie classes/range/map/staleness, easing and identity-safe smoothing, independent world/entity fades and wall route retirement\n";
}
