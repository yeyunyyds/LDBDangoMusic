#pragma once
#include "lyrics_settings.hpp"
#include "native_menu_model.hpp"
#include "render_lifecycle.hpp"

namespace dango::menus {
inline std::string value(const std::string& name,const std::string& current){return name+": ["+current+"]";}
inline const char* on(bool b){return b?u8"开":u8"关";}
inline std::string number(float v){char text[40];std::snprintf(text,sizeof(text),"%.2f",v);return text;}
inline std::string saveLabel(const Config& current,const Config& saved,bool saving,bool failed,bool hasSaved=true){
 bool dirty=!hasSaved||encodeConfig(current)!=encodeConfig(saved);
 return value(u8"保存设置",dirty?u8"未保存":saving?u8"保存中":u8"已保存")+(saving&&dirty?u8" · 保存中":failed?u8" · 保存失败，见日志":"");
}
inline hud::Document root(const Config& c,const std::string& save){return {"L4D2DangoMusic 0.9.4",{
 {value(u8"歌词显示",on(c.enabled)),1},{u8"显示与歌词信息",20},{u8"位置与数量（固定预览）",23},
 {value(u8"歌词颜色",presetName(colorPresetId(c))),21},{u8"动画、速度与停留",22},{value(u8"场景挂画",on(c.wallEnabled)),30},
 {save,18},{u8"更多设置",35}}};}
inline hud::Document display(const Config& c){return {u8"显示与歌词信息",{
 {value(u8"翻译",on(c.translation)),2},{value(u8"单行长度",number(c.lineWidthScale)+"x"),31},
 {value(u8"同步偏移",std::to_string(c.offset)+"ms"),16},{value(u8"世界歌词",on(c.worldLyrics)),7},
 {u8"世界位置与回归",36},{u8"歌名与作者",37}}};}
inline hud::Document world(const Config& c){return {u8"世界位置与回归",{
 {value(u8"换句沿用视野内原位置",on(c.reuseVisibleAnchor)),29},
 {value(u8"复用距离上限",number(c.reuseDistanceFactor)+u8" × 配置距离"),33},
 {value(u8"靠近时后退",on(c.keepAway)),9},
 {value(u8"长句回到视野",c.reanchorMs>0?reanchorSeconds(c.reanchorMs)+u8"秒":u8"关闭"),19}}};}
inline hud::Document metadata(const Config& c){return {u8"歌名与作者",{
 {value(u8"显示歌名",on(c.showTitle)),5},{value(u8"显示作者",on(c.showArtist)),6},
 {u8"歌名位置（固定预览）",2100},{u8"作者位置（固定预览）",2101}}};}
inline hud::Document colors(const Config& c){return {u8"歌词颜色",{
 {value(u8"颜色预设",presetName(colorPresetId(c))),10},{value(u8"正文颜色",colorText(c.textColor)),11},
 {value(u8"光晕颜色",colorText(c.accent)),12},{value(u8"柔光",on(c.glow)),8}}};}
inline hud::Document palette(const Config& c,bool accent){
 const char* names[]={u8"暖白",u8"纯白",u8"樱花粉",u8"冰蓝",u8"金色",u8"薄荷绿",u8"淡紫",u8"橘红"};
 hud::Document doc{value(accent?u8"光晕颜色":u8"正文颜色",colorText(accent?c.accent:c.textColor)),{}};
 for(unsigned i=0;i<8;++i)doc.rows.push_back({names[i],(accent?310u:300u)+i});
 doc.rows.push_back({u8"自定义 RGB",accent?41u:40u});return doc;
}
inline hud::Document channels(const Config& c,bool accent){hud::Document doc{accent?u8"光晕 RGB":u8"正文 RGB",{}};auto color=accent?c.accent:c.textColor;for(unsigned i=0;i<3;++i)doc.rows.push_back({value(std::string(1,"RGB"[i]),std::to_string((color>>((2-i)*8))&255)),(accent?410u:400u)+i});return doc;}
inline hud::Document animation(const Config& c){return {u8"动画、速度与停留",{
 {value(u8"有逐字时间轴时按时间显字",on(c.progressiveReveal)),28},
 {value(u8"无逐字时间轴",c.fallbackReveal?u8"估算渐显":u8"整句显示"),32},
 {value(u8"显字动画速度",number(c.animationSpeed)+"x"),13},{value(u8"抖动",on(c.wiggle)),3},{value(u8"弹跳入场",on(c.pop)),4},
 {u8"停留与退场",38},{u8"恢复默认动画（抖动＋弹跳）",26},{u8"动画预览（示例歌词）",27}}};}
inline hud::Document timing(const Config& c){return {u8"停留与退场",{
 {value(u8"换句后额外停留",std::to_string(c.lingerMs)+"ms"),15},
 {value(u8"退场耗时",std::to_string(c.transition)+"ms"),14}}};}
inline hud::Document more(){return {u8"更多设置",{{u8"菜单外观",9000},{u8"版本、路径与播放器诊断",24}}};}
inline hud::Document wall(const Config& c,const std::string& status){return {u8"场景挂画歌词",{
 {value(u8"场景挂画",on(c.wallEnabled)),700},{value(u8"放置方式",c.wallAutomatic?(c.wallSurface?u8"自动寻找地面":u8"自动寻找墙面"):u8"手动挂画轮换"),709},
 {value(u8"内容",c.wallLiveLyrics?u8"歌词轮换":u8"固定示例"),702},{u8"尺寸、数量、搜索与停留",711},
 {u8"手动放置：3秒后按准星",701},{value(u8"新句无表面时固定世界歌词",on(c.wallFallback)),710},
 {value(u8"无新表面时复用旧挂画",on(c.wallReuse)),718},{u8"挂画外观与维护",39},{value(u8"状态",status),0}}};}
inline hud::Document wallAppearance(const Config& c){return {u8"挂画外观与维护",{
 {value(u8"背景面板",on(c.wallBackdrop)),703},{value(u8"调试标识",on(c.wallDebug)),705},{u8"挂画文字位置与大小（固定预览）",720},{value(u8"放置表面",c.wallSurface?u8"地面":u8"墙面"),719},{value(u8"地面文字随视角转向",on(c.wallFloorFollow)),726},{u8"清空全部挂画",704}}};}
}
