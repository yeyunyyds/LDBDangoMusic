#include "source_entity_binding.hpp"
#include <cstring>
#include <iostream>
#include <stdexcept>
struct Memory {
 std::vector<uint8_t> data=std::vector<uint8_t>(65536);unsigned reads=0;
 bool read(uint32_t address,void* target,size_t count){++reads;if(uint64_t(address)+count>data.size())return false;std::memcpy(target,data.data()+address,count);return true;}
 void put(uint32_t address,uint32_t value){std::memcpy(data.data()+address,&value,4);}
};
void check(bool v){if(!v)throw std::runtime_error("Entity binding regression");}
int main(){
 Memory m;constexpr uint32_t base=0x1000,object=0x2000,table=0x3000,methods[]={0x4000,0x4100,0x4200,0x4300};
 m.put(object,table);for(unsigned i=0;i<4;++i)m.put(table+i*4,base+methods[i]);
 auto inspect=[&](){return dango::entitywire::inspectBinding(m,"mock",object,base,object-base,table-base,methods);};
 auto original=inspect();check(original.layout()&&original.callable(15)&&original.details.empty());
 // Hooks in unused slots cannot invalidate a raw-layout read, or prevent
 // dispatch of independently verified model slots 1 and 3.
 m.put(table,0xdead0000);auto unused=inspect();check(unused.layout()&&unused.callable(10)&&!unused.callable(15)&&unused.details.size()==1);
 m.put(table+4,0xdead0010);auto used=inspect();check(used.layout()&&!used.callable(10)&&used.details.size()==2);
 check(used.details[1].find("slot 1")!=std::string::npos&&used.details[1].find("actual=")!=std::string::npos);
 // Unknown object/vtable layouts remain rejected; no address is dispatched.
 m.put(object,table+4);auto wrong=inspect();check(!wrong.layout()&&!wrong.callable(10)&&wrong.details[0].find("vtable")!=std::string::npos);
 m.reads=0;auto null=dango::entitywire::inspectBinding(m,"missing",0,base,object-base,table-base,methods);check(!null.layout()&&m.reads==0&&null.details[0].find("object")!=std::string::npos);
 std::cout<<"Passed: exact object/layout checks, uncalled-slot hook tolerance, verified model dispatch mask, detailed failures and no unknown pointer calls\n";
}
