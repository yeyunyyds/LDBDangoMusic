#pragma once
#include "core.hpp"
namespace dango {
inline float smooth(float x){x=std::clamp(x,0.f,1.f);return x*x*(3-2*x);}
struct Motion {float alpha,scale,lift;};
inline Motion motion(int64_t age,int fadeMs=180){float t=smooth(float(age)/std::max(fadeMs,1));return {t,1.f+.12f*(1-t)*(1-t),6.f*(1-t)};}
inline float outgoingAlpha(int64_t currentAge,int transitionMs){return 1-smooth(float(currentAge)/std::max(transitionMs,1));}
inline int64_t lyricPosition(int64_t position,int offsetMs){return position+offsetMs;}
struct Glyph {std::string text;uint32_t first;};
// Common combining sequences, emoji modifiers, flags and ZWJ chains stay together.
// This deliberately does not claim complete UAX #29 segmentation.
inline std::vector<Glyph> graphemes(const std::string& text){
 std::vector<Glyph> out;bool join=false;unsigned regional=0;
 for(size_t i=0;i<text.size();){size_t begin=i;unsigned char c=text[i++];uint32_t cp=c;int tail=0;
 if(c>=0xc2&&c<=0xdf){cp=c&31;tail=1;}else if(c>=0xe0&&c<=0xef){cp=c&15;tail=2;}else if(c>=0xf0&&c<=0xf4){cp=c&7;tail=3;}
 for(int k=0;k<tail&&i<text.size();++k){unsigned char next=text[i];if((next&0xc0)!=0x80)break;cp=(cp<<6)|(next&63);++i;}
 bool mark=(cp>=0x300&&cp<=0x36f)||(cp>=0x1ab0&&cp<=0x1aff)||(cp>=0x1dc0&&cp<=0x1dff)||(cp>=0xfe00&&cp<=0xfe0f)||(cp>=0xfe20&&cp<=0xfe2f)||(cp>=0x1f3fb&&cp<=0x1f3ff)||(cp>=0xe0100&&cp<=0xe01ef);
 bool flag=cp>=0x1f1e6&&cp<=0x1f1ff;
 if(!out.empty()&&(mark||join||cp==0x200d||(flag&&regional%2)))out.back().text+=text.substr(begin,i-begin);else out.push_back({text.substr(begin,i-begin),cp});
 join=cp==0x200d;regional=flag?regional+1:0;
 }return out;
}
struct Reveal {std::string text;int64_t start;};
inline std::vector<Reveal> reveal(const Line& line,int64_t next,bool character){
 std::vector<Reveal> out;
 if(!line.words.empty()){
 for(auto& word:line.words){auto units=graphemes(word.text);if(!character){out.push_back({word.text,word.start});continue;}for(size_t i=0;i<units.size();++i)out.push_back({units[i].text,word.start+int64_t(i)*std::max<int64_t>(word.duration,0)/int64_t(std::max<size_t>(units.size(),1))});}
 }else{
 auto units=graphemes(line.text);std::vector<std::string> groups;
 for(auto& g:units){bool latin=g.first<0x2e80&&g.first>32;if(!character&&latin&&!groups.empty()&&static_cast<unsigned char>(groups.back()[0])<128&&groups.back().back()!=' ')groups.back()+=g.text;else groups.push_back(g.text);}
 int64_t duration=std::max<int64_t>(next-line.start,500);int64_t span=std::min<int64_t>(duration*3/4,std::max<int64_t>(300,int64_t(groups.size())*100));
 for(size_t i=0;i<groups.size();++i)out.push_back({groups[i],line.start+int64_t(i)*span/int64_t(std::max<size_t>(groups.size(),1))});
 }return out;
}
}
