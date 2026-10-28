#pragma once
#include <windows.h>
#include <memory>
#include <string>
#include <atomic>
#include <cstring>
#include <cstdio>
#include "world_camera.hpp"
#include "source_build_profile.hpp"
namespace source_camera {
using namespace dango;
inline HMODULE clientModule=nullptr,engineModule=nullptr;
inline void* client=nullptr;inline void* engine=nullptr;inline void** table=nullptr;
inline uint32_t epoch=1;inline bool hooked=false;
inline std::atomic<int> reason{0};inline std::atomic<unsigned> captureState{0},seenCaptureStates{0};inline std::atomic<uint64_t> capturedFrames{0};
inline const char* captureStatus(unsigned state){switch(state){case 1:return "Source not in game";case 2:return "Source loading image";case 3:return "matrix pointer unreadable";case 4:return "scene matrix validation failed";case 5:return "snapshot captured (check age/viewport)";case 6:return "level shutdown";default:return "waiting for View_Render callback";}}
inline const char* status(){switch(reason.load()){case 1:return "CreateInterface missing";case 2:return "VClient016 unavailable - see interface inventory log";case 3:return "VEngineClient013 unavailable - see interface inventory log";case 4:return "client and engine interfaces unavailable - see log";case 5:return "interface memory/module mismatch - see log";case 6:return captureStatus(captureState.load());case 7:return "game build or callback validation failed - see log";case 8:return "could not install view hook - see log";default:return "waiting for game modules";}}
template<class Factory> std::string inventory(Factory factory,const char* prefix){
 std::string names;for(int i=1;i<=32;++i){char name[64]{};std::snprintf(name,sizeof(name),"%s%03d",prefix,i);if(factory(name,nullptr)){if(!names.empty())names+=",";names+=name;}}return names.empty()?"none (001..032 queried)":names;
}

inline std::shared_ptr<const GameCameraFrame> snapshot=std::make_shared<GameCameraFrame>();
inline void (*logger)(const std::string&)=nullptr;
inline void log(const std::string& s){if(logger)logger(s);}
inline bool readable(const void* p,size_t n){MEMORY_BASIC_INFORMATION i{};return p&&VirtualQuery(p,&i,sizeof(i))&&i.State==MEM_COMMIT&&!(i.Protect&(PAGE_GUARD|PAGE_NOACCESS))&&reinterpret_cast<uintptr_t>(p)+n<=reinterpret_cast<uintptr_t>(i.BaseAddress)+i.RegionSize;}
inline bool owns(const void* p,HMODULE m){HMODULE owner=nullptr;return p&&GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,reinterpret_cast<const wchar_t*>(p),&owner)&&owner==m;}
using RenderFn=void(__thiscall*)(void*,void*);using EventFn=void(__thiscall*)(void*);
inline RenderFn originalRender=nullptr;inline EventFn originalShutdown=nullptr,originalLevelShutdown=nullptr;
inline void capture(bool active){
 GameCameraFrame f;f.epoch=epoch;f.tick=GetTickCount64();unsigned state=6;
 if(active&&engine){++capturedFrames;auto vt=*reinterpret_cast<void***>(engine);using Bool=bool(__thiscall*)(void*);using Size=void(__thiscall*)(void*,int&,int&);using Matrix=const Matrix4&(__thiscall*)(void*);
 if(!reinterpret_cast<Bool>(vt[InGameSlot])(engine))state=1;
 else if(reinterpret_cast<Bool>(vt[LoadingSlot])(engine))state=2;
 else{int w=0,h=0;reinterpret_cast<Size>(vt[ScreenSizeSlot])(engine,w,h);f.width=w>0?uint32_t(w):0;f.height=h>0?uint32_t(h):0;
 const auto& clip=reinterpret_cast<Matrix>(vt[WorldClipSlot])(engine);const auto& view=reinterpret_cast<Matrix>(vt[WorldViewSlot])(engine);
 if(readable(&clip,64)&&readable(&view,64)){std::memcpy(f.worldToClip.data(),&clip,64);std::memcpy(f.worldToView.data(),&view,64);CameraPose pose;state=cameraPose(f,pose)?5:4;if(state==5)f.active=1;}else state=3;}}
 captureState=state;unsigned bit=1u<<state;if(!(seenCaptureStates.fetch_or(bit)&bit))log(std::string("Source capture: ")+captureStatus(state));
 std::atomic_store(&snapshot,std::make_shared<const GameCameraFrame>(f));
}
inline void __fastcall renderHook(void* self,void*,void* rect){static thread_local int depth=0;++depth;originalRender(self,rect);if(depth==1)capture(true);--depth;}
inline void __fastcall shutdownHook(void* self,void*){++epoch;capture(false);originalShutdown(self);}
inline void __fastcall levelHook(void* self,void*){++epoch;capture(false);originalLevelShutdown(self);}
inline bool replace(size_t slot,void* expected,void* value){DWORD old=0;if(!VirtualProtect(table+slot,sizeof(void*),PAGE_READWRITE,&old))return false;void* actual=InterlockedCompareExchangePointer(table+slot,value,expected);DWORD ignored=0;VirtualProtect(table+slot,sizeof(void*),old,&ignored);return actual==expected;}
inline bool executable(const void* address){MEMORY_BASIC_INFORMATION info{};if(!address||!VirtualQuery(address,&info,sizeof(info))||info.State!=MEM_COMMIT||(info.Protect&(PAGE_GUARD|PAGE_NOACCESS)))return false;return (info.Protect&(PAGE_EXECUTE|PAGE_EXECUTE_READ|PAGE_EXECUTE_READWRITE|PAGE_EXECUTE_WRITECOPY))!=0;}
inline std::string targetOwner(const void* address){HMODULE owner=nullptr;if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,reinterpret_cast<const wchar_t*>(address),&owner))return "private-or-unmapped";wchar_t path[32768]{};if(!GetModuleFileNameW(owner,path,32768))return "unknown-module";const wchar_t* name=path;for(const wchar_t* p=path;*p;++p)if(*p==L'\\'||*p==L'/')name=p+1;char utf8[512]{};WideCharToMultiByte(CP_UTF8,0,name,-1,utf8,sizeof(utf8),nullptr,nullptr);return utf8;}
inline bool matchesImage(HMODULE module,uint32_t timestamp,uint32_t bytes,const char* name="module"){
 if(!readable(module,sizeof(IMAGE_DOS_HEADER))){log(std::string("Build check ")+name+": DOS header unreadable");return false;}
 auto dos=reinterpret_cast<const IMAGE_DOS_HEADER*>(module);if(dos->e_magic!=IMAGE_DOS_SIGNATURE||dos->e_lfanew<0||dos->e_lfanew>1024*1024){log(std::string("Build check ")+name+": invalid DOS header");return false;}
 auto nt=reinterpret_cast<const IMAGE_NT_HEADERS*>(reinterpret_cast<const char*>(module)+dos->e_lfanew);
 if(!readable(nt,sizeof(*nt))||nt->Signature!=IMAGE_NT_SIGNATURE){log(std::string("Build check ")+name+": invalid/unreadable PE header");return false;}
 bool ok=nt->FileHeader.Machine==IMAGE_FILE_MACHINE_I386&&nt->FileHeader.TimeDateStamp==timestamp&&nt->OptionalHeader.SizeOfImage==bytes;
 char detail[320]{};std::snprintf(detail,sizeof(detail),"Build check %s: %s machine=0x%x timestamp=%lu expected=%lu image_size=0x%lx expected=0x%lx",name,ok?"match":"MISMATCH",nt->FileHeader.Machine,static_cast<unsigned long>(nt->FileHeader.TimeDateStamp),static_cast<unsigned long>(timestamp),static_cast<unsigned long>(nt->OptionalHeader.SizeOfImage),static_cast<unsigned long>(bytes));log(detail);return ok;
}
template<size_t N> bool matchesMethods(void** vtable,HMODULE module,const MethodSlot (&methods)[N],const char* name="interface"){
 bool valid=true;for(auto& method:methods){auto actual=vtable[method.slot];auto expected=reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(module)+method.rva);if(actual==expected)continue;
 char detail[320]{};std::snprintf(detail,sizeof(detail),"Method check %s slot=%u expected_rva=0x%lx actual=0x%lx owner=%s",name,method.slot,static_cast<unsigned long>(method.rva),static_cast<unsigned long>(reinterpret_cast<uintptr_t>(actual)),targetOwner(actual).c_str());log(detail);
 if(!executable(actual)||!executable(expected)){valid=false;log("Method check rejected: current or original game entry is not executable");}
 else log("Method check: chaining existing executable interface callback; preserving current target as original");
 }return valid;
}
inline void connect(){
 if(hooked||!clientModule||!engineModule)return;using Factory=void*(__cdecl*)(const char*,int*);
 auto cf=reinterpret_cast<Factory>(GetProcAddress(clientModule,"CreateInterface")),ef=reinterpret_cast<Factory>(GetProcAddress(engineModule,"CreateInterface"));if(!cf||!ef){reason=1;log("Source CreateInterface missing: client="+std::string(cf?"present":"missing")+" engine="+(ef?"present":"missing"));return;}
 // The uploaded Steam build differs from the public SDK despite the same engine interface name.
 client=cf("VClient016",nullptr);engine=ef("VEngineClient013",nullptr);
 static int previousAvailability=-1;int availability=(client?1:0)|(engine?2:0);
 if(availability!=previousAvailability){previousAvailability=availability;
 log("Camera query: VClient016="+std::string(client?"found":"missing")+" VEngineClient013="+(engine?"found":"missing"));
 log("Client factory inventory: "+inventory(cf,"VClient"));log("Engine factory inventory: "+inventory(ef,"VEngineClient"));
 log("Inventory is names only. Unverified interface versions will NOT be called or hooked.");}
 if(!client||!engine){reason=!client?(!engine?4:2):3;return;}reason=5;
 if(!readable(client,sizeof(void*))||!readable(engine,sizeof(void*)))return;table=*reinterpret_cast<void***>(client);auto et=*reinterpret_cast<void***>(engine);
 if(!readable(table,(RenderSlot+1)*sizeof(void*))||!readable(et,(WorldViewSlot+1)*sizeof(void*)))return;
 bool clientBuild=matchesImage(clientModule,ClientTimestamp,ClientImageSize,"client.dll");
 bool engineBuild=matchesImage(engineModule,EngineTimestamp,EngineImageSize,"engine.dll");
 if(!clientBuild||!engineBuild){reason=7;log("Camera refused: game build identity differs from uploaded DLL pair; method dispatch not attempted");return;}
 bool clientEntries=matchesMethods(table,clientModule,ClientMethods,"VClient016");
 bool engineEntries=matchesMethods(et,engineModule,EngineMethods,"VEngineClient013");
 if(!clientEntries||!engineEntries){reason=7;log("Camera refused: interface callback is not a callable executable entry; no hooks installed");return;}

 originalRender=reinterpret_cast<RenderFn>(table[RenderSlot]);originalShutdown=reinterpret_cast<EventFn>(table[ShutdownSlot]);originalLevelShutdown=reinterpret_cast<EventFn>(table[LevelShutdownSlot]);
 if(!replace(ShutdownSlot,reinterpret_cast<void*>(originalShutdown),reinterpret_cast<void*>(&shutdownHook))){reason=8;log("Source Shutdown hook install failed");return;}
 if(!replace(LevelShutdownSlot,reinterpret_cast<void*>(originalLevelShutdown),reinterpret_cast<void*>(&levelHook))){replace(ShutdownSlot,reinterpret_cast<void*>(&shutdownHook),reinterpret_cast<void*>(originalShutdown));reason=8;log("Source LevelShutdown hook install failed");return;}
 if(!replace(RenderSlot,reinterpret_cast<void*>(originalRender),reinterpret_cast<void*>(&renderHook))){replace(ShutdownSlot,reinterpret_cast<void*>(&shutdownHook),reinterpret_cast<void*>(originalShutdown));replace(LevelShutdownSlot,reinterpret_cast<void*>(&levelHook),reinterpret_cast<void*>(originalLevelShutdown));reason=8;log("Source View_Render hook install failed");return;}
 hooked=true;reason=6;log("Source camera installed: verified VClient016 View_Render[25], VEngineClient013 matrices[37,38] (experimental)");
}
inline void stop(){if(hooked&&readable(table,(RenderSlot+1)*sizeof(void*))){replace(RenderSlot,reinterpret_cast<void*>(&renderHook),reinterpret_cast<void*>(originalRender));replace(LevelShutdownSlot,reinterpret_cast<void*>(&levelHook),reinterpret_cast<void*>(originalLevelShutdown));replace(ShutdownSlot,reinterpret_cast<void*>(&shutdownHook),reinterpret_cast<void*>(originalShutdown));}hooked=false;}
}
