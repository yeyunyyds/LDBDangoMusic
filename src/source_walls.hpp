#pragma once
#include <windows.h>
#include <atomic>
#include <memory>
#include <vector>
#include "source_camera.hpp"
#include "source_trace_contract.hpp"
namespace source_walls {
using namespace dango;
struct Item {wall::Panel panel;unsigned id=0;bool visible=false;};
struct Frame {uint32_t epoch=0;uint64_t tick=0,traces=0;std::vector<Item> panels;std::string message="Wall experiment is off";};
inline std::shared_ptr<const Frame> snapshot=std::make_shared<Frame>();
inline std::atomic<bool> enabled{false},liveLyrics{false},clearRequested{false},resetRequested{false};
inline std::atomic<uint64_t> placeAt{0};inline std::atomic<int> sizeChoice{1};
inline void* traceObject=nullptr;inline bool wasEnabled=false,faulted=false,held=false;inline Frame state;inline unsigned sequence=0;inline uint64_t nextVisibility=0;
inline void log(const std::string& s){source_camera::log("Wall experiment: "+s);}
inline void publish(){std::atomic_store(&snapshot,std::make_shared<const Frame>(state));}
inline void status(const std::string& s){if(state.message!=s){state.message=s;log(s);}}
inline bool connect(){
 using namespace source_camera;
 if(!hooked||!engineModule)return false;
 if(!matchesImage(engineModule,EngineTimestamp,EngineImageSize,"wall-trace engine.dll"))return false;
 using Factory=void*(__cdecl*)(const char*,int*);auto factory=reinterpret_cast<Factory>(GetProcAddress(engineModule,"CreateInterface"));if(!factory)return false;
 void* object=factory("EngineTraceClient003",nullptr);auto base=reinterpret_cast<uintptr_t>(engineModule);
 if(object!=reinterpret_cast<void*>(base+0x42fa5c)||!readable(object,4))return false;auto table=*reinterpret_cast<void***>(object);
 if(table!=reinterpret_cast<void**>(base+0x387474)||!readable(table,32))return false;
 constexpr uint32_t methods[]={0x177630,0x177600,0x178950,0x177990,0x179ff0,0x179760,0x177d60,0x177da0};
 for(unsigned i=0;i<8;++i)if(table[i]!=reinterpret_cast<void*>(base+methods[i])||!executable(table[i]))return false;
 const uint8_t entry[]={0x53,0x8b,0xdc,0x83,0xec,0x08,0x83,0xe4,0xf0};
 const uint8_t clear[]={0x55,0x8b,0xec,0x56,0x8b,0x75,0x08,0x6a,0x54};
 const uint8_t tail[]={0xc2,0x10,0x00};
 if(!readable(table[5],sizeof(entry))||std::memcmp(table[5],entry,sizeof(entry))||!readable(reinterpret_cast<void*>(base+0x14ba10),sizeof(clear))||std::memcmp(reinterpret_cast<void*>(base+0x14ba10),clear,sizeof(clear))||!readable(reinterpret_cast<void*>(base+0x179e2b),3)||std::memcmp(reinterpret_cast<void*>(base+0x179e2b),tail,3))return false;
 traceObject=object;log("TraceRay bound to static profile; result size=84 Ray size=80; game-thread runtime validation pending");return true;
}
// Explicit MSVC x86 filter table: ITraceFilter has exactly two methods and no destructor.
inline bool __fastcall rejectEntity(void*,void*,void*,int){return false;}
inline int __fastcall worldOnly(void*,void*){return 1;}
inline void* filterMethods[]={reinterpret_cast<void*>(&rejectEntity),reinterpret_cast<void*>(&worldOnly)};
struct Filter {void** methods=filterMethods;};
inline bool cast(Vec3 start,Vec3 end,trace::Result& output){
 if(faulted||!traceObject||!wall::finite(start)||!wall::finite(end))return false;
 auto ray=trace::ray(start,end);trace::GuardedResult guarded;Filter filter;
 using Function=void(__thiscall*)(void*,const trace::Ray&,unsigned,void*,trace::Result*);
 auto table=*reinterpret_cast<void***>(traceObject);reinterpret_cast<Function>(table[5])(traceObject,ray,0x400b,&filter,&guarded.result);++state.traces;
 if(!guarded.intact()||!trace::valid(guarded.result,start,end)){faulted=true;status("Trace result validation failed; toggle experiment off/on to retry");return false;}
 output=guarded.result;if(state.traces==1)log("First TraceRay result passed validation; fraction="+std::to_string(output.fraction));return true;
}
// Only invoked by the Source View_Render callback after its original function.
// Menu and Present callbacks merely publish requests / read immutable snapshots.
inline void onFrame(const GameCameraFrame& camera){
 bool active=enabled.load();if(!active){if(wasEnabled){state={};traceObject=nullptr;faulted=false;placeAt=0;clearRequested=false;status("Wall experiment is off");publish();}wasEnabled=false;held=false;return;}
 bool reset=resetRequested.exchange(false);if(!wasEnabled||reset){wasEnabled=true;faulted=false;traceObject=nullptr;state={};nextVisibility=0;}
 state.tick=camera.tick;
 if(state.epoch!=camera.epoch){state.panels.clear();state.epoch=camera.epoch;nextVisibility=0;}
 if(clearRequested.exchange(false)){state.panels.clear();placeAt=0;status("All wall panels cleared");}
 CameraPose pose;if(!camera.active||!cameraPose(camera,pose)){state.panels.clear();placeAt=0;status("Waiting for an active Source camera");publish();return;}
 if(faulted){publish();return;}
 if(!traceObject){if(!connect()){faulted=true;status("Trace interface/profile mismatch; no TraceRay calls made");publish();return;}status("Ready: aim at a flat wall and press F8");}
 else if(state.message=="Waiting for an active Source camera")status("Ready: aim at a flat wall and press F8");
 bool key=(GetAsyncKeyState(VK_F8)&0x8000)!=0;if(key&&!held)placeAt=camera.tick;held=key;
 uint64_t due=placeAt.load();if(due&&camera.tick>=due&&placeAt.compare_exchange_strong(due,0)){
 trace::Result hit;
 bool queried=cast(pose.eye,pose.eye+pose.forward*1024,hit);if(queried){char details[400];std::snprintf(details,sizeof(details),"Placement trace fraction=%.4f end=(%.2f,%.2f,%.2f) normal=(%.3f,%.3f,%.3f) surface_flags=0x%x startsolid=%u allsolid=%u",hit.fraction,hit.end.x,hit.end.y,hit.end.z,hit.plane.normal.x,hit.plane.normal.y,hit.plane.normal.z,hit.surface.flags,hit.startSolid,hit.allSolid);log(details);}
 if(queried&&trace::wallHit(hit)){
 const float widths[]={80,120,180},heights[]={28,40,60};int size=std::clamp(sizeChoice.load(),0,2);wall::Panel panel;
 if(wall::makePanel(hit.end,hit.plane.normal,widths[size],heights[size],camera.epoch,panel)&&trace::supported(panel,cast)){
 if(state.panels.size()>=4)state.panels.erase(state.panels.begin());state.panels.push_back({panel,++sequence,false});status("Panel placed; axes are fixed to the wall");log("Panel id="+std::to_string(sequence)+" epoch="+std::to_string(panel.epoch)+" width="+std::to_string(panel.width)+" height="+std::to_string(panel.height));nextVisibility=0;
 }else if(!faulted)status("Wall is not a supported flat 3x3 patch; try a smaller panel");
 }else if(!faulted)status("No suitable wall hit (sky/glass/floor/solid start rejected)");
 }
 if(!faulted&&camera.tick>=nextVisibility){nextVisibility=camera.tick+100;for(auto& item:state.panels){item.visible=trace::visible(item.panel,pose.eye,cast);if(faulted)break;}}
 if(faulted)state.panels.clear();publish();
}
inline void stop(){enabled=false;placeAt=0;clearRequested=true;}
}
