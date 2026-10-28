// Windows/x86 only: exercises the factory probe without invoking any virtual slot.
#include "source_camera.hpp"
#include <stdexcept>
#include <iostream>
int probes=0;
void originalCallback(){}
void existingHook(){}
void* fakeFactory(const char* name,int*){++probes;return std::strcmp(name,"VClient016")==0?reinterpret_cast<void*>(1):nullptr;}
int main(){auto names=source_camera::inventory(fakeFactory,"VClient");if(names!="VClient016"||probes!=32)throw std::runtime_error("Invalid inventory");void* entries[39]{};auto base=reinterpret_cast<HMODULE>(uintptr_t(0x10000000));for(auto method:source_camera::EngineMethods)entries[method.slot]=reinterpret_cast<void*>(uintptr_t(0x10000000)+method.rva);
 if(!source_camera::matchesMethods(entries,base,source_camera::EngineMethods))return 1;entries[source_camera::WorldClipSlot]=nullptr;if(source_camera::matchesMethods(entries,base,source_camera::EngineMethods))return 1;
 auto localModule=GetModuleHandleW(nullptr);source_camera::MethodSlot localMethods[]={{0,uint32_t(reinterpret_cast<uintptr_t>(&originalCallback)-reinterpret_cast<uintptr_t>(localModule))}};void* hookedEntry[]={reinterpret_cast<void*>(&existingHook)};
 if(!source_camera::matchesMethods(hookedEntry,localModule,localMethods))return 1;
 hookedEntry[0]=nullptr;if(source_camera::matchesMethods(hookedEntry,localModule,localMethods))return 1;
 source_camera::reason=2;if(!std::strstr(source_camera::status(),"VClient016 unavailable"))return 1;std::cout<<"Passed: exact-name factory inventory without vtable dispatch\n";}
