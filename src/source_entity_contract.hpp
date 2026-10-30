#pragma once
#include "world_camera.hpp"
#include <string>
#include <vector>
#include <map>
#include <set>
#include <optional>
#include <cstring>
namespace dango::entitywire {
struct Entry {uint32_t entity=0,serial=0,previous=0,next=0;};
struct ClientClass {uint32_t create=0,event=0,name=0,table=0,next=0;int32_t id=0;};
struct Table {uint32_t props=0;int32_t count=0;uint32_t decoder=0,name=0;uint8_t initialized=0,inMain=0,padding[2]{};};
struct Prop {uint32_t name=0;int32_t type=0,flags=0,stringSize=0;uint8_t inside=0,padding[3]{};uint32_t extra=0,array=0,lengthProxy=0,proxy=0,tableProxy=0,table=0;int32_t offset=0,stride=0,elements=0;uint32_t parentName=0;};
static_assert(sizeof(Entry)==16&&sizeof(ClientClass)==24&&sizeof(Table)==20&&sizeof(Prop)==60,"x86 diagnostic wire layouts");
struct Field {uint32_t offset=0;int32_t type=0;};
using Fields=std::map<std::string,Field>;
inline bool className(const std::string& name){
 if(name.empty()||name.size()>127)return false;
 auto letter=[](unsigned char c){return (c>='A'&&c<='Z')||(c>='a'&&c<='z')||c=='_';};
 if(!letter(static_cast<unsigned char>(name[0])))return false;
 for(unsigned char c:name)if(!letter(c)&&!(c>='0'&&c<='9'))return false;return true;
}
template<class Reader,class T> bool load(Reader& r,uint32_t address,T& out){return address&&uint64_t(address)+sizeof(T)<=0x100000000ull&&r.read(address,&out,sizeof(T));}
template<class Reader> std::string text(Reader& r,uint32_t address,size_t limit=128){
 std::string result;if(!address)return result;for(size_t i=0;i<limit;){char bytes[16];size_t n=std::min(sizeof(bytes),limit-i);if(uint64_t(address)+i+n>0x100000000ull)return {};
 if(!r.read(address+uint32_t(i),bytes,n)){n=1;if(!r.read(address+uint32_t(i),bytes,n))return {};}
 for(size_t k=0;k<n;++k){unsigned char c=bytes[k];if(!c)return result;if(c<32||c==127)return {};result+=char(c);}i+=n;}return {};
}
inline std::optional<uint32_t> handle(unsigned index,uint32_t serial){if(index>=4096||serial>0xfffff)return {};uint32_t value=(serial<<12)|index;return value==0xffffffffu?std::nullopt:std::optional<uint32_t>(value);}
// Walk only embedded baseclass tables, not arbitrary proxy-backed datatables.
// Fields are network properties; they are NOT interpolated render poses.
template<class Reader> bool fields(Reader& r,uint32_t root,Fields& result){
 Fields next;std::set<uint32_t> seen;uint32_t address=root,base=0;size_t budget=8192;
 for(unsigned depth=0;address&&depth<16;++depth){if(!seen.insert(address).second)return false;Table t;if(!load(r,address,t)||t.count<0||t.count>2048||size_t(t.count)>budget||text(r,t.name).rfind("DT_",0)!=0)return false;budget-=size_t(t.count);
 std::vector<Prop> props(size_t(t.count));if(t.count&&(!t.props||uint64_t(t.props)+props.size()*sizeof(Prop)>0x100000000ull||!r.read(t.props,props.data(),props.size()*sizeof(Prop))))return false;
 uint32_t parent=0,parentOffset=0;
 for(const auto& p:props){auto name=text(r,p.name);if(name.empty()||p.type<0||p.type>7||p.offset<0||p.offset>65536)return false;
 if(name=="baseclass"&&p.type==6){if(parent||!p.table)return false;parent=p.table;parentOffset=uint32_t(p.offset);}
 if(name=="moveparent"||name=="m_iParentAttachment"||name=="m_vecOrigin"||name=="m_angRotation"||name=="m_nModelIndex"||name=="m_iTeamNum"||name=="m_hOwnerEntity"||name=="m_hActiveWeapon"||name=="m_hViewModel[0]"){
 if(uint64_t(base)+uint32_t(p.offset)>65536)return false;next.emplace(name,Field{base+uint32_t(p.offset),p.type});}}
 if(!parent){result=std::move(next);return true;}if(uint64_t(base)+parentOffset>65536)return false;base+=parentOffset;address=parent;
 }return false;
}
// Decode tiny read-only getters instead of invoking unknown entity virtuals.
template<class Reader> std::optional<uint32_t> addressGetter(Reader& r,uint32_t code,uint32_t self){
 uint8_t b[8];if(!r.read(code,b,sizeof(b)))return {};int64_t out=0;
 auto u32=[&](int i){uint32_t x;std::memcpy(&x,b+i,4);return x;};
 if(b[0]==0xb8&&b[5]==0xc3)out=u32(1);
 else if(b[0]==0x8b&&b[1]==0xc1&&b[2]==0xc3)out=self;
 else if(b[0]==0x8d&&b[1]==0x41&&b[3]==0xc3)out=int64_t(self)+int8_t(b[2]);
 else if(b[0]==0x8d&&b[1]==0x81&&b[6]==0xc3)out=int64_t(self)+int32_t(u32(2));else return {};
 return out>0&&out<=0xffffffffll?std::optional<uint32_t>(uint32_t(out)):std::nullopt;
}
template<class Reader> std::optional<bool> byteGetter(Reader& r,uint32_t code,uint32_t self){
 uint8_t b[8];if(!r.read(code,b,sizeof(b)))return {};int64_t address=0;
 if(b[0]==0x32&&b[1]==0xc0&&b[2]==0xc3)return false;
 if(b[0]==0xb0&&b[1]<=1&&b[2]==0xc3)return b[1]!=0;
 if(b[0]==0x8a&&b[1]==0x41&&b[3]==0xc3)address=int64_t(self)+int8_t(b[2]);
 else if(b[0]==0x8a&&b[1]==0x81&&b[6]==0xc3){int32_t offset;std::memcpy(&offset,b+2,4);address=int64_t(self)+offset;}
 else if(b[0]==0x83&&b[1]==0x79&&b[3]==0xff&&b[4]==0x74&&b[5]==0x07&&b[6]==0x8a&&b[7]==0x81){
 // Verified IsDormant: an unregistered index (-1) returns false; otherwise
 // return the object's boolean field. Decode reads only; never call the getter.
 uint8_t codeBytes[16];if(!r.read(code,codeBytes,sizeof(codeBytes))||codeBytes[12]!=0xc3||codeBytes[13]!=0x32||codeBytes[14]!=0xc0||codeBytes[15]!=0xc3)return {};
 auto indexAddress=int64_t(self)+int8_t(b[2]);if(indexAddress<=0||indexAddress>0xffffffffll)return {};int32_t index;if(!load(r,uint32_t(indexAddress),index))return {};if(index==-1)return false;
 int32_t offset;std::memcpy(&offset,codeBytes+8,4);address=int64_t(self)+offset;
 }else return {};
 if(address<=0||address>0xffffffffll)return {};uint8_t value;if(!load(r,uint32_t(address),value)||value>1)return {};return value!=0;
}
}
