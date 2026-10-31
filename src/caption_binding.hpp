#pragma once
#include "source_entity_contract.hpp"
namespace dango::captionwire {
struct Slot {uint32_t address=0,value=0;bool direct=false,owned=false;};
struct Registry {bool valid=false;int captionId=-1,directId=-1;unsigned count=0;std::vector<Slot> slots;std::vector<uint32_t> foreign;std::string status;};
inline bool span(uint32_t p,uint64_t n){return p&&uint64_t(p)+n<=0x100000000ull;}
// UserMessage has {size,name,hooks[2]}; each CUtlVector is 20 bytes. Only
// already-registered, exactly profiled native callbacks may be replaced.
template<class Reader> Registry inspect(Reader& reader,uint32_t managerPointer,uint32_t nativeCaption,uint32_t nativeDirect,uint32_t hookCaption,uint32_t hookDirect){
 Registry out;auto fail=[&](const char* s){out.status=s;out.slots.clear();return out;};uint32_t manager=0,nodes=0;int32_t count=0;
 if(!entitywire::load(reader,managerPointer,manager)||!span(manager,24)||!entitywire::load(reader,manager+4,nodes)||!entitywire::load(reader,manager+20,count)||count<1||count>512||!span(nodes,uint64_t(count)*24)||nodes%4)return fail("Caption registry unavailable");out.count=unsigned(count);
 for(int i=0;i<count;++i){uint32_t object=0,name=0;if(!entitywire::load(reader,nodes+uint32_t(i)*24+20,object)||!span(object,48)||!entitywire::load(reader,object+4,name))continue;auto text=entitywire::text(reader,name,128);bool direct=text=="CloseCaptionDirect";if(!direct&&text!="CloseCaption")continue;
 int& id=direct?out.directId:out.captionId;if(id>=0)return fail("Duplicate caption message names");id=i;
 for(unsigned player=0;player<2;++player){auto vector=object+8+player*20;uint32_t data=0,cached=0;int32_t allocated=0,size=0;
 if(!entitywire::load(reader,vector,data)||!entitywire::load(reader,vector+4,allocated)||!entitywire::load(reader,vector+12,size)||!entitywire::load(reader,vector+16,cached)||size<0||size>64||allocated<size||allocated>256||(size&&(!span(data,uint64_t(size)*4)||data%4||cached!=data)))return fail("Unsupported caption callback vector");
 for(int k=0;k<size;++k){uint32_t target=0,address=data+uint32_t(k)*4;if(!entitywire::load(reader,address,target))return fail("Unreadable caption callback cell");auto native=direct?nativeDirect:nativeCaption,hook=direct?hookDirect:hookCaption;
 if(target==native||target==hook)out.slots.push_back({address,target,direct,target==hook});else if(out.foreign.size()<8)out.foreign.push_back(target);
 }} }
 if(out.captionId<0||out.directId<0)return fail("Caption names not registered");out.valid=true;out.status=out.slots.empty()?"Native caption callbacks not registered":"Caption callback registry verified";return out;
}
struct PatchResult {unsigned active=0,changed=0,raced=0;};
// Replace is an expected-value CAS. A fresh scan, rather than saved heap cells,
// is used for every reconciliation/restore so vector reallocations are safe.
template<class Replace> PatchResult reconcile(const Registry& registry,bool install,uint32_t nativeCaption,uint32_t nativeDirect,uint32_t hookCaption,uint32_t hookDirect,Replace replace){
 PatchResult out;if(!registry.valid)return out;for(const auto& slot:registry.slots){uint32_t wanted=install?(slot.direct?hookDirect:hookCaption):(slot.direct?nativeDirect:nativeCaption);
 if(slot.value==wanted){if(install)++out.active;continue;}if(replace(slot.address,slot.value,wanted)){++out.changed;if(install)++out.active;}else ++out.raced;}return out;
}
}
