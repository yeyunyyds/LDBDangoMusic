#include "lyrics_settings.hpp"
#include "render_lifecycle.hpp"
#include <iostream>
#include <stdexcept>
void check(bool value){if(!value)throw std::runtime_error("Settings/lifecycle regression");}
int main(int argc,char**){using namespace dango;
 if(argc>1){auto list=nlohmann::json::array();for(int i=0;i<6;++i)list.push_back(encodeConfig(applyPreset(Config{},i)));std::cout<<list.dump();return 0;}
 Config c;c.port=12345;c.offset=350;c.demo=false;c.worldLyrics=true;c.translation=true;
 for(int i=0;i<6;++i){auto preset=applyPreset(c,i);check(preset.port==12345&&preset.offset==350&&preset.translation&&preset.worldLyrics&&preset.presetId==i);auto copy=decodeConfig(encodeConfig(preset));check(copy.textColor==preset.textColor&&copy.port==12345&&copy.lingerMs==1000&&copy.reanchorMs==3000);}
 bool invalid=false;try{parseColor("#BADHEX");}catch(...){invalid=true;}check(invalid);check(colorText(parseColor("#12ABef"))=="#12ABEF");
 check(!needsReanchor(2.999,0,3000)&&needsReanchor(3,0,3000)&&!needsReanchor(4,0,0));
 FrameGate gate;check(gate.allow(100,1));check(!gate.allow(100,1));check(gate.allow(200,1));check(gate.allow(100,2));check(!gate.allow(100,2));check(gate.painted.load()==3&&gate.duplicates.load()==2);gate.reset();check(gate.allow(100,2));
 auto hold=exitFrame(999,1000,400);check(hold.alpha==1&&!hold.done);auto fade=exitFrame(1200,1000,400);check(std::abs(fade.alpha-.5f)<.001&&!fade.done);check(exitFrame(1400,1000,400).done);
 std::cout<<"Passed: six presets preserve connection/sync, config roundtrip, RGB validation, per-buffer per-scene duplicate suppression, one-second linger\n";
}
