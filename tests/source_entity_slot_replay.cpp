#include "source_entity_contract.hpp"
#include "source_entity_profile.hpp"
#include "json.hpp"
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace dango;namespace wire=dango::entitywire;namespace profile=source_entities::profile;
struct Memory {
 std::map<uint32_t,std::vector<uint8_t>> segments;
 bool read(uint32_t address,void* out,size_t count){auto i=segments.upper_bound(address);if(i==segments.begin())return false;--i;auto offset=uint64_t(address)-i->first;if(offset+count>i->second.size())return false;std::memcpy(out,i->second.data()+offset,count);return true;}
};
void check(bool v){if(!v)throw std::runtime_error("Real DLL slot replay regression");}
int main(int argc,char** argv){
 if(argc!=2)throw std::runtime_error("Pass the --replay-output JSON from verify_entity_inventory.py");std::ifstream file(argv[1]);auto cases=nlohmann::json::parse(file);check(cases.size()>=4);unsigned count=0,nonC=0;
 for(const auto& c:cases){Memory m;for(const auto& segment:c.at("segments"))m.segments[segment[0].get<uint32_t>()]=segment[1].get<std::vector<uint8_t>>();constexpr uint32_t base=0x1000,net=base+8;auto vt=c.at("vtable").get<uint32_t>();auto& object=m.segments[base];object.resize(512);std::memcpy(object.data()+8,&vt,4);int32_t index=17;std::memcpy(object.data()+8+0x50,&index,4);object[8+0xd5]=1;
 auto method=[&](unsigned slot){uint32_t f=0;check(wire::load(m,vt+slot*4,f));return f;};
 // Replay the old failure using the actual reported functions, without ever
 // executing a game instruction or loading the game DLL into this process.
 check(!wire::addressGetter(m,method(2),net));check(!wire::byteGetter(m,method(8),net));check(!wire::addressGetter(m,method(11),net));
 auto address=wire::addressGetter(m,method(profile::ClassSlot),net);check(bool(address));wire::ClientClass cls;check(wire::load(m,*address,cls));auto name=wire::text(m,cls.name);check(name==c.at("name").get<std::string>()&&wire::className(name));nonC+=name[0]!='C';
 check(wire::addressGetter(m,method(profile::DataBaseSlot),net)==base);
 bool constant=c.at("constant_dormant_false").get<bool>();check(wire::byteGetter(m,method(profile::DormantSlot),net)==!constant);index=-1;std::memcpy(object.data()+8+0x50,&index,4);check(wire::byteGetter(m,method(profile::DormantSlot),net)==false);++count;
 }
 check(nonC>0);std::cout<<"Passed: "<<count<<" real DLL networkable tables; old 2/8/11 failure reproduced, corrected 1/7/10 class/base/dormancy decoded; "<<nonC<<" valid non-C class names\n";
}
