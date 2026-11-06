#pragma once
#include "visual_style.hpp"
#include "peak_style.hpp"
namespace dango::visual {
inline float entrance(const Style& style,const peak::VisibleAnimation& motion,size_t index,double clock){if(!style.softEntrance)return 1;if(index>=motion.appeared.size()||motion.appeared[index]<0)return 0;float t=float(std::clamp((clock-motion.appeared[index])/.24,0.,1.));return t*t*(3-2*t);}
struct Tone {uint32_t color;float alpha=1,glow=1;};
inline Tone shade(const Style& s,const peak::Animation& animation,size_t index,int64_t song,uint32_t base,bool translated){
 Tone out{translated&&!s.translationFollow?s.translationColor:base,translated?s.translationOpacity:s.originalOpacity,1};
 if(index>=animation.starts.size())return out;
 // A line without real word timing is never presented as precisely timed.
 float active=0;bool pending=false;if(animation.hasWordTiming){auto onset=animation.starts[index];int64_t end=index<animation.ends.size()?animation.ends[index]:animation.lineEnd;for(size_t j=index+1;j<animation.starts.size();++j)if(animation.starts[j]>onset){end=std::min(end,animation.starts[j]);break;}pending=song<onset;if(!pending&&song<end){float attack=float(std::clamp(double(song-onset)/90.,0.,1.));float release=float(std::clamp(double(end-song)/120.,0.,1.));active=attack*release;}}
 if(s.tone&&animation.hasWordTiming){float brightness=pending?s.pending:s.sung;out.color=chroma::mix(chroma::mix(0,out.color,brightness),s.highlight,active*.65f);}
 if(s.dynamicGlow)out.glow=animation.hasWordTiming?.4f+.9f*active:.65f;
 return out;
}
}
