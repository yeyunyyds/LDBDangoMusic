#pragma once
#include "json.hpp"
#include "component_layout.hpp"
#include "hud_appearance.hpp"
#include "wall_layout.hpp"
#include <string>
#include <stdexcept>
#include <algorithm>
#include <cstdint>
#include <cmath>
namespace dango {
struct Config {
 int port=23330,size=32;bool enabled=true,worldLyrics=false,worldTestAnchor=false;
 float worldDistance=200,worldSide=75,worldHeight=8;bool cameraDiagnostics=false,demo=true,translation=false,alternate=true;
 WallTextLayout wallLayout;
 ComponentLayout layout;hud::Appearance hud;
 bool progressiveReveal=true,reuseVisibleAnchor=true,fallbackReveal=false,wallBackdrop=false,wallDebug=false;float lineWidthScale=1,reuseDistanceFactor=3;
 bool wallFloorFollow=false,wallReuse=false;int wallSurface=0;
 bool wallEnabled=false,wallAutomatic=true,wallLiveLyrics=true,wallFallback=true;int wallSize=1,wallCapacity=6,wallLingerMs=4000,wallFadeMs=1000;float wallRange=600,wallSpacing=140;
 float y=.44f;int offset=500;bool outline=false,glow=true,pop=true,wiggle=true,keepAway=true;
 float wiggleFrequency=8,wiggleAmount=.8f,spread=30;bool debugOverlay=false;float glowRadius=.5f,glowStrength=.3f,opacity=1.f,width=.32f;
 int transition=400,lingerMs=1000,reanchorMs=0;float animationSpeed=1;bool showTitle=false,showArtist=false;int presetId=0;
 std::string mode="peak";uint32_t textColor=0xfff7e1d7,accent=0xfff7e1d7;
};
inline uint32_t parseColor(const std::string& text){if(text.size()!=7||text[0]!='#'||text.find_first_not_of("0123456789abcdefABCDEF",1)!=std::string::npos)throw std::runtime_error("Color must be #RRGGBB");return uint32_t(std::stoul(text.substr(1),nullptr,16));}
inline std::string colorText(uint32_t color){const char* digits="0123456789ABCDEF";std::string out="#000000";for(int i=0;i<6;++i)out[6-i]=digits[(color>>(i*4))&15];return out;}
inline Config decodeConfig(const nlohmann::json& j){
 if(!j.is_object())throw std::runtime_error("Config must be a JSON object");Config c;
#define BOOL_FIELD(name,field,defaultValue) c.field=j.value(name,defaultValue)
 BOOL_FIELD("enabled",enabled,true);BOOL_FIELD("worldLyrics",worldLyrics,false);BOOL_FIELD("worldTestAnchor",worldTestAnchor,false);BOOL_FIELD("cameraDiagnostics",cameraDiagnostics,false);BOOL_FIELD("demo",demo,true);BOOL_FIELD("translation",translation,false);BOOL_FIELD("alternate",alternate,true);BOOL_FIELD("outline",outline,false);BOOL_FIELD("glow",glow,true);BOOL_FIELD("pop",pop,true);BOOL_FIELD("wiggle",wiggle,true);BOOL_FIELD("keepAway",keepAway,true);BOOL_FIELD("debugOverlay",debugOverlay,false);BOOL_FIELD("showTitle",showTitle,false);BOOL_FIELD("showArtist",showArtist,false);
 BOOL_FIELD("progressiveReveal",progressiveReveal,true);BOOL_FIELD("reuseVisibleAnchor",reuseVisibleAnchor,true);
 BOOL_FIELD("wallReuse",wallReuse,false);BOOL_FIELD("wallFloorFollow",wallFloorFollow,false);
 BOOL_FIELD("fallbackReveal",fallbackReveal,false);BOOL_FIELD("wallBackdrop",wallBackdrop,false);BOOL_FIELD("wallDebug",wallDebug,false);
 BOOL_FIELD("wallEnabled",wallEnabled,false);BOOL_FIELD("wallAutomatic",wallAutomatic,true);BOOL_FIELD("wallLiveLyrics",wallLiveLyrics,true);
 BOOL_FIELD("wallFallback",wallFallback,true);
#undef BOOL_FIELD
 auto integer=[&](const char* key,int value,int lo,int hi){return std::clamp(j.value(key,value),lo,hi);};
 auto number=[&](const char* key,float value,float lo,float hi){float n=j.value(key,value);if(!std::isfinite(n))throw std::runtime_error("Non-finite setting");return std::clamp(n,lo,hi);};
 c.wallSurface=integer("wallSurface",0,0,1);if(j.contains("wallLayout"))c.wallLayout=readWallLayout(j.at("wallLayout"));
 c.wallSize=integer("wallSize",1,0,2);c.wallCapacity=integer("wallCapacity",6,2,8);c.wallLingerMs=integer("wallLingerMs",4000,0,15000);c.wallFadeMs=integer("wallFadeMs",1000,200,3000);c.wallRange=number("wallRange",600,160,1200);c.wallSpacing=number("wallSpacing",140,40,400);
 c.reuseDistanceFactor=number("reuseDistanceFactor",3,1,10);
 c.lineWidthScale=number("lineWidthScale",1,.5f,4);
 c.port=integer("port",23330,1,65535);c.size=integer("fontSize",32,12,96);c.offset=integer("offsetMs",500,-10000,10000);c.transition=integer("transitionMs",400,100,3000);c.lingerMs=integer("lingerMs",1000,0,5000);c.reanchorMs=integer("reanchorMs",0,0,300000);c.presetId=integer("presetId",0,-1,5);
 c.worldDistance=number("worldDistance",200,32,1000);c.worldSide=number("worldSide",75,0,300);c.worldHeight=number("worldHeight",8,1,64);c.y=number("verticalPosition",.44f,.1f,.85f);c.width=number("maxWidth",.32f,.2f,.9f);c.opacity=number("opacity",1,.1f,1);c.glowStrength=number("glowStrength",.3f,0,3);c.glowRadius=number("glowRadius",.5f,0,1);c.wiggleFrequency=number("wiggleFrequency",8,4,24);c.wiggleAmount=number("wiggleAmount",.8f,0,2);c.spread=number("spreadDegrees",30,0,60);c.animationSpeed=number("animationSpeed",1,.25f,3);
 for(auto& p:c.layout.lyrics)p.distance=c.worldDistance;if(j.contains("layout"))c.layout=readLayout(j.at("layout"),c.layout);
 if(j.value("defaultsVersion",0)<2){migrateDefaultHeights(c.layout);if(c.reanchorMs==3000)c.reanchorMs=0;if(std::abs(c.y-.56f)<.0001f||std::abs(c.y-.61f)<.0001f)c.y=.44f;}
 if(j.contains("hudAppearance")){auto a=j.at("hudAppearance");if(!a.is_object())throw std::runtime_error("HUD appearance must be an object");auto f=[&](const char* key,float fallback,float low,float high){float v=a.value(key,fallback);return std::isfinite(v)?std::clamp(v,low,high):fallback;};c.hud.preset=unsigned(std::clamp(a.value("preset",0),0,3));c.hud.step=unsigned(std::clamp(a.value("step",8),1,64));c.hud.opacity=f("opacity",.48f,.2f,.95f);c.hud.font=f("font",22,16,32);c.hud.x=f("x",8,0,16384);c.hud.y=f("y",14,0,16384);c.hud.centerY=a.value("centerY",std::abs(c.hud.y-14.f)<.001f);c.hud.f10=a.value("f10",false);}
 c.textColor=parseColor(j.value("textColor",std::string("#F7E1D7")));c.accent=parseColor(j.value("accentColor",std::string("#F7E1D7")));return c;
}
inline nlohmann::json encodeConfig(const Config& c){return {
 {"wallFloorFollow",c.wallFloorFollow},{"wallReuse",c.wallReuse},{"wallSurface",c.wallSurface},{"wallLayout",wallLayoutJson(c.wallLayout)},
 {"hudAppearance",{{"preset",c.hud.preset},{"step",c.hud.step},{"opacity",c.hud.opacity},{"font",c.hud.font},{"x",c.hud.x},{"y",c.hud.y},{"centerY",c.hud.centerY},{"f10",c.hud.f10}}},{"wallFallback",c.wallFallback},{"wallEnabled",c.wallEnabled},{"wallAutomatic",c.wallAutomatic},{"wallLiveLyrics",c.wallLiveLyrics},{"wallSize",c.wallSize},{"wallCapacity",c.wallCapacity},{"wallLingerMs",c.wallLingerMs},{"wallFadeMs",c.wallFadeMs},{"wallRange",c.wallRange},{"wallSpacing",c.wallSpacing},{"fallbackReveal",c.fallbackReveal},{"wallBackdrop",c.wallBackdrop},{"wallDebug",c.wallDebug},{"progressiveReveal",c.progressiveReveal},{"reuseVisibleAnchor",c.reuseVisibleAnchor},{"reuseDistanceFactor",c.reuseDistanceFactor},{"lineWidthScale",c.lineWidthScale},{"defaultsVersion",2},{"layout",layoutJson(c.layout)},{"enabled",c.enabled},{"port",c.port},{"fontSize",c.size},{"worldLyrics",c.worldLyrics},{"worldTestAnchor",c.worldTestAnchor},{"worldDistance",c.worldDistance},{"worldSide",c.worldSide},{"worldHeight",c.worldHeight},{"cameraDiagnostics",c.cameraDiagnostics},{"demo",c.demo},{"translation",c.translation},{"alternate",c.alternate},{"verticalPosition",c.y},{"offsetMs",c.offset},{"outline",c.outline},{"glow",c.glow},{"glowStrength",c.glowStrength},{"glowRadius",c.glowRadius},{"opacity",c.opacity},{"maxWidth",c.width},{"transitionMs",c.transition},{"lingerMs",c.lingerMs},{"reanchorMs",c.reanchorMs},{"animationSpeed",c.animationSpeed},{"pop",c.pop},{"wiggle",c.wiggle},{"wiggleFrequency",c.wiggleFrequency},{"wiggleAmount",c.wiggleAmount},{"spreadDegrees",c.spread},{"keepAway",c.keepAway},{"debugOverlay",c.debugOverlay},{"textColor",colorText(c.textColor)},{"accentColor",colorText(c.accent)},{"showTitle",c.showTitle},{"showArtist",c.showArtist},{"presetId",c.presetId},{"displayMode","peak"}};}
inline const char* presetName(int id){static const char* names[]={u8"暖白浮光",u8"轻柔舒缓",u8"霓虹粉",u8"冰蓝",u8"琥珀金",u8"简洁纯净"};return id>=0&&id<6?names[id]:u8"自定义";}
inline Config applyPreset(Config c,int id){if(id<0||id>5)return c;c.presetId=id;c.pop=true;c.wiggle=true;c.outline=false;c.glow=true;c.glowStrength=.3f;c.glowRadius=.5f;c.wiggleFrequency=8;c.wiggleAmount=.8f;c.animationSpeed=1;c.transition=400;c.lingerMs=1000;c.textColor=c.accent=0xf7e1d7;
 switch(id){case 1:c.textColor=0xf6f1eb;c.accent=0xe3cedf;c.pop=false;c.wiggle=false;c.glowStrength=.2f;c.animationSpeed=.65f;c.transition=700;break;
 case 2:c.textColor=0xffd6ea;c.accent=0xff4d9f;c.glowStrength=.65f;c.glowRadius=.8f;c.wiggleFrequency=12;c.wiggleAmount=.55f;break;
 case 3:c.textColor=0xe5f7ff;c.accent=0x5ac8fa;c.glowStrength=.4f;c.wiggleFrequency=6;c.wiggleAmount=.25f;break;
 case 4:c.textColor=0xffebcd;c.accent=0xffb347;c.glowStrength=.35f;c.wiggleFrequency=6;c.wiggleAmount=.3f;break;
 case 5:c.textColor=c.accent=0xf6f6f6;c.glow=false;c.pop=false;c.wiggle=false;c.transition=500;break;}return c;
}
inline Config applyColorPreset(Config c,int id){if(id<0||id>5)return c;auto p=applyPreset(Config{},id);c.presetId=id;c.textColor=p.textColor;c.accent=p.accent;c.glow=p.glow;c.glowStrength=p.glowStrength;c.glowRadius=p.glowRadius;c.outline=p.outline;return c;}
inline int colorPresetId(const Config& c){if(c.presetId<0||c.presetId>5)return -1;auto p=applyColorPreset(Config{},c.presetId);return (c.textColor&0xffffff)==(p.textColor&0xffffff)&&(c.accent&0xffffff)==(p.accent&0xffffff)&&c.glow==p.glow&&c.glowStrength==p.glowStrength&&c.glowRadius==p.glowRadius&&c.outline==p.outline?c.presetId:-1;}
}
