#include "lyrics_settings.hpp"
#include "lyric_lifecycle.hpp"
#include <iostream>
#include <stdexcept>
#include <limits>
using namespace dango;
void check(bool ok){if(!ok)throw std::runtime_error("Component layout regression");}
bool near(float a,float b){return std::abs(a-b)<.001f;}
int main(){
 Config c;check(c.layout.lyricCount==2);check(near(c.layout.lyrics[0].y,.44f)&&near(c.layout.lyrics[1].y,.44f));check(c.layout.title.y<.2f&&c.layout.title.distance>c.layout.lyrics[0].distance);
 // Old configs gain the new layout while preserving sync and a customized distance.
 auto old=decodeConfig({{"offsetMs",275},{"worldDistance",320},{"verticalPosition",.61f}});check(old.offset==275&&near(old.layout.lyrics[0].distance,320)&&near(old.layout.lyrics[0].y,.44f));
 c.layout.lyricCount=9;for(int i=0;i<11;++i){auto& p=component(c.layout,i);p.x=.1f+i*.07f;p.y=.2f+i*.05f;p.distance=100+i*60;p.size=.5f+i*.1f;}
 auto saved=encodeConfig(c);auto copy=decodeConfig(saved);check(layoutJson(copy.layout)==layoutJson(c.layout));auto preset=applyPreset(c,3);check(layoutJson(preset.layout)==layoutJson(c.layout));
 for(int i=0;i<11;++i){adjustPosition(copy.layout,i,0,2);check(near(component(copy.layout,i).x,component(c.layout,i).x+.01f));adjustPosition(copy.layout,i,0,4);check(near(component(copy.layout,i).x,component(ComponentLayout{},i).x));}
 auto clamped=readLayout({{"lyricCount",100},{"lyrics",nlohmann::json::array({{{"x",-5},{"y",10},{"distance",0},{"size",10}}})}});check(clamped.lyricCount==9&&clamped.lyrics[0].x==.02f&&clamped.lyrics[0].y==.98f&&clamped.lyrics[0].distance==32&&clamped.lyrics[0].size==3);
 bool invalid=false;try{readLayout({{"lyrics","bad"}});}catch(...){invalid=true;}check(invalid);invalid=false;try{readPosition({{"size",std::numeric_limits<double>::infinity()}},{});}catch(...){invalid=true;}check(invalid);
 // Group adjustments keep title/artist untouched, include disabled slots,
 // and preserve differences between individual lyric positions.
 auto grouped=c.layout;auto titleBefore=positionJson(grouped.title);float gap=grouped.lyrics[1].y-grouped.lyrics[0].y;
 adjustLyricGroup(grouped,1,1);check(near(grouped.lyrics[8].y,c.layout.lyrics[8].y-.01f)&&near(grouped.lyrics[1].y-grouped.lyrics[0].y,gap)&&positionJson(grouped.title)==titleBefore);
 adjustLyricGroup(grouped,2,3);check(near(grouped.lyrics[8].distance,c.layout.lyrics[8].distance+50));adjustLyricGroup(grouped,1,4);check(near(grouped.lyrics[0].y,.44f));
 // Upgrade the old defaults once, preserving custom positions and explicit new settings.
 auto legacy=layoutJson(ComponentLayout{});legacy["lyrics"][0]["y"]=.56;legacy["lyrics"][1]["y"]=.37;
 auto migrated=decodeConfig({{"layout",legacy},{"reanchorMs",3000}});check(near(migrated.layout.lyrics[0].y,.44f)&&near(migrated.layout.lyrics[1].y,.37f)&&migrated.reanchorMs==0);
 auto explicitNew=decodeConfig({{"defaultsVersion",2},{"layout",legacy},{"reanchorMs",3000}});check(near(explicitNew.layout.lyrics[0].y,.56f)&&explicitNew.reanchorMs==3000);
 // Screen fractions round-trip through perspective for every default component.
 GameCameraFrame f;f.active=1;f.width=1920;f.height=1080;f.tick=1000;f.worldToView={1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};f.worldToClip={1,0,0,0,0,1,0,0,0,0,1.0001f,-1.0001f,0,0,1,0};CameraPose camera;check(cameraPose(f,camera));
 ComponentLayout defaults;for(int i=0;i<11;++i){const auto& p=component(defaults,i);Vec3 anchor;Placement placed;check(componentAnchor(f,camera,p,anchor)&&worldComponent(f,anchor,p,1920,1080,8,32,placed));check(near(placed.x,p.x*1920)&&near(placed.y,p.y*1080));}
 auto p=defaults.lyrics[0];Vec3 anchor;Placement close,far;check(componentAnchor(f,camera,p,anchor)&&worldComponent(f,anchor,p,1920,1080,8,32,close));p.distance*=2;check(componentAnchor(f,camera,p,anchor)&&worldComponent(f,anchor,p,1920,1080,8,32,far));check(near(far.scale,close.scale*.5f)&&near(far.x,close.x)&&near(far.y,close.y));p.size*=2;check(worldComponent(f,anchor,p,1920,1080,8,32,far)&&near(far.scale,close.scale));
 // Slot rotation supports 1..9 independent positions, with bounded outgoing lines.
 for(int count=1;count<=9;++count){LyricLifecycle model;for(int line=0;line<30;++line){model.update("track",line,std::to_string(line),line*.01,1000,400,false,count);check(model.entries.back().slot==line%count&&model.entries.size()<=size_t(count));for(auto& e:model.entries)for(auto& other:model.entries)check(e.id==other.id||e.slot!=other.slot);}model.update("track",29,"29",10,1000,400,false,count);check(model.entries.size()==1&&model.entries[0].index==29);}
 std::cout<<"Passed: 11 component positions, legacy migration/roundtrip/preset preservation, bounds, perspective placement, distance/size, 1..9 slot rotation and expiry\n";
}
