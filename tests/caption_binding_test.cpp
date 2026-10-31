#include "caption_binding.hpp"
#include <iostream>
#include <stdexcept>
namespace cw=dango::captionwire;
void check(bool b){if(!b)throw std::runtime_error("Caption registry hook regression");}
struct Memory {
 std::vector<uint8_t> bytes=std::vector<uint8_t>(65536);std::vector<uint32_t> writes;
 bool read(uint32_t p,void* out,size_t n){if(uint64_t(p)+n>bytes.size())return false;std::memcpy(out,bytes.data()+p,n);return true;}
 void put(uint32_t p,uint32_t x){std::memcpy(bytes.data()+p,&x,4);}
 uint32_t get(uint32_t p){uint32_t x=0;check(read(p,&x,4));return x;}
 void text(uint32_t p,const char* s){std::memcpy(bytes.data()+p,s,std::strlen(s)+1);}
 bool replace(uint32_t p,uint32_t expected,uint32_t wanted){if(get(p)!=expected)return false;writes.push_back(p);put(p,wanted);return true;}
 void vector(uint32_t at,uint32_t data,int count){put(at,data);put(at+4,uint32_t(count));put(at+12,uint32_t(count));put(at+16,data);}
};
constexpr uint32_t A=0x1011fbc0,B=0x1011fbf0,HA=0x20100000,HB=0x20100010,Foreign=0x30123450;
Memory fixture(){Memory m;m.put(0x100,0x200);m.put(0x204,0x300);m.put(0x214,2);m.put(0x314,0x500);m.put(0x32c,0x600);m.put(0x504,0x700);m.put(0x604,0x740);m.text(0x700,"CloseCaption");m.text(0x740,"CloseCaptionDirect");m.vector(0x508,0x900,2);m.put(0x900,A);m.put(0x904,Foreign);m.vector(0x51c,0x980,1);m.put(0x980,A);m.vector(0x608,0xa00,1);m.put(0xa00,B);return m;}
cw::Registry inspect(Memory& m){return cw::inspect(m,0x100,A,B,HA,HB);}
auto reconcile(Memory& m,bool install){return cw::reconcile(inspect(m),install,A,B,HA,HB,[&](uint32_t p,uint32_t a,uint32_t b){return m.replace(p,a,b);});}
int main(){
 auto m=fixture();
 // Reproduce build 1's reported external dispatcher RVA: the old entry check
 // rejects it. The callback path never needs to replace or call that target.
 m.put(0x180,uint32_t(0x10000000u+3741388896u));check(m.get(0x180)!=0x100a9160);
 auto r=inspect(m);check(r.valid&&r.captionId==0&&r.directId==1&&r.slots.size()==3&&r.foreign.size()==1);
 auto installed=reconcile(m,true);check(installed.active==3&&installed.changed==3&&m.get(0x180)==0xef011060&&m.get(0x904)==Foreign);
 auto again=reconcile(m,true);check(again.active==3&&again.changed==0&&again.raced==0);
 // CUtlVector moves: restore must enumerate current cells, not remembered heap addresses.
 m.put(0xb00,m.get(0x900));m.put(0xb04,Foreign);m.vector(0x508,0xb00,2);m.writes.clear();auto restored=reconcile(m,false);
 check(restored.changed==3&&m.get(0xb00)==A&&m.get(0x980)==A&&m.get(0xa00)==B&&m.get(0x900)==HA&&m.get(0xb04)==Foreign);check(std::find(m.writes.begin(),m.writes.end(),0x900)==m.writes.end());
 // A foreign replacement racing our scan is preserved by expected-value CAS.
 m=fixture();r=inspect(m);m.put(0x900,Foreign);auto raced=cw::reconcile(r,true,A,B,HA,HB,[&](uint32_t p,uint32_t a,uint32_t b){return m.replace(p,a,b);});check(raced.raced==1&&raced.active==2&&m.get(0x900)==Foreign);
 m=fixture();m.put(0x214,513);check(!inspect(m).valid&&reconcile(m,true).changed==0);
 m=fixture();m.put(0x514,65);check(!inspect(m).valid&&reconcile(m,true).changed==0);
 m=fixture();m.put(0x518,0x910);check(!inspect(m).valid);
 m=fixture();m.put(0x204,0xfffffff8);check(!inspect(m).valid);
 m=fixture();m.text(0x740,"OtherMessage");check(!inspect(m).valid);
 std::cout<<"Passed: external dispatcher preserved, exact native callback selection, split-screen vectors, idempotence, realloc restore, foreign CAS races and bounded registry reads\n";
}
