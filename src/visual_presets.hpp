#pragma once
#include "lyrics_settings.hpp"
namespace dango::visual {
enum class Scope{All,Colors,Animation,Layout};
inline bool metadataAnchorChanged(const Config& a,const Config& b){auto changed=[](const ComponentPosition& x,const ComponentPosition& y){return x.x!=y.x||x.y!=y.y||x.distance!=y.distance;};return a.worldLyrics!=b.worldLyrics||changed(a.layout.title,b.layout.title)||changed(a.layout.artist,b.layout.artist);}
inline const char* presetName(int index){const char* names[]={u8"极简字幕",u8"霓虹夜行",u8"落日胶片",u8"冰晶",u8"水墨",u8"呼吸悬浮",u8"美术馆"};return index>=0&&index<7?names[index]:u8"自定义";}
inline nlohmann::json capture(const Config& c,const std::string& name){auto all=encodeConfig(c);nlohmann::json fields=nlohmann::json::object();for(auto key:{"textColor","accentColor","glow","glowStrength","glowRadius","outline","opacity","colorEffect","colorSpeed","colorAmount","pop","wiggle","wiggleFrequency","wiggleAmount","animationSpeed","transitionMs","lingerMs","fontSize","lineWidthScale","maxWidth","wallBackdrop","visualStyle"})fields[key]=all.at(key);return {{"schema",1},{"name",name.substr(0,64)},{"settings",fields}};}
inline Config apply(Config c,const nlohmann::json& blob,Scope scope){
 if(!blob.is_object()||blob.value("schema",0)!=1||!blob.contains("settings")||!blob.at("settings").is_object())throw std::runtime_error("Invalid visual preset");
 auto fields=capture(c,"")["settings"];for(auto it=fields.begin();it!=fields.end();++it)if(blob.at("settings").contains(it.key()))it.value()=blob.at("settings").at(it.key());
 auto p=decodeConfig(fields);bool colors=scope==Scope::All||scope==Scope::Colors,animation=scope==Scope::All||scope==Scope::Animation,layout=scope==Scope::All||scope==Scope::Layout;
 if(colors){c.textColor=p.textColor;c.accent=p.accent;c.glow=p.glow;c.glowStrength=p.glowStrength;c.glowRadius=p.glowRadius;c.outline=p.outline;c.opacity=p.opacity;c.colorEffect=p.colorEffect;c.colorSpeed=p.colorSpeed;c.colorAmount=p.colorAmount;c.visual.tone=p.visual.tone;c.visual.pending=p.visual.pending;c.visual.sung=p.visual.sung;c.visual.highlight=p.visual.highlight;c.visual.dynamicGlow=p.visual.dynamicGlow;c.visual.originalOpacity=p.visual.originalOpacity;c.visual.translationColor=p.visual.translationColor;c.visual.translationFollow=p.visual.translationFollow;c.visual.translationOpacity=p.visual.translationOpacity;c.visual.wallEffects=p.visual.wallEffects;c.presetId=-1;}
 if(animation){c.pop=p.pop;c.wiggle=p.wiggle;c.wiggleFrequency=p.wiggleFrequency;c.wiggleAmount=p.wiggleAmount;c.animationSpeed=p.animationSpeed;c.transition=p.transition;c.lingerMs=p.lingerMs;c.visual.softEntrance=p.visual.softEntrance;c.visual.popStrength=p.visual.popStrength;c.visual.exit=p.visual.exit;c.visual.bob=p.visual.bob;c.visual.bobPeriod=p.visual.bobPeriod;c.visual.bobAmount=p.visual.bobAmount;c.visual.smoothScale=p.visual.smoothScale;c.visual.smoothMs=p.visual.smoothMs;}
 if(scope==Scope::All)c.wallBackdrop=p.wallBackdrop;
 if(layout){c.visual.metadataSize=p.visual.metadataSize;c.size=p.size;c.width=p.width;c.lineWidthScale=p.lineWidthScale;c.visual.translationSize=p.visual.translationSize;c.visual.translationGap=p.visual.translationGap;}
 return c;
}
inline nlohmann::json builtin(int index){if(index<0||index>=7)throw std::runtime_error("Unknown visual preset");Config c=applyPreset(Config{},5);c.visual.smoothScale=true;c.visual.translationOpacity=.8f;c.visual.translationSize=.68f;c.visual.translationGap=9;c.visual.tone=true;c.visual.popStrength=.35f;c.outline=true;c.glow=false;c.wiggle=false;c.pop=false;c.transition=650;c.visual.exit=1;
 switch(index){
 case 0:c.textColor=0xf4f4f4;c.visual.pending=.55f;break;
 case 1:c.textColor=0x79e8ff;c.accent=0xc19bff;c.colorEffect=1;c.glow=true;c.glowStrength=.3f;c.glowRadius=.65f;c.visual.highlight=0xffffff;c.visual.dynamicGlow=true;c.visual.wallEffects=true;c.pop=true;c.visual.popStrength=.35f;c.visual.exit=2;break;
 case 2:c.visual.softEntrance=true;c.textColor=0xffe5cb;c.accent=0xf08c72;c.colorEffect=1;c.glow=true;c.glowStrength=.15f;c.visual.translationFollow=false;c.visual.translationColor=0xddc6b1;c.transition=900;c.visual.exit=2;c.animationSpeed=.75f;break;
 case 3:c.textColor=0xc6edff;c.accent=0x82caff;c.glow=true;c.glowStrength=.2f;c.visual.dynamicGlow=true;c.visual.wallEffects=true;c.pop=true;c.visual.popStrength=.2f;c.visual.exit=3;break;
 case 4:c.visual.softEntrance=true;c.textColor=0xd9d5cc;c.accent=0x8c867a;c.visual.highlight=0xfff7e8;c.visual.translationFollow=false;c.visual.translationColor=0xb8b2a6;c.transition=1100;c.visual.exit=4;c.visual.pending=.65f;break;
 case 5:c.textColor=0xe4f6ec;c.accent=0xb9d4fb;c.colorEffect=4;c.colorSpeed=.08f;c.glow=true;c.glowStrength=.12f;c.visual.bob=true;c.visual.bobPeriod=4;c.visual.bobAmount=.006f;c.visual.tone=false;c.visual.exit=2;break;
 case 6:c.visual.metadataSize=.75f;c.textColor=0xf2e8d8;c.accent=0xd0bd9e;c.visual.tone=false;c.visual.translationSize=.58f;c.visual.translationOpacity=.72f;c.visual.translationGap=16;c.lineWidthScale=1.5f;c.transition=1000;c.visual.exit=1;break;}
 return capture(c,presetName(index));
}
inline bool occupied(const Config& c,int slot){return slot>=0&&slot<int(c.visualPresets.size())&&!c.visualPresets.at(slot).is_null();}
inline void save(Config& c,int slot){if(slot<0||slot>=8)return;while(c.visualPresets.size()<=size_t(slot))c.visualPresets.push_back(nullptr);c.visualPresets[slot]=capture(c,u8"自定义 "+std::to_string(slot+1));}
inline const char* paletteLabel(const Config& c){
 static const auto presets=[](){std::array<Config,7> out;for(int i=0;i<7;++i)out[i]=apply(Config{},builtin(i),Scope::Colors);return out;}();
 for(int i=0;i<7;++i){const auto& p=presets[i];const auto& a=c.visual;const auto& b=p.visual;
 if((c.textColor&0xffffff)==(p.textColor&0xffffff)&&(c.accent&0xffffff)==(p.accent&0xffffff)&&c.glow==p.glow&&c.glowStrength==p.glowStrength&&c.glowRadius==p.glowRadius&&c.outline==p.outline&&c.opacity==p.opacity&&c.colorEffect==p.colorEffect&&c.colorSpeed==p.colorSpeed&&c.colorAmount==p.colorAmount&&a.tone==b.tone&&a.pending==b.pending&&a.sung==b.sung&&a.highlight==b.highlight&&a.dynamicGlow==b.dynamicGlow&&a.originalOpacity==b.originalOpacity&&a.translationColor==b.translationColor&&a.translationFollow==b.translationFollow&&a.translationOpacity==b.translationOpacity&&a.wallEffects==b.wallEffects)return presetName(i);}
 const Style defaults;const auto& a=c.visual;if(a.tone||a.dynamicGlow||a.wallEffects||a.pending!=defaults.pending||a.sung!=defaults.sung||a.highlight!=defaults.highlight||a.originalOpacity!=1||a.translationOpacity!=1||!a.translationFollow)return u8"自定义";
 return colorPresetLabel(c);
}

}
