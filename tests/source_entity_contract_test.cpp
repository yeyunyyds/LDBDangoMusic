#include "source_entity_contract.hpp"
#include <cstring>
#include <iostream>
#include <stdexcept>
using namespace dango;namespace wire=dango::entitywire;
void check(bool v){if(!v)throw std::runtime_error("Entity inventory contract regression");}
struct Memory {
 std::vector<uint8_t> data=std::vector<uint8_t>(65536);uint32_t next=256;
 bool read(uint32_t p,void* out,size_t n){if(uint64_t(p)+n>data.size())return false;std::memcpy(out,data.data()+p,n);return true;}
 template<class T>uint32_t add(const T& t){uint32_t at=next;next+=uint32_t((sizeof(t)+3)&~size_t(3));std::memcpy(data.data()+at,&t,sizeof(t));return at;}
 uint32_t str(const std::string& s){auto at=next;next+=uint32_t(s.size()+1);std::memcpy(data.data()+at,s.c_str(),s.size()+1);return at;}
 uint32_t props(const std::vector<wire::Prop>& p){next=(next+3)&~3u;auto at=next;next+=uint32_t(p.size()*sizeof(wire::Prop));std::memcpy(data.data()+at,p.data(),p.size()*sizeof(wire::Prop));return at;}
};
wire::Prop prop(Memory& m,const char* name,int type,int offset){wire::Prop p;p.name=m.str(name);p.type=type;p.offset=offset;return p;}
int main(){Memory m;
 check(wire::handle(17,0xabc)==std::optional<uint32_t>(0xabc011));check(!wire::handle(4096,1)&&!wire::handle(4095,0xfffff));check(wire::handle(3,4)!=wire::handle(3,5));
 auto origin=prop(m,"m_vecOrigin",2,32),model=prop(m,"m_nModelIndex",0,64),owner=prop(m,"m_hOwnerEntity",0,80);
 wire::Table base;base.count=3;base.props=m.props({origin,model,owner});base.name=m.str("DT_BaseEntity");auto baseAddress=m.add(base);
 auto parent=prop(m,"baseclass",6,16);parent.table=baseAddress;auto weapon=prop(m,"m_hActiveWeapon",0,128);
 wire::Table derived;derived.count=2;derived.props=m.props({parent,weapon});derived.name=m.str("DT_BasePlayer");auto root=m.add(derived);
 wire::Fields found;check(wire::fields(m,root,found));check(found.at("m_vecOrigin").offset==48&&found.at("m_nModelIndex").offset==80&&found.at("m_hActiveWeapon").offset==128);
 auto previous=found;auto cycle=parent;cycle.table=root;std::memcpy(m.data.data()+derived.props,&cycle,sizeof(cycle));check(!wire::fields(m,root,found)&&found.size()==previous.size());std::memcpy(m.data.data()+derived.props,&parent,sizeof(parent));
 auto invalid=derived;invalid.count=3000;auto bad=m.add(invalid);check(!wire::fields(m,bad,found));check(!wire::fields(m,65530,found));
 auto code=m.next;m.next+=16;std::memcpy(m.data.data()+code,"\xb8\x00\x10\x00\x00\xc3",6);check(wire::addressGetter(m,code,0)==4096);
 std::memcpy(m.data.data()+code,"\x8d\x41\xf8\xc3",4);check(wire::addressGetter(m,code,1000)==992);
 std::memcpy(m.data.data()+code,"\x8d\x81\x00\x01\x00\x00\xc3",7);check(wire::addressGetter(m,code,1000)==1256);
 std::memcpy(m.data.data()+code,"\x8a\x41\x04\xc3",4);m.data[1004]=1;check(wire::byteGetter(m,code,1000)==true);m.data[1004]=0;check(wire::byteGetter(m,code,1000)==false);m.data[1004]=2;check(!wire::byteGetter(m,code,1000));
 std::memset(m.data.data()+code,0xcc,8);check(!wire::addressGetter(m,code,1000));check(wire::text(m,base.name)=="DT_BaseEntity");
 std::cout<<"Passed: full handle serials, bounded RecvTable base traversal, offsets/types, cycle/read rejection and no-dispatch getter decoding\n";
}
