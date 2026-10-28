#pragma once
#include "json.hpp"
#include "component_layout.hpp"
#include <string>
#include <stdexcept>
#include <algorithm>
#include <cstdint>
#include <cmath>
namespace dango {
struct Config {
 int port=23330,size=32;bool enabled=true,worldLyrics=false,worldTestAnchor=false;
 float worldDistance=200,worldSide=75,worldHeight=8;bool cameraDiagnostics=false,demo=true,translation=false,alternate=true;
 ComponentLayout layout;
 float y=.56f;int offset=500;bool outline=false,glow=true,pop=true,wiggle=true,keepAway=true;
 float wiggleFrequency=8,wiggleAmount=.8f,spread=30;bool debugOverlay=false;float glowRadius=.5f,glowStrength=.3f,opacity=1.f,width=.32f;
 int transition=400,lingerMs=1000,reanchorMs=3000;float animationSpeed=1;bool showTitle=false,showArtist=false;int presetId=0;
 std::string mode="peak";uint32_t textColor=0xfff7e1d7,accent=0xfff7e1d7;
};
inline uint32_t parseColor(const std::string& text){if(text.size()!=7||text[0]!='#'||text.find_first_not_of("0123456789abcdefABCDEF",1)!=std::string::npos)throw std::runtime_error("Color must be #RRGGBB");return uint32_t(std::stoul(text.substr(1),nullptr,16));}
inline std::string colorText(uint32_t color){const char* digits="0123456789ABCDEF";std::string out="#000000";for(int i=0;i<6;++i)out[6-i]=digits[(color>>(i*4))&15];return out;}
inline Config decodeConfig(const nlohmann::json& j){
 if(!j.is_object())throw std::runtime_error("Config must be a JSON object");Config c;
#define BOOL_FIELD(name,field,defaultValue) c.field=j.value(name,defaultValue)
 BOOL_FIELD("enabled",enabled,true);BOOL_FIELD("worldLyrics",worldLyrics,false);BOOL_FIELD("worldTestAnchor",worldTestAnchor,false);BOOL_FIELD("cameraDiagnostics",cameraDiagnostics,false);BOOL_FIELD("demo",demo,true);BOOL_FIELD("translation",translation,false);BOOL_FIELD("alternate",alternate,true);BOOL_FIELD("outline",outline,false);BOOL_FIELD("glow",glow,true);BOOL_FIELD("pop",pop,true);BOOL_FIELD("wiggle",wiggle,true);BOOL_FIELD("keepAway",keepAway,true);BOOL_FIELD("debugOverlay",debugOverlay,false);BOOL_FIELD("showTitle",showTitle,false);BOOL_FIELD("showArtist",showArtist,false);
#undef BOOL_FIELD
 auto integer=[&](const char* key,int value,int lo,int hi){return std::clamp(j.value(key,value),lo,hi);};
 auto number=[&](const char* key,float value,float lo,float hi){float n=j.value(key,value);if(!std::isfinite(n))throw std::runtime_error("Non-finite setting");return std::clamp(n,lo,hi);};
 c.port=integer("port",23330,1,65535);c.size=integer("fontSize",32,12,96);c.offset=integer("offsetMs",500,-10000,10000);c.transition=integer("transitionMs",400,100,3000);c.lingerMs=integer("lingerMs",1000,0,5000);c.reanchorMs=integer("reanchorMs",3000,0,30000);c.presetId=integer("presetId",0,-1,5);
 c.worldDistance=number("worldDistance",200,32,1000);c.worldSide=number("worldSide",75,0,300);c.worldHeight=number("worldHeight",8,1,64);c.y=number("verticalPosition",.56f,.1f,.85f);c.width=number("maxWidth",.32f,.2f,.9f);c.opacity=number("opacity",1,.1f,1);c.glowStrength=number("glowStrength",.3f,0,3);c.glowRadius=number("glowRadius",.5f,0,1);c.wiggleFrequency=number("wiggleFrequency",8,4,24);c.wiggleAmount=number("wiggleAmount",.8f,0,2);c.spread=number("spreadDegrees",30,0,60);c.animationSpeed=number("animationSpeed",1,.25f,3);
 for(auto& p:c.layout.lyrics)p.distance=c.worldDistance;if(j.contains("layout"))c.layout=readLayout(j.at("layout"),c.layout);
 c.textColor=parseColor(j.value("textColor",std::string("#F7E1D7")));c.accent=parseColor(j.value("accentColor",std::string("#F7E1D7")));return c;
}
inline nlohmann::json encodeConfig(const Config& c){return {
 {"layout",layoutJson(c.layout)},{"enabled",c.enabled},{"port",c.port},{"fontSize",c.size},{"worldLyrics",c.worldLyrics},{"worldTestAnchor",c.worldTestAnchor},{"worldDistance",c.worldDistance},{"worldSide",c.worldSide},{"worldHeight",c.worldHeight},{"cameraDiagnostics",c.cameraDiagnostics},{"demo",c.demo},{"translation",c.translation},{"alternate",c.alternate},{"verticalPosition",c.y},{"offsetMs",c.offset},{"outline",c.outline},{"glow",c.glow},{"glowStrength",c.glowStrength},{"glowRadius",c.glowRadius},{"opacity",c.opacity},{"maxWidth",c.width},{"transitionMs",c.transition},{"lingerMs",c.lingerMs},{"reanchorMs",c.reanchorMs},{"animationSpeed",c.animationSpeed},{"pop",c.pop},{"wiggle",c.wiggle},{"wiggleFrequency",c.wiggleFrequency},{"wiggleAmount",c.wiggleAmount},{"spreadDegrees",c.spread},{"keepAway",c.keepAway},{"debugOverlay",c.debugOverlay},{"textColor",colorText(c.textColor)},{"accentColor",colorText(c.accent)},{"showTitle",c.showTitle},{"showArtist",c.showArtist},{"presetId",c.presetId},{"displayMode","peak"}};}
inline const char* presetName(int id){static const char* names[]={u8"暖白浮光",u8"轻柔舒缓",u8"霓虹粉",u8"冰蓝",u8"琥珀金",u8"简洁纯净"};return id>=0&&id<6?names[id]:u8"自定义";}
inline Config applyPreset(Config c,int id){if(id<0||id>5)return c;c.presetId=id;c.pop=true;c.wiggle=true;c.outline=false;c.glow=true;c.glowStrength=.3f;c.glowRadius=.5f;c.wiggleFrequency=8;c.wiggleAmount=.8f;c.animationSpeed=1;c.transition=400;c.lingerMs=1000;c.textColor=c.accent=0xf7e1d7;
 switch(id){case 1:c.textColor=0xf6f1eb;c.accent=0xe3cedf;c.pop=false;c.wiggle=false;c.glowStrength=.2f;c.animationSpeed=.65f;c.transition=700;break;
 case 2:c.textColor=0xffd6ea;c.accent=0xff4d9f;c.glowStrength=.65f;c.glowRadius=.8f;c.wiggleFrequency=12;c.wiggleAmount=.55f;break;
 case 3:c.textColor=0xe5f7ff;c.accent=0x5ac8fa;c.glowStrength=.4f;c.wiggleFrequency=6;c.wiggleAmount=.25f;break;
 case 4:c.textColor=0xffebcd;c.accent=0xffb347;c.glowStrength=.35f;c.wiggleFrequency=6;c.wiggleAmount=.3f;break;
 case 5:c.textColor=c.accent=0xf6f6f6;c.glow=false;c.pop=false;c.wiggle=false;c.transition=500;break;}return c;
}
}
