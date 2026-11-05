#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include "sync/steam_transport.hpp"
#include "sync/runtime.hpp"
#include <cassert>
#include <iostream>
constexpr uint64_t id=76561197960265729ULL;
uint64_t __thiscall registerReturn(void*){return id;}
void* __thiscall storageReturn(void*,uint64_t* output){*output=id;return output;}
int main(){void* methods[3]{};struct Object{void** table;} object{methods};volatile uint32_t canary=0x12345678;
 for(int i=0;i<10000;++i){methods[2]=reinterpret_cast<void*>(&registerReturn);assert(dango::sync::steam::legacyUserId(&object)==id);methods[2]=reinterpret_cast<void*>(&storageReturn);assert(dango::sync::steam::legacyUserId(&object)==id);assert(canary==0x12345678);}
 dango::sync::Requests requests;dango::sync::Runtime runtime(requests,nullptr);dango::sync::Settings settings;dango::State state;int reads=0,http=0;
 for(int i=0;i<100;++i){assert(requests.push({2,{}}));auto status=runtime.tick(settings,state,1000+i,[&](){++reads;return state;},[&](const std::string&){++http;});assert(status.create.rfind("BLOCKED",0)==0&&status.identity==0&&status.network=="Disabled");}
 assert(reads==0&&http==0);
 std::cout<<"Passed: x86 SteamID register/hidden-storage return and balanced stack; message ABI prefix; blocked user probe has no Steam binding or LX activity\n";
}
