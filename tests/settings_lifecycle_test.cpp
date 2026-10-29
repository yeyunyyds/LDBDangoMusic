#include "lyrics_settings.hpp"
#include "render_lifecycle.hpp"
#include <iostream>
#include <stdexcept>
void check(bool value){if(!value)throw std::runtime_error("Settings/lifecycle regression");}
int main(int argc,char**){using namespace dango;
 if(argc>1){auto list=nlohmann::json::array();for(int i=0;i<6;++i)list.push_back(encodeConfig(applyPreset(Config{},i)));std::cout<<list.dump();return 0;}
 Config c;c.port=12345;c.offset=350;c.demo=false;c.worldLyrics=true;c.translation=true;
 for(int i=0;i<6;++i){auto preset=applyPreset(c,i);check(preset.port==12345&&preset.offset==350&&preset.translation&&preset.worldLyrics&&preset.presetId==i);auto copy=decodeConfig(encodeConfig(preset));check(copy.textColor==preset.textColor&&copy.port==12345&&copy.lingerMs==1000&&copy.reanchorMs==0);}
 auto defaults=decodeConfig(nlohmann::json::object());check(defaults.progressiveReveal&&defaults.reuseVisibleAnchor&&defaults.reuseDistanceFactor==3&&defaults.lineWidthScale==1&&!defaults.fallbackReveal);
 c.wallEnabled=true;c.wallAutomatic=false;c.wallLiveLyrics=false;c.wallFallback=false;c.wallSize=2;c.wallCapacity=8;c.wallLingerMs=8000;c.wallFadeMs=2000;c.wallRange=900;c.wallSpacing=220;
 auto wallCopy=decodeConfig(encodeConfig(c));check(wallCopy.wallEnabled&&!wallCopy.wallAutomatic&&!wallCopy.wallLiveLyrics&&!wallCopy.wallFallback&&wallCopy.wallSize==2&&wallCopy.wallCapacity==8&&wallCopy.wallLingerMs==8000&&wallCopy.wallFadeMs==2000&&wallCopy.wallRange==900&&wallCopy.wallSpacing==220);
 check(!defaults.wallEnabled&&defaults.wallAutomatic&&defaults.wallLiveLyrics&&defaults.wallFallback);
 auto boundedWall=decodeConfig({{"wallCapacity",100},{"wallRange",-1},{"wallSize",42},{"wallFadeMs",0}});check(boundedWall.wallCapacity==8&&boundedWall.wallRange==160&&boundedWall.wallSize==2&&boundedWall.wallFadeMs==200);
 c.reuseDistanceFactor=4.5f;check(decodeConfig(encodeConfig(c)).reuseDistanceFactor==4.5f&&applyPreset(c,3).reuseDistanceFactor==4.5f);check(decodeConfig({{"reuseDistanceFactor",-100}}).reuseDistanceFactor==1&&decodeConfig({{"reuseDistanceFactor",100}}).reuseDistanceFactor==10);
 c.progressiveReveal=false;c.reuseVisibleAnchor=false;c.lineWidthScale=3;c.fallbackReveal=true;auto restored=decodeConfig(encodeConfig(c));check(!restored.progressiveReveal&&!restored.reuseVisibleAnchor&&restored.lineWidthScale==3&&restored.fallbackReveal);
 check(decodeConfig({{"lineWidthScale",100}}).lineWidthScale==4);
 bool invalid=false;try{parseColor("#BADHEX");}catch(...){invalid=true;}check(invalid);check(colorText(parseColor("#12ABef"))=="#12ABEF");
 check(!needsReanchor(2.999,0,3000)&&needsReanchor(3,0,3000)&&!needsReanchor(4,0,0));

 check(adjustReanchor(0,3)==100&&adjustReanchor(100,2)==0&&adjustReanchor(299900,5)==300000);
 check(reanchorSeconds(0)=="0"&&reanchorSeconds(100)=="0.1"&&reanchorSeconds(1875)=="1.875"&&reanchorSeconds(300000)=="300");
 c.reanchorMs=123400;auto custom=decodeConfig(encodeConfig(c));check(custom.reanchorMs==123400&&decodeConfig({{"reanchorMs",999999}}).reanchorMs==300000);
 auto hold=exitFrame(999,1000,400);check(hold.alpha==1&&!hold.done);auto fade=exitFrame(1200,1000,400);check(std::abs(fade.alpha-.5f)<.001&&!fade.done);check(exitFrame(1400,1000,400).done);
 std::cout<<"Passed: six presets preserve connection/sync, config roundtrip, RGB validation, one-second linger\n";
}
