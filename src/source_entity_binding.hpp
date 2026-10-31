#pragma once
#include "source_entity_contract.hpp"
#include <cstdio>
#include <string>
#include <vector>
namespace dango::entitywire {
struct BindingCheck {
 bool object=false,vtable=false;uint32_t matched=0;std::vector<std::string> details;
 bool layout()const{return object&&vtable;}
 bool callable(uint32_t slots)const{return layout()&&(matched&slots)==slots;}
};
inline std::string bindingMismatch(const std::string& what,uint32_t expected,uint32_t actual){char text[100];std::snprintf(text,sizeof(text)," expected=0x%08x actual=0x%08x",expected,actual);return what+text;}
template<class Reader,size_t N> BindingCheck inspectBinding(Reader& reader,const std::string& label,uint32_t object,uint32_t module,uint32_t objectRva,uint32_t tableRva,const uint32_t(&methods)[N]){
 static_assert(N<32,"Binding mask width");BindingCheck check;
 check.object=object==module+objectRva;
 if(!check.object){check.details.push_back(bindingMismatch(label+" object",module+objectRva,object));return check;}
 uint32_t table=0;if(!load(reader,object,table)){check.details.push_back(label+" object vtable unreadable");return check;}
 check.vtable=table==module+tableRva;
 if(!check.vtable){check.details.push_back(bindingMismatch(label+" vtable",module+tableRva,table));return check;}
 for(unsigned slot=0;slot<N;++slot){uint32_t method=0;if(load(reader,table+slot*4,method)&&method==module+methods[slot])check.matched|=uint32_t(1)<<slot;else check.details.push_back(bindingMismatch(label+" slot "+std::to_string(slot),module+methods[slot],method));}
 return check;
}
}
