#pragma once
#include <windows.h>
#include <memory>
#include <atomic>
#include <cstring>
#include "world_camera.hpp"
namespace source_camera {
using namespace dango;
inline HMODULE clientModule=nullptr,engineModule=nullptr;
inline void* client=nullptr;inline void* engine=nullptr;inline void** table=nullptr;
inline uint32_t epoch=1;inline bool hooked=false;
inline std::shared_ptr<const GameCameraFrame> snapshot=std::make_shared<GameCameraFrame>();
inline void (*logger)(const std::string&)=nullptr;
inline void log(const std::string& s){if(logger)logger(s);}
inline bool readable(const void* p,size_t n){MEMORY_BASIC_INFORMATION i{};return p&&VirtualQuery(p,&i,sizeof(i))&&i.State==MEM_COMMIT&&!(i.Protect&(PAGE_GUARD|PAGE_NOACCESS))&&reinterpret_cast<uintptr_t>(p)+n<=reinterpret_cast<uintptr_t>(i.BaseAddress)+i.RegionSize;}
inline bool owns(const void* p,HMODULE m){HMODULE owner=nullptr;return p&&GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,reinterpret_cast<const wchar_t*>(p),&owner)&&owner==m;}
using RenderFn=void(__thiscall*)(void*,void*);using EventFn=void(__thiscall*)(void*);
inline RenderFn originalRender=nullptr;inline EventFn originalShutdown=nullptr,originalLevelShutdown=nullptr;
inline void capture(bool active){
 GameCameraFrame f;f.epoch=epoch;f.tick=GetTickCount64();
 if(active&&engine){auto vt=*reinterpret_cast<void***>(engine);using Bool=bool(__thiscall*)(void*);using Size=void(__thiscall*)(void*,int&,int&);using Matrix=const Matrix4&(__thiscall*)(void*);
 if(reinterpret_cast<Bool>(vt[26])(engine)&&!reinterpret_cast<Bool>(vt[28])(engine)){int w=0,h=0;reinterpret_cast<Size>(vt[5])(engine,w,h);f.width=w>0?uint32_t(w):0;f.height=h>0?uint32_t(h):0;
 const auto& clip=reinterpret_cast<Matrix>(vt[36])(engine);const auto& view=reinterpret_cast<Matrix>(vt[37])(engine);
 if(readable(&clip,64)&&readable(&view,64)){std::memcpy(f.worldToClip.data(),&clip,64);std::memcpy(f.worldToView.data(),&view,64);CameraPose pose;if(cameraPose(f,pose))f.active=1;}}}
 std::atomic_store(&snapshot,std::make_shared<const GameCameraFrame>(f));
}
inline void __fastcall renderHook(void* self,void*,void* rect){static thread_local int depth=0;++depth;originalRender(self,rect);if(depth==1)capture(true);--depth;}
inline void __fastcall shutdownHook(void* self,void*){++epoch;capture(false);originalShutdown(self);}
inline void __fastcall levelHook(void* self,void*){++epoch;capture(false);originalLevelShutdown(self);}
inline bool replace(size_t slot,void* expected,void* value){DWORD old=0;if(!VirtualProtect(table+slot,sizeof(void*),PAGE_READWRITE,&old))return false;void* actual=InterlockedCompareExchangePointer(table+slot,value,expected);DWORD ignored=0;VirtualProtect(table+slot,sizeof(void*),old,&ignored);return actual==expected;}
inline void connect(){
 if(hooked||!clientModule||!engineModule)return;using Factory=void*(__cdecl*)(const char*,int*);
 auto cf=reinterpret_cast<Factory>(GetProcAddress(clientModule,"CreateInterface")),ef=reinterpret_cast<Factory>(GetProcAddress(engineModule,"CreateInterface"));if(!cf||!ef){log("Source CreateInterface missing");return;}
 // Exact public SDK contract; do not guess layouts for other interface versions.
 client=cf("VClient015",nullptr);engine=ef("VEngineClient013",nullptr);
 if(!client||!engine){log("Camera disabled: require VClient015 / VEngineClient013; no alternate slots guessed");return;}
 if(!readable(client,sizeof(void*))||!readable(engine,sizeof(void*)))return;table=*reinterpret_cast<void***>(client);auto et=*reinterpret_cast<void***>(engine);
 if(!readable(table,24*sizeof(void*))||!readable(et,38*sizeof(void*)))return;
 for(auto index:{5,26,28,36,37})if(!owns(et[index],engineModule)){log("Camera disabled: engine method module mismatch");return;}
 for(auto index:{2,5,23})if(!owns(table[index],clientModule)){log("Camera disabled: client method module mismatch or existing hook");return;}
 originalRender=reinterpret_cast<RenderFn>(table[23]);originalShutdown=reinterpret_cast<EventFn>(table[2]);originalLevelShutdown=reinterpret_cast<EventFn>(table[5]);
 if(!replace(2,reinterpret_cast<void*>(originalShutdown),reinterpret_cast<void*>(&shutdownHook)))return;
 if(!replace(5,reinterpret_cast<void*>(originalLevelShutdown),reinterpret_cast<void*>(&levelHook))){replace(2,reinterpret_cast<void*>(&shutdownHook),reinterpret_cast<void*>(originalShutdown));return;}
 if(!replace(23,reinterpret_cast<void*>(originalRender),reinterpret_cast<void*>(&renderHook))){replace(2,reinterpret_cast<void*>(&shutdownHook),reinterpret_cast<void*>(originalShutdown));replace(5,reinterpret_cast<void*>(&levelHook),reinterpret_cast<void*>(originalLevelShutdown));return;}
 hooked=true;log("Source camera installed: VClient015 View_Render[23], VEngineClient013 matrices[36,37] (experimental)");
}
inline void stop(){if(hooked&&readable(table,24*sizeof(void*))){replace(23,reinterpret_cast<void*>(&renderHook),reinterpret_cast<void*>(originalRender));replace(5,reinterpret_cast<void*>(&levelHook),reinterpret_cast<void*>(originalLevelShutdown));replace(2,reinterpret_cast<void*>(&shutdownHook),reinterpret_cast<void*>(originalShutdown));}hooked=false;}
}
