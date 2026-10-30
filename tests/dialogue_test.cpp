#include "dialogue.hpp"
#include <iostream>
#include <stdexcept>
using namespace dango;
void check(bool b){if(!b)throw std::runtime_error("Dialogue regression");}
struct Memory {std::vector<uint8_t> bytes=std::vector<uint8_t>(2048);bool read(uint32_t p,void* out,size_t n){if(uint64_t(p)+n>bytes.size())return false;std::memcpy(out,bytes.data()+p,n);return true;}};
inventory::Record player(uint32_t handle,int role){inventory::Record r;r.handle=handle;r.className="CTerrorPlayer";r.character=role;r.team=2;r.health=100;r.lifeState=0;r.dormant=false;r.parent=0xffffffffu;r.networkOrigin=Vec3{100,0,0};r.networkAngles=Vec3{};return r;}
int main(){
 check(dialogue::parse("<clr:255,255,255>Zoey: Reloading!").role==5);check(dialogue::parse(u8"教练：装弹中").role==2);check(dialogue::parse("[NICK]: hello").role==0);
 for(int i=0;i<8;++i)check(dialogue::parse(std::string(dialogue::roleName(i))+": test").role==i);
 for(auto s:{"Zoey is nearby","Someone: Zoey is nearby","Zoey:","<sfx>Zoey: noise","<bad Zoey: invalid"})check(dialogue::parse(s).role==-1);
 inventory::Frame f;f.epoch=7;f.tick=1000;for(int i=0;i<8;++i)f.records.push_back(player(0x9000+i,i));
 for(int i=0;i<8;++i){auto match=dialogue::match(f,i,7,1100);check(match.status=="matched"&&match.handle==uint32_t(0x9000+i));}
 check(dialogue::match(f,5,8,1100).status=="wrong_map"&&dialogue::match(f,5,7,1600).status=="stale_inventory");
 f.records.push_back(player(0xa005,5));check(dialogue::match(f,5,7,1100).status=="ambiguous_role"&&dialogue::match(f,5,7,1100).handle==0);f.records.back().dormant=true;check(dialogue::match(f,5,7,1100).handle==0x9005);f.records.pop_back();
 f.records[5].lifeState=1;check(dialogue::match(f,5,7,1100).status=="no_entity");f.records[5].lifeState=0;f.records[5].health=0;check(dialogue::match(f,5,7,1100).status=="no_entity");f.records[5].health=100;f.records[5].team=3;check(dialogue::match(f,5,7,1100).status=="no_entity");f.records[5].team=2;check(dialogue::speakerEntity(f.records[5],5)&&!dialogue::speakerEntity(f.records[5],0));
 dialogue::Timeline timeline;check(timeline.add({7,0x9005,42,5,1000,3000,"Zoey: one"},7,1100));check(timeline.add({7,0x9000,43,0,1100,3100,"Nick: two"},7,1100)&&timeline.lines.size()==2);
 check(!timeline.add({7,0x9005,42,5,1100,3100,"Zoey: one"},7,1100));check(timeline.add({7,0x9005,44,5,1200,3200,"Zoey: next"},7,1200)&&timeline.lines.size()==2);timeline.expire(7,3150);check(timeline.lines.size()==1&&timeline.lines[0].hash==44);timeline.expire(8,3200);check(timeline.lines.empty());
 check(!timeline.add({7,0x9005,44,5,1200,3200,"old"},8,3300));
 // Real buffered bf_read layouts at byte and bit offsets, including partial heads.
 for(unsigned prefix=0;prefix<8;++prefix){Memory mem;uint64_t payload=uint64_t(0xabc12345)|(uint64_t(25)<<32)|(uint64_t(1)<<47);unsigned n=(48+prefix+7)/8;for(unsigned bit=0;bit<48;++bit)if((payload>>bit)&1)mem.bytes[512+(prefix+bit)/8]|=uint8_t(1u<<((prefix+bit)%8));
 dialogue::BitRead reader;reader.bits=48+prefix;reader.bytes=n;reader.data=512;reader.end=512+n;unsigned head=n%4;reader.next=512+(head?head:4);reader.available=int(head?head*8:32)-int(prefix);std::memcpy(mem.bytes.data()+256,&reader,sizeof(reader));
 auto before=mem.bytes;auto parsed=dialogue::readCaption(mem,256);check(parsed&&parsed->hash==0xabc12345&&parsed->durationMs==2500&&parsed->flag&&mem.bytes==before);check(dialogue::readCaption(mem,256)->hash==parsed->hash);
 reader.overflow=1;std::memcpy(mem.bytes.data()+256,&reader,sizeof(reader));check(!dialogue::readCaption(mem,256));reader.overflow=0;reader.bits=40;std::memcpy(mem.bytes.data()+256,&reader,sizeof(reader));check(!dialogue::readCaption(mem,256));
 }
 std::cout<<"Passed: localized speakers, unique living survivor matching, ambiguity/map/stale guards, concurrent captions/expiry and non-consuming x86 bit readers\n";
}
