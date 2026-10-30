#pragma once
#include "entity_follow.hpp"
#include "source_entity_contract.hpp"
#include <map>
#include <optional>
#include <string>
#include <vector>
namespace dango::dialogue {
inline const char* roleName(int role){static const char* names[]={"Nick","Rochelle","Coach","Ellis","Bill","Zoey","Francis","Louis"};return role>=0&&role<8?names[role]:"Unknown";}
inline std::string trim(std::string s){auto white=[](unsigned char c){return c<=32;};while(!s.empty()&&white(s.front()))s.erase(s.begin());while(!s.empty()&&white(s.back()))s.pop_back();return s;}
inline std::string lower(std::string s){for(auto& c:s)if(c>='A'&&c<='Z')c=char(c+32);return s;}
inline std::string clean(const std::string& text){
 std::string out;bool tag=false;for(unsigned char c:text){if(c=='<'){tag=true;continue;}if(c=='>'&&tag){tag=false;continue;}if(tag)continue;if(c=='\n'||c=='\r'||c=='\t')c=' ';if(c>=32)out+=char(c);if(out.size()>4096)return {};}
 return tag?std::string{}:trim(out);
}
struct Spoken {int role=-1;std::string text,speaker,body;};
inline Spoken parse(const std::string& input){
 Spoken out;if(lower(input).find("<sfx>")!=std::string::npos)return out;out.text=clean(input);auto colon=out.text.find(':');size_t separator=1;auto wide=out.text.find(u8"：");if(wide!=std::string::npos&&(colon==std::string::npos||wide<colon)){colon=wide;separator=3;}
 if(colon==std::string::npos||colon>96)return out;out.speaker=trim(out.text.substr(0,colon));out.body=trim(out.text.substr(colon+separator));auto name=lower(out.speaker);
 if(name.size()>1&&name.front()=='['&&name.back()==']')name=trim(name.substr(1,name.size()-2));
 const std::vector<std::vector<std::string>> aliases={{"nick",u8"尼克"},{"rochelle",u8"罗谢尔",u8"羅謝爾",u8"罗雪尔",u8"蘿雪兒"},{"coach",u8"教练",u8"教練"},{"ellis",u8"艾利斯",u8"埃利斯",u8"艾丽斯"},{"bill",u8"比尔",u8"比爾"},{"zoey",u8"佐伊",u8"佐依",u8"佐薇"},{"francis",u8"弗朗西斯",u8"法兰西斯",u8"法蘭西斯"},{"louis",u8"路易斯",u8"路易",u8"刘易斯",u8"劉易斯"}};
 if(!out.body.empty())for(int i=0;i<8;++i)if(std::find(aliases[i].begin(),aliases[i].end(),name)!=aliases[i].end()){out.role=i;break;}return out;
}
inline bool speakerEntity(const inventory::Record& r,int role){return role>=0&&role<8&&(r.className=="CTerrorPlayer"||r.className=="SurvivorBot")&&r.team&&*r.team==2&&r.character&&*r.character==role&&r.dormant&&!*r.dormant&&(!r.lifeState||*r.lifeState==0)&&(!r.health||*r.health>0);}
struct Match {uint32_t handle=0;unsigned candidates=0;std::string status="unknown_speaker";};
inline Match match(const inventory::Frame& frame,int role,uint32_t epoch,uint64_t now){
 Match out;if(role<0||role>7)return out;if(frame.epoch!=epoch||!epoch){out.status="wrong_map";return out;}if(!frame.tick||now<frame.tick||now-frame.tick>500){out.status="stale_inventory";return out;}
 for(const auto& r:frame.records){if(!speakerEntity(r,role))continue;
 if(!follow::pose(frame,r.handle).ready)continue;++out.candidates;out.handle=r.handle;}
 out.status=out.candidates==1?"matched":out.candidates?"ambiguous_role":"no_entity";if(out.candidates!=1)out.handle=0;return out;
}
struct Caption {uint32_t epoch=0,handle=0,hash=0;int role=-1;uint64_t born=0,expires=0;std::string text;bool synthetic=false;};
struct Timeline {
 uint32_t epoch=0;std::vector<Caption> lines;
 void expire(uint32_t map,uint64_t now){if(epoch!=map){lines.clear();epoch=map;}lines.erase(std::remove_if(lines.begin(),lines.end(),[&](const Caption& c){return now<c.born||now>=c.expires;}),lines.end());}
 bool add(Caption c,uint32_t map,uint64_t now){expire(map,now);if(c.epoch!=map||!map||!c.handle||c.text.empty()||now<c.born||now>=c.expires)return false;
 auto same=std::find_if(lines.begin(),lines.end(),[&](const Caption& old){return old.handle==c.handle&&old.hash==c.hash&&old.text==c.text&&c.born>=old.born&&c.born-old.born<250;});if(same!=lines.end())return false;
 lines.erase(std::remove_if(lines.begin(),lines.end(),[&](const Caption& old){return old.handle==c.handle;}),lines.end());if(lines.size()>=8)lines.erase(lines.begin());lines.push_back(std::move(c));return true;}
};
// This game's bf_read uses a buffered-word layout, not the old m_iCurBit ABI.
struct BitRead {uint32_t debug=0,overflow=0;int32_t bits=0,bytes=0;uint32_t word=0;int32_t available=0;uint32_t next=0,end=0,data=0;};
static_assert(sizeof(BitRead)==36,"Verified x86 bf_read copy size");
struct WireCaption {uint32_t hash=0;unsigned durationMs=0;bool flag=false;};
template<class Reader> std::optional<WireCaption> readCaption(Reader& reader,uint32_t address){
 BitRead b;if(!entitywire::load(reader,address,b)||(b.overflow&255)||b.bits<48||b.bytes<6||b.bytes>65536||b.bits>int64_t(b.bytes)*8||b.available<0||b.available>32||!b.data||b.next<b.data||b.end<b.next||uint64_t(b.data)+b.bytes>0xffffffffull)return {};
 int64_t pos=int64_t((b.next-b.data)/4)*32-b.available+(b.bytes&3)*8;if(pos<0||pos+48>b.bits)return {};
 uint8_t bytes[7]{};unsigned shift=unsigned(pos%8),count=(shift+48+7)/8;if(uint64_t(b.data)+pos/8+count>uint64_t(b.data)+b.bytes||!reader.read(b.data+uint32_t(pos/8),bytes,count))return {};
 uint64_t value=0;for(unsigned i=0;i<count;++i)value|=uint64_t(bytes[i])<<(8*i);value>>=shift;return WireCaption{uint32_t(value),unsigned((value>>32)&32767)*100,bool((value>>47)&1)};
}
}
