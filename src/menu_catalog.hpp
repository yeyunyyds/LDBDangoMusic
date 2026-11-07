#pragma once
#include "menu_pages.hpp"
#include "factory_defaults.hpp"
#include "menu_navigation_ids.hpp"
#include "lyric_motion.hpp"
#include "entity_follow.hpp"
#include <optional>
namespace dango::catalog {
using hud::Document;using hud::Row;using menus::value;using menus::on;using menus::number;
struct State {unsigned scope=0;std::string song,artist,source,status,wallStatus,entityStatus,autoState;bool manualEnabled=false,sourceLX=false,wordTiming=false;};
inline const char* scopeName(unsigned i){const char* names[]={u8"整套",u8"仅颜色",u8"仅动画",u8"仅排版"};return names[i%4];}
inline const nlohmann::json& theme(unsigned i){static const auto data=[](){std::array<nlohmann::json,17> all;for(unsigned n=0;n<7;++n)all[n]=visual::builtin(int(n));for(unsigned n=0;n<6;++n)all[7+n]=visual::capture(applyPreset(Config{},int(n)),presetName(int(n)));for(unsigned n=0;n<4;++n)all[13+n]=visual::capture(applyChromaPalette(Config{},int(n)),chroma::palette(int(n)).name);return all;}();if(i>=data.size())throw std::runtime_error("Unknown catalog theme");return data[i];}
inline std::string appearanceLabel(const Config& c){auto current=visual::capture(c,"")["settings"];for(unsigned i=0;i<17;++i)if(current==theme(i).at("settings"))return theme(i).at("name").get<std::string>();for(int i=0;i<8;++i)if(visual::occupied(c,i)&&current==c.visualPresets.at(i).at("settings"))return u8"收藏 "+std::to_string(i+1);return u8"自定义";}
enum class Field:unsigned {OriginalSize,OriginalOpacity,LineWidth,TranslationSize,TranslationOpacity,TranslationGap,ColorAmount,ColorSpeed,GlowStrength,GlowRadius,Pending,Sung,PopStrength,WiggleFrequency,WiggleAmount,BobPeriod,BobAmount,ScaleSmoothMs,MoveMs,Linger,Fade,WallRange,WallSpacing,WallCapacity,WallLinger,WallFade,ReuseFactor,MetadataSize};
constexpr unsigned FieldCount=28;
struct Number {const char* name;float value,low,high,step;const char* unit="";float display=1;};
inline Number numberField(const Config& c,Field field){switch(field){
 case Field::OriginalSize:return {u8"原文显示大小",c.visual.originalSize,.5f,2,.05f,"x"};case Field::OriginalOpacity:return {u8"原文透明度",c.visual.originalOpacity,.1f,1,.1f};case Field::LineWidth:return {u8"单行长度",c.lineWidthScale,.5f,4,.1f,"x"};
 case Field::TranslationSize:return {u8"翻译显示大小",c.visual.translationSize,.35f,1.5f,.05f,"x"};case Field::TranslationOpacity:return {u8"翻译透明度",c.visual.translationOpacity,.1f,1,.1f};case Field::TranslationGap:return {u8"原文与翻译间距",c.visual.translationGap,0,64,2};
 case Field::ColorAmount:return {u8"混色强度",c.colorAmount,0,1,.1f};case Field::ColorSpeed:return {u8"流动速度",c.colorSpeed,0,2,.1f};case Field::GlowStrength:return {u8"柔光强度",c.glowStrength,0,3,.1f};case Field::GlowRadius:return {u8"柔光半径",c.glowRadius,0,1,.1f};
 case Field::Pending:return {u8"未唱部分亮度",c.visual.pending,.1f,1,.1f};case Field::Sung:return {u8"已唱部分亮度",c.visual.sung,.1f,1,.1f};case Field::PopStrength:return {u8"弹跳幅度",c.visual.popStrength,0,1.5f,.1f};case Field::WiggleFrequency:return {u8"抖动频率",c.wiggleFrequency,4,24,1,"Hz"};case Field::WiggleAmount:return {u8"抖动幅度",c.wiggleAmount,0,2,.1f};
 case Field::BobPeriod:return {u8"整句悬浮周期",c.visual.bobPeriod,.2f,20,.2f,u8"秒"};case Field::BobAmount:return {u8"整句悬浮幅度",c.visual.bobAmount,0,.05f,.001f,u8"‰",1000};case Field::ScaleSmoothMs:return {u8"距离字号平滑耗时",float(c.visual.smoothMs),50,1000,50,"ms"};case Field::MoveMs:return {u8"共享移动耗时",float(c.moveMs),100,2000,100,"ms"};
 case Field::Linger:return {u8"换句后额外停留",float(c.lingerMs),0,5000,100,"ms"};case Field::Fade:return {u8"全局退场耗时",float(c.transition),100,3000,100,"ms"};case Field::WallRange:return {u8"挂画搜索距离",c.wallRange,160,1200,50};case Field::WallSpacing:return {u8"挂画间距",c.wallSpacing,40,400,10};case Field::WallCapacity:return {u8"挂画数量上限",float(c.wallCapacity),2,8,1};
 case Field::WallLinger:return {u8"挂画独立停留",float(c.wallLingerMs),0,15000,500,"ms"};case Field::WallFade:return {u8"挂画独立退场耗时",float(c.wallFadeMs),200,3000,100,"ms"};case Field::ReuseFactor:return {u8"位置复用距离上限",c.reuseDistanceFactor,1,10,.5f,u8"倍"};case Field::MetadataSize:return {u8"歌名作者外观缩放",c.visual.metadataSize,.4f,1.5f,.05f,"x"};}
 throw std::runtime_error("Unknown numeric setting");}
inline std::string numeric(float x){char out[32];std::snprintf(out,sizeof(out),"%.3f",x);std::string text=out;while(text.size()>1&&text.back()=='0')text.pop_back();if(text.back()=='.')text.pop_back();return text;}
inline uintptr_t fieldPage(Field f){return 5600+unsigned(f);}
inline Row field(const Config& c,Field f){auto n=numberField(c,f);bool permitted=(f!=Field::WallLinger&&f!=Field::WallFade)||c.wallIndependentTiming;return {value(n.name,numeric(n.value*n.display)+n.unit)+(permitted?"":u8"（跟随全局时停用）"),permitted?fieldPage(f):0};}
inline bool adjust(Config& c,uintptr_t action){if(action<6600||action>=6600+FieldCount*5)return false;unsigned code=unsigned(action-6600),operation=code%5;Field f=Field(code/5);if((f==Field::WallLinger||f==Field::WallFade)&&!c.wallIndependentTiming)return false;auto n=numberField(c,f);float v=operation<2?n.value+(operation?1:-1)*n.step:operation==2?numberField(installedDefaults(),f).value:operation==3?n.low:n.high;v=std::clamp(float(std::round(v*10000)/10000),n.low,n.high);
 switch(f){case Field::OriginalSize:c.visual.originalSize=v;break;case Field::OriginalOpacity:c.visual.originalOpacity=v;break;case Field::LineWidth:c.lineWidthScale=v;break;case Field::TranslationSize:c.visual.translationSize=v;break;case Field::TranslationOpacity:c.visual.translationOpacity=v;break;case Field::TranslationGap:c.visual.translationGap=v;break;case Field::ColorAmount:c.colorAmount=v;break;case Field::ColorSpeed:c.colorSpeed=v;break;case Field::GlowStrength:c.glowStrength=v;c.presetId=-1;break;case Field::GlowRadius:c.glowRadius=v;c.presetId=-1;break;case Field::Pending:c.visual.pending=v;break;case Field::Sung:c.visual.sung=v;break;case Field::PopStrength:c.visual.popStrength=v;break;case Field::WiggleFrequency:c.wiggleFrequency=v;break;case Field::WiggleAmount:c.wiggleAmount=v;break;case Field::BobPeriod:c.visual.bobPeriod=v;break;case Field::BobAmount:c.visual.bobAmount=v;break;case Field::ScaleSmoothMs:c.visual.smoothMs=int(v);break;case Field::MoveMs:c.moveMs=int(v);break;case Field::Linger:c.lingerMs=int(v);break;case Field::Fade:c.transition=int(v);break;case Field::WallRange:c.wallRange=v;break;case Field::WallSpacing:c.wallSpacing=v;break;case Field::WallCapacity:c.wallCapacity=int(v);break;case Field::WallLinger:c.wallLingerMs=int(v);break;case Field::WallFade:c.wallFadeMs=int(v);break;case Field::ReuseFactor:c.reuseDistanceFactor=v;break;case Field::MetadataSize:c.visual.metadataSize=v;break;}return true;}
inline Document root(const Config& c,const std::string& saved,bool following){Document d{"L4D2DangoMusic 0.9.8-preview2",{{value(u8"歌词显示",std::string(on(c.enabled))+(following?u8" · 实体跟随":"")),1},{u8"外观与预设",5000},{u8"动画与退场",5100},{u8"位置与场景",5200},{u8"音乐与歌曲信息",5300},{u8"实体跟随",5400},{saved,18},{u8"系统与恢复",5500}}};if(following)d.rows.push_back({u8"返回普通歌词",847});return d;}
inline std::optional<Document> page(uintptr_t id,const Config& c,const State& state={}){
 if(id>=5600&&id<5600+FieldCount){Field f=Field(id-5600);auto n=numberField(c,f);uintptr_t base=6600+unsigned(f)*5;return Document{value(n.name,numeric(n.value*n.display)+n.unit),{{u8"减小 "+numeric(n.step*n.display)+n.unit,base},{u8"增大 "+numeric(n.step*n.display)+n.unit,base+1},{u8"恢复此项默认值",base+2},{u8"设为最小值 "+numeric(n.low*n.display)+n.unit,base+3},{u8"设为最大值 "+numeric(n.high*n.display)+n.unit,base+4}}};}
 if(id>=1350&&id<=1357){unsigned slot=unsigned(id-1350);bool full=visual::occupied(c,int(slot));return Document{u8"收藏 "+std::to_string(slot+1)+(full?u8" · 已记录":u8" · 空槽"),{{value(u8"应用范围",scopeName(state.scope)),6000},{u8"应用此收藏",full?6200+slot:0},{u8"保存当前外观到此槽（覆盖）",1600+slot*10},{u8"清空此槽",full?1605+slot*10:0},{u8"记录后请返回主菜单保存设置",0}}};}
 switch(id){
 case 5000:return Document{u8"外观与预设",{{value(u8"视觉预设",appearanceLabel(c)),5010},{u8"我的预设（八个收藏）",1340},{u8"原文样式",5020},{u8"翻译样式",5030},{u8"配色与柔光",5040},{u8"逐字明暗",5050},field(c,Field::MetadataSize),{u8"效果预览",27}}};
 case 5010:{Document d{u8"视觉预设 · 新视觉／经典／渐变",{{value(u8"应用范围",scopeName(state.scope)),6000}}};for(unsigned i=0;i<17;++i)d.rows.push_back({theme(i).at("name").get<std::string>(),6100+i});d.pinned=1;d.sections={1,8,14};return d;}
 case 5020:return Document{u8"原文样式",{field(c,Field::OriginalSize),field(c,Field::OriginalOpacity),field(c,Field::LineWidth),{value(u8"正文颜色",colorText(c.textColor)),11}}};
 case 5030:return Document{u8"翻译样式",{{value(u8"显示翻译",on(c.translation)),2},field(c,Field::TranslationSize),field(c,Field::TranslationOpacity),field(c,Field::TranslationGap),{value(u8"沿用原文配色",on(c.visual.translationFollow)),1706},{value(u8"独立颜色",colorText(c.visual.translationColor))+u8"（选择后改用独立配色）",1371}}};
 case 5040:return Document{u8"配色与柔光",{{value(u8"着色方式",chroma::name(c.colorEffect)),891},{value(u8"第二色／光晕颜色",colorText(c.accent)),12},field(c,Field::ColorAmount),field(c,Field::ColorSpeed),{value(u8"柔光",on(c.glow)),8},field(c,Field::GlowStrength),field(c,Field::GlowRadius),{value(u8"描边",on(c.outline)),6500}}};
 case 5050:return Document{u8"逐字明暗 · "+std::string(state.wordTiming?u8"当前句有逐字时间":u8"当前句无逐字时间"),{{value(u8"明暗分层",on(c.visual.tone)),1750},field(c,Field::Pending),field(c,Field::Sung),{value(u8"当前词高光颜色",colorText(c.visual.highlight)),1372},{value(u8"动态柔光",c.glow?on(c.visual.dynamicGlow):u8"已停用（需开启柔光）"),c.glow?1751u:0u},{u8"基础柔光设置",5040},{u8"分层按真实逐字时间；显示整句可看见未唱部分",0}}};
 case 5060:{Document d{u8"效果预览 · 原文与翻译 · 循环入场和退场",{{u8"保存设置",18},{u8"结束预览并回主菜单",2099}}};d.note=u8"预览不控制音乐，返回上层即恢复当前歌曲。";return d;}
 case 5100:return Document{u8"动画与退场",{{u8"显字方式",5110},{u8"入场效果",5111},{u8"抖动",5112},{u8"整句悬浮",5113},{u8"换句移动",5114},{u8"距离字号平滑",5115},{u8"停留与退场",5116},{u8"动画预览",27}}};
 case 5110:return Document{u8"显字方式",{{value(u8"有逐字时间时按时间显字",on(c.progressiveReveal)),28},{value(u8"无逐字时间轴",c.fallbackReveal?u8"估算渐显":u8"整句显示"),32},{value(u8"显字动画速度",number(c.animationSpeed)+"x"),13}}};
 case 5111:return Document{u8"入场效果",{{value(u8"弹跳入场",on(c.pop)),4},field(c,Field::PopStrength),{value(u8"柔和渐入",on(c.visual.softEntrance)),1777}}};
 case 5112:return Document{u8"抖动",{{value(u8"抖动",on(c.wiggle)),3},field(c,Field::WiggleFrequency),field(c,Field::WiggleAmount)}};
 case 5113:return Document{u8"整句悬浮",{{value(u8"上下悬浮",on(c.visual.bob)),1770},field(c,Field::BobPeriod),field(c,Field::BobAmount)}};
 case 5114:return Document{u8"换句移动",{{value(u8"换句位置移动",on(c.lyricMove)),110011},{value(u8"移动曲线",movement::name(c.moveCurve))+u8"（与实体平滑共用）",879},field(c,Field::MoveMs)}};
 case 5115:return Document{u8"距离字号平滑",{{value(u8"平滑缩放",on(c.visual.smoothScale)),1760},field(c,Field::ScaleSmoothMs)}};
 case 5116:return Document{u8"停留与退场",{field(c,Field::Linger),field(c,Field::Fade),{value(u8"退场外观",visual::exitName(c.visual.exit)),5140}}};
 case 5140:{Document d{u8"退场外观（全局）",{}};for(int i=0;i<5;++i)d.rows.push_back({std::string(c.visual.exit==i?u8"当前 · ":"")+visual::exitName(i),1740u+unsigned(i)});return d;}
 case 5200:return Document{u8"位置与场景",{{value(u8"显示空间",c.worldLyrics?u8"世界":u8"屏幕"),7},{u8"歌词位置与数量（固定预览）",5201},{u8"歌名与作者位置（固定预览）",5202},{u8"位置复用与回归",36},{u8"场景挂画",5230}}};
 case 5201:{Document d{u8"歌词位置与数量 · 固定预览",{{value(u8"歌词位置数量",std::to_string(c.layout.lyricCount)),2003},{u8"统一调整全部歌词",5203}}};for(int i=0;i<c.layout.lyricCount;++i)d.rows.push_back({u8"歌词位置 "+std::to_string(i+1),2102u+unsigned(i)});d.rows.push_back({u8"重置歌词布局",2005});d.pinned=2;return d;}
 case 5202:return Document{u8"歌名与作者位置 · 固定预览",{{u8"歌名位置",2100},{u8"作者位置",2101},{u8"重置歌名与作者布局",6912}}};
 case 5203:return Document{u8"统一调整全部歌词 · 固定预览",{{u8"左右位置",3200},{u8"高低位置",3201},{u8"距离",3202},{u8"组件大小",3203},{u8"重置歌词布局",2005}}};
 case 5230:return Document{u8"场景挂画",{{value(u8"挂画歌词",on(c.wallEnabled)),700},{value(u8"放置方式",c.wallAutomatic?u8"自动":u8"手动"),709},{value(u8"放置表面",c.wallSurface?u8"地面":u8"墙面"),719},{u8"手动放置：3 秒后按准星",c.wallEnabled?701u:0u},{u8"搜索与复用",5231},{u8"外观与布局",5232},{u8"时间设置",5233},{u8"清空现有挂画",704},{state.wallStatus,0}}};
 case 5231:return Document{u8"挂画搜索与复用",{field(c,Field::WallRange),field(c,Field::WallSpacing),field(c,Field::WallCapacity),{value(u8"无合适表面时复用旧挂画",on(c.wallReuse)),718},{value(u8"无合适表面时使用世界歌词",on(c.wallFallback)),710}}};
 case 5232:return Document{u8"挂画外观与布局",{{value(u8"面板尺寸",c.wallSize==0?u8"小 80×28":c.wallSize==1?u8"中 120×40":u8"大 180×60"),712},{u8"歌名布局（固定预览）",721},{u8"作者布局（固定预览）",722},{u8"歌词布局，包含翻译（固定预览）",723},{value(u8"背景面板",on(c.wallBackdrop)),703},{value(u8"挂画柔光与描边",(c.glow||c.outline)?on(c.visual.wallEffects):u8"已停用（需柔光或描边）"),(c.glow||c.outline)?1756u:0u},{value(u8"地面文字随视角旋转",c.wallSurface?on(c.wallFloorFollow):u8"已停用（仅地面有效）"),c.wallSurface?726u:0u},{u8"重置挂画文字布局",724}}};
 case 5233:return Document{u8"挂画时间设置",{{value(u8"时间来源",c.wallIndependentTiming?u8"独立设置":u8"跟随全局"),727},c.wallIndependentTiming?field(c,Field::WallLinger):Row{value(u8"额外停留",std::to_string(c.lingerMs)+u8"ms（来自全局）"),0},c.wallIndependentTiming?field(c,Field::WallFade):Row{value(u8"退场耗时",std::to_string(c.transition)+u8"ms（来自全局）"),0},{value(u8"退场外观（全局共享）",visual::exitName(c.visual.exit)),5140},{u8"调整全局停留与退场",5116}}};
 case 5300:return Document{u8"音乐与歌曲信息",{{value(u8"音乐来源",media::sourceName(c.musicSource)),9500},{value(u8"歌词同步偏移",std::to_string(c.offset)+"ms"),16},{value(u8"显示歌名",on(c.showTitle)),5},{value(u8"显示作者",on(c.showArtist)),6},{u8"当前歌曲："+state.song,0},{u8"歌手："+state.artist,0},{u8"当前来源："+state.source,0},{u8"连接状态："+state.status,0}}};
 case 5400:return Document{u8"实体跟随",{{u8"手动锁定与解除",5410},{u8"自动追踪",5420},{u8"实体歌词布局",812},{u8"跟随运动",5402},{u8"关闭全部实体跟随",6904}}};
 case 5402:return Document{u8"实体跟随运动",{{value(u8"实体位置平滑",on(c.entitySmooth)),110010},{value(u8"移动曲线",movement::name(c.moveCurve))+u8"（与换句移动共用）",879},field(c,Field::MoveMs),{u8"实体专属上下往复",818}}};
 case 5500:{Document d{u8"系统与恢复",{{u8"菜单外观",9000},{u8"恢复默认外观（保留布局与收藏）",6900},{u8"恢复全部设置（保留收藏）",5501},{u8"一起听（实验，默认关闭）",9602},{u8"版本与路径",24}}};
d.note=u8"恢复外观只重置歌词样式和动画，布局、音乐、规则及收藏保留。全部恢复按安装默认值重置，收藏保留。";
#if DANGO_DIAGNOSTICS
 d.rows.push_back({u8"独立诊断工具",9600});d.rows.push_back({u8"实体诊断",800});
#endif
 return d;}
 case 5501:{Document d{u8"确认恢复全部设置",{{u8"确认恢复并返回主菜单",6901},{u8"取消",6902}}};d.note=u8"将恢复安装默认值、解除实体锁定、清理显示并关闭联机。八个收藏保留；不自动覆盖配置，需回主菜单保存。";return d;}
 default:return {};}
}
}
