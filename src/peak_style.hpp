#pragma once
// GPL-3.0: native port of PeakDangoMusic's LyricAnimation / LyricLineView,
// c9fe555b53f7d3cea85bfdcb92476a1ed6853ad0. See NOTICE.md.
#include "presentation.hpp"
#include "world_camera.hpp"
namespace dango::peak {
inline std::string trim(std::string s){auto units=graphemes(s);size_t start=0,end=s.size(),offset=0;for(auto& g:units){if(g.first>32&&g.first!=0x3000)break;start+=g.text.size();}for(auto i=units.rbegin();i!=units.rend();++i){if(i->first>32&&i->first!=0x3000)break;end-=i->text.size();}offset=std::min(start,end);return s.substr(offset,end-offset);}
inline Timeline splitTimeline(const Timeline& input){
 Timeline output;for(size_t index=0;index<input.size();++index){const auto& line=input[index];std::string text=line.text;
 if(text.find("纯音乐请欣赏")!=std::string::npos||text=="纯音乐"||text=="instrumental")continue;
 auto glyphs=graphemes(text);std::vector<size_t> cuts{0};size_t offset=0;
 for(size_t i=0;i<glyphs.size();){if(glyphs[i].first!=32&&glyphs[i].first!=0x3000){offset+=glyphs[i++].text.size();continue;}size_t begin=i;bool wide=false;while(i<glyphs.size()&&(glyphs[i].first==32||glyphs[i].first==0x3000)){wide|=glyphs[i].first==0x3000;offset+=glyphs[i++].text.size();}if(begin>0&&i<glyphs.size()&&(wide||(glyphs[begin-1].first>=0x2e80&&glyphs[i].first>=0x2e80)))cuts.push_back(offset);}
 if(cuts.size()==1){output.push_back(line);continue;}
 auto next=index+1<input.size()?input[index+1].start:line.start+int64_t(glyphs.size())*300;std::vector<Line> parts(cuts.size());size_t total=0,before=0;auto count=[](const std::string& s){size_t n=0;for(auto& g:graphemes(s))if(g.first>32&&g.first!=0x3000)++n;return n;};
 for(size_t i=0;i<cuts.size();++i){parts[i].text=trim(text.substr(cuts[i],(i+1<cuts.size()?cuts[i+1]:text.size())-cuts[i]));parts[i].translation=line.translation;total+=count(parts[i].text);}
 for(auto& part:parts){part.start=line.start+int64_t(std::max<int64_t>(0,next-line.start)*.85*before/std::max<size_t>(total,1));before+=count(part.text);}
 if(!line.words.empty()){offset=0;for(auto& word:line.words){size_t skip=0;for(auto& g:graphemes(word.text)){if(g.first>32&&g.first!=0x3000)break;skip+=g.text.size();}size_t at=offset+skip;auto it=std::upper_bound(cuts.begin(),cuts.end(),at);size_t part=size_t(it-cuts.begin()-1);auto& p=parts[part];if(p.words.empty())p.start=word.start;else p.start=std::min(p.start,word.start);p.words.push_back(word);offset+=word.text.size();}}
 std::vector<Line> merged;auto merge=[](Line& a,const Line& b){a.text+=" "+b.text;a.words.insert(a.words.end(),b.words.begin(),b.words.end());};
 for(auto& part:parts){if(!merged.empty()&&(count(part.text)<2||count(merged.back().text)<2||part.start-merged.back().start<500))merge(merged.back(),part);else merged.push_back(part);}
 if(merged.size()>1&&index+1<input.size()&&next-merged.back().start<500){auto last=merged.back();merged.pop_back();merge(merged.back(),last);}if(merged.size()<2)output.push_back(line);else output.insert(output.end(),merged.begin(),merged.end());
 }return output;
}
struct GlyphFrame {float alpha=0,scale=1,x=0,y=0,rotation=0;};
inline bool blank(const Glyph& g){return g.first<=32||g.first==0x3000;}
inline uint32_t seed(const std::string& text){uint32_t hash=2166136261u;for(auto& g:graphemes(text)){const auto& s=g.text;for(size_t i=0;i<s.size();){uint32_t c=uint8_t(s[i++]);unsigned n=0;if(c>=0xf0){c&=7;n=3;}else if(c>=0xe0){c&=15;n=2;}else if(c>=0xc0){c&=31;n=1;}while(n--&&i<s.size())c=(c<<6)|(uint8_t(s[i++])&63);if(c>0xffff){c-=0x10000;hash=(hash^(0xd800+(c>>10)))*16777619u;c=0xdc00+(c&1023);}hash=(hash^c)*16777619u;}}return hash;}
inline float noise(unsigned index,uint64_t frame,unsigned salt,uint32_t textSeed){uint32_t h=index*73856093u^uint32_t(frame*19349663u)^salt*83492791u^textSeed;h^=h>>13;h*=1540483477u;h^=h>>15;return (h&65535)/32767.5f-1;}
struct Animation {
 std::vector<Glyph> glyphs;std::vector<int64_t> starts;uint32_t textSeed=0;int64_t lineStart=0;double revealSpan=0;
 Animation(const Line& line,int64_t next):glyphs(graphemes(line.text)),textSeed(seed(line.text)),lineStart(line.start){
 double duration=std::max<int64_t>(next-line.start,400);revealSpan=std::min(std::max(glyphs.size()*55.,250.),duration*.6);
 starts.resize(glyphs.size(),line.start);
 std::vector<std::pair<std::string,int64_t>> timed;for(auto& word:line.words)for(auto& g:graphemes(word.text))if(!blank(g))timed.push_back({g.text,std::max(line.start,word.start)});
 size_t cursor=0;bool aligned=!timed.empty();for(size_t i=0;i<glyphs.size();++i){if(blank(glyphs[i]))continue;if(cursor>=timed.size()||timed[cursor].first!=glyphs[i].text){aligned=false;break;}starts[i]=timed[cursor++].second;}aligned=aligned&&cursor==timed.size();
 if(!aligned){std::vector<int> groups(glyphs.size(),-1);int group=0,cjk=0;bool pending=false;
 for(size_t i=0;i<glyphs.size();++i){auto& g=glyphs[i];if(blank(g)){if(pending){++group;pending=false;cjk=0;}continue;}bool asian=g.first>=0x2e80;if(pending&&((asian&&cjk==0)||(!asian&&cjk>0))){++group;pending=false;cjk=0;}groups[i]=group;pending=true;if(asian&&++cjk>=2){++group;pending=false;cjk=0;}}
 int count=std::max(1,group+(pending?1:0));double span=std::min(std::max(300.,duration)*.75,count*330.);for(size_t i=0;i<glyphs.size();++i)starts[i]=line.start+int64_t(span*std::max(0,groups[i])/count);}
 }
 GlyphFrame sample(size_t i,int64_t song,double seconds,bool pop=true,bool wiggle=true,float frequency=8,float amount=.8f)const{
 if(i>=glyphs.size()||blank(glyphs[i]))return {};GlyphFrame f;
 if(pop){double age=double(song-starts[i]);f.alpha=float(std::clamp(age*3/160.,0.,1.));double back=std::clamp(1-age/160.,0.,1.);f.scale=float(1+.55*back*back);}
 else{double t=std::clamp((song-lineStart-i/double(std::max<size_t>(glyphs.size(),1))*revealSpan)/180.,0.,1.);f.alpha=float(t*t*(3-2*t));f.y=(1-f.alpha)*5;}
 if(wiggle){auto bucket=uint64_t(std::max(0.,std::floor(seconds*std::clamp(frequency,4.f,24.f))));float a=std::clamp(amount,0.f,2.f);f.rotation=noise(unsigned(i),bucket,1,textSeed)*9*a*.01745329252f;f.x=noise(unsigned(i),bucket,2,textSeed)*.8f*a;f.y+=noise(unsigned(i),bucket,3,textSeed)*1.6f*a;f.scale*=1+noise(unsigned(i),bucket,4,textSeed)*.12f*a;}return f;
 }
};
struct LayoutGlyph {size_t index;float x,y,width;};
struct Layout {std::vector<LayoutGlyph> glyphs;std::vector<float> rows;float height=0;};
template<class Measure> Layout layout(const std::vector<Glyph>& glyphs,Measure measure,float maxWidth,float lineHeight){
 Layout out;float x=0;unsigned row=0;for(size_t i=0;i<std::min<size_t>(glyphs.size(),160);++i){auto& g=glyphs[i];float width=std::max(measure(g.text),lineHeight*.09f);
 if(g.first=='\n'||(x+width>maxWidth&&x>0)){out.rows.push_back(x);x=0;if(++row>=3)break;if(g.first=='\n')continue;}
 out.glyphs.push_back({i,x,float(row)*lineHeight,width});x+=width;}out.rows.push_back(x);out.height=out.rows.size()*lineHeight;for(auto& g:out.glyphs)g.x-=out.rows[size_t(g.y/lineHeight)]*.5f;return out;
}
inline Vec3 position(const CameraPose& pose,float distance,float spread,unsigned serial,float randomPitch){
 float yaw=(serial%2?1.f:-1.f)*spread*.8f*.01745329252f;float pitch=randomPitch*.01745329252f;
 return pose.eye+(pose.forward*std::cos(yaw)+pose.right*std::sin(yaw))* (distance*std::cos(pitch))+pose.up*(distance*std::sin(pitch));
}
inline Vec3 keepDistance(Vec3 anchor,const CameraPose& camera,float distance){Vec3 ray=anchor-camera.eye;float len=length(ray);return len>.0001f&&len<distance*.6f?camera.eye+ray*(distance*.6f/len):anchor;}
inline std::vector<float> blur(const std::vector<float>& input,int width,int height,int radius){
 if(width<=0||height<=0||input.size()!=size_t(width)*height)return {};radius=std::clamp(radius,1,8);auto a=input;std::vector<float> b(a.size());
 for(int pass=0;pass<3;++pass){for(int y=0;y<height;++y)for(int x=0;x<width;++x){float sum=0;for(int k=-radius;k<=radius;++k)if(x+k>=0&&x+k<width)sum+=a[y*width+x+k];b[y*width+x]=sum/(radius*2+1);}
 for(int y=0;y<height;++y)for(int x=0;x<width;++x){float sum=0;for(int k=-radius;k<=radius;++k)if(y+k>=0&&y+k<height)sum+=b[(y+k)*width+x];a[y*width+x]=sum/(radius*2+1);}}return a;
}
struct Life {float alpha=1,lift=0,scale=1;bool done=false;};
inline Life retire(double age,int exit=1){float t=float(age/.4);if(t>=1||exit==3)return {0,0,1,true};return {1-t,exit==1?t*.16f:0,exit==2?1-t*.5f:1,false};}
}
