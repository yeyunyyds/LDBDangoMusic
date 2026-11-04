#include "entity_auto.hpp"
#include "lyrics_settings.hpp"
#include "native_menu_model.hpp"
#include <stdexcept>
#include <iostream>
using namespace dango;
void check(bool b){if(!b)throw std::runtime_error("Category toggle / visual regression");}
int main(){
 auto types=autofollow::catalogue();AutoFollowConfig rules;setAutoRule(rules,"class:Tank",true);setAutoRule(rules,"class:CGnome",true);rules.enabled=true;
 auto partial=autofollow::counts(types,rules,0);check(partial.first==1&&partial.second>1);
 autofollow::toggleGroup(types,rules,0);check(!autoRule(rules,"class:Tank")&&autoRule(rules,"class:CGnome")&&rules.enabled);
 autofollow::toggleGroup(types,rules,0);check(autofollow::counts(types,rules,0).first==autofollow::counts(types,rules,0).second);
 autofollow::toggleGroup(types,rules,0);check(autofollow::counts(types,rules,0).first==0&&autoRule(rules,"class:CGnome"));
 autofollow::toggleGroup(types,rules);check(rules.rules.empty()&&!rules.enabled);autofollow::toggleGroup(types,rules);check(rules.enabled&&rules.rules.size()==types.size());autofollow::toggleGroup(types,rules);check(rules.rules.empty()&&!rules.enabled);
 using hud::RowTone;check(hud::rowTone(u8"开关: [开启]",820,true)==RowTone::Enabled);check(hud::rowTone(u8"开关: [关]",820,true)==RowTone::Disabled);check(hud::rowTone(u8"一键开关: [部分开启]",110001,true)==RowTone::Partial);check(hud::rowTone(u8"场景挂画: [开]",30,true)==RowTone::Neutral);check(hud::rowTone(u8"进入颜色",890,true)==RowTone::Neutral);check(hud::rowTone(u8"F10: 开",0,true)==RowTone::Enabled);check(hud::rowTone("info",0,false)==RowTone::Muted);
 Config base;base.lingerMs=2345;base.transition=765;base.entitySmooth=false;base.autoFollow.enabled=true;setAutoRule(base.autoFollow,"class:Tank",true);
 check(base.colorEffect==0&&chroma::sample(0xff123456,0xffffff,0,.5f,123,.2f,1)==0x123456);
 check(chroma::sample(0xff0000,0x0000ff,1,0,0,1,1)==0xff0000&&chroma::sample(0xff0000,0x0000ff,1,1,0,1,1)==0x0000ff);
 check(chroma::sample(0x123456,0xffffff,2,.5f,123,.2f,0)==0x123456);check(chroma::sample(0xffffff,0xff0000,2,0,0,.2f,1)!=chroma::sample(0xffffff,0xff0000,2,1,0,.2f,1));
 check(chroma::sample(0xffffff,0xff0000,3,.2f,0,.2f,1)!=chroma::sample(0xffffff,0xff0000,3,.2f,1,.2f,1));check(chroma::sample(0xffffff,0xff0000,4,0,1,.2f,1)==chroma::sample(0xffffff,0xff0000,4,1,1,.2f,1));
 for(int i=0;i<4;++i){auto c=applyChromaPalette(base,i);check(c.lingerMs==2345&&c.transition==765&&!c.entitySmooth&&autoRule(c.autoFollow,"class:Tank"));check(std::string(colorPresetLabel(c))==chroma::palette(i).name);auto restored=decodeConfig(encodeConfig(c));check(restored.colorEffect==c.colorEffect&&restored.colorSpeed==c.colorSpeed&&restored.colorAmount==c.colorAmount);c.colorAmount=.5f;check(std::string(colorPresetLabel(c))==u8"自定义");c=applyColorPreset(c,0);check(c.colorEffect==0&&colorPresetId(c)==0);}
 std::cout<<"Passed: scoped and global partial/off/on toggles, semantic HUD colors, legacy restoration, gradient/rainbow/flow/pulse colors, persistence and unchanged fade/auto settings\n";
}
