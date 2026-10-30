#include "menu_pages.hpp"
#include "hud_menu_policy.hpp"
#include <stdexcept>
#include <iostream>
using namespace dango;
void check(bool b){if(!b)throw std::runtime_error("Menu layout/save state regression");}
int main(){
 Config base;auto current=base;check(menus::wall(base,"ready").rows.size()==9);check(menus::wallAppearance(base).rows.size()<=9);
 check(menus::saveLabel(current,base,false,false,false)==u8"保存设置: [未保存]");
 auto label=menus::saveLabel(current,base,false,false);check(label==u8"保存设置: [已保存]");
 auto root=menus::root(current,label);check(root.rows.size()<=9&&root.rows[6].action==18);
 for(auto& row:root.rows)check(row.action!=10&&row.action!=11&&row.action!=31&&row.action!=16);
 check(root.rows[3].label==u8"歌词颜色: [暖白浮光]");
 auto display=menus::display(base);check(display.rows[1].action==31&&display.rows[2].action==16&&display.rows[2].label==u8"同步偏移: [500ms]");
 for(auto doc:{menus::root(base,label),menus::display(base),menus::world(base),menus::metadata(base),menus::colors(base),menus::animation(base),menus::timing(base),menus::more(),menus::wall(base,"active"),menus::wallAppearance(base),menus::palette(base,false),menus::palette(base,true),menus::channels(base,false)})check(doc.rows.size()<=9);
 check(menus::more().rows.back().action==850&&menus::more().rows[2].action==810);
 for(auto id:{35,36,37,38,39,40,41,720,721,722,723,730,741,800,810,811,812,850,860})check(hud::opensChild(id));
 current.translation=true;check(menus::saveLabel(current,base,false,false)==u8"保存设置: [未保存]");
 auto firstRequest=current;current.offset=600;
 // An older completed write only advances the saved baseline to that request;
 // it must never mark a newer edit as saved.
 check(menus::saveLabel(current,firstRequest,false,false)==u8"保存设置: [未保存]");
 check(menus::saveLabel(current,firstRequest,true,false).find(u8"[未保存]")!=std::string::npos);
 check(menus::saveLabel(current,firstRequest,false,true).find(u8"保存失败")!=std::string::npos);
 check(menus::saveLabel(current,current,false,false)==u8"保存设置: [已保存]");
 current=base;current.hud.font=28;current.hud.opacity=.75f;current.hud.f10=true;
 check(menus::saveLabel(current,base,false,false)==u8"保存设置: [未保存]");
 auto copy=decodeConfig(encodeConfig(current));check(copy.hud==current.hud);
 // Default menu height follows screen center; legacy custom coordinates stay manual.
 check(base.hud.centerY&&decodeConfig(nlohmann::json::object()).hud.centerY);
 check(decodeConfig({{"hudAppearance",{{"y",14}}}}).hud.centerY);
 auto custom=decodeConfig({{"hudAppearance",{{"y",220}}}});check(!custom.hud.centerY&&custom.hud.y==220);
 auto manualTop=decodeConfig({{"hudAppearance",{{"y",14},{"centerY",false}}}});check(!manualTop.hud.centerY);
 check(decodeConfig(encodeConfig(custom)).hud==custom.hud&&decodeConfig(encodeConfig(manualTop)).hud==manualTop.hud);
 auto centered=custom;centered.hud.centerY=true;check(menus::saveLabel(centered,custom,false,false)==u8"保存设置: [未保存]");
 current.pop=false;current.wiggle=false;current.animationSpeed=2;current.lingerMs=2000;auto selected=applyColorPreset(current,3);
 check(!selected.pop&&!selected.wiggle&&selected.animationSpeed==2&&selected.lingerMs==2000&&selected.hud==current.hud&&colorPresetId(selected)==3);
 selected.textColor=0x123456;check(colorPresetId(selected)==-1&&menus::root(selected,label).rows[3].label==u8"歌词颜色: [自定义]");
 selected=applyColorPreset(selected,3);selected.presetId=-1;check(colorPresetId(selected)==-1);
 std::cout<<"Passed: single-page categories, first-page save, bracketed current values, color-only presets/custom labels, saved baseline vs in-flight edits and persistent HUD appearance\n";
}
