#pragma once
#include <windows.h>
#include <atomic>
#include <memory>
#include <vector>
#include "source_camera.hpp"
#include "wall_scene.hpp"
#include "lyrics_settings.hpp"
namespace source_walls {
using namespace dango;
using Item=wallscene::Item;
struct Frame {uint32_t epoch=0;uint64_t tick=0,traces=0;std::vector<Item> panels;std::string message="Wall experiment is off";};
inline std::shared_ptr<const Frame> snapshot=std::make_shared<Frame>();
inline std::atomic<bool> enabled{false},clearRequested{false},resetRequested{false};
inline std::atomic<uint64_t> placeAt{0};
inline std::shared_ptr<const wallscene::Options> options=std::make_shared<wallscene::Options>();
inline std::shared_ptr<const wallscene::Ticket> ticket=std::make_shared<wallscene::Ticket>();
inline wallscene::Scene scene;inline uint64_t nextSearch=0,searchToken=0;inline unsigned searchAttempt=0,visibilityCursor=0;
inline void configure(const Config& c){
 wallscene::Options o;o.automatic=c.wallAutomatic;o.live=c.wallLiveLyrics;o.size=c.wallSize;o.capacity=c.wallCapacity;o.lingerMs=c.wallLingerMs;o.fadeMs=c.wallFadeMs;o.range=c.wallRange;o.spacing=c.wallSpacing;
 auto old=std::atomic_load(&options);bool reset=old->automatic!=o.automatic||old->live!=o.live;
 std::atomic_store(&options,std::make_shared<const wallscene::Options>(o));
 bool active=c.wallEnabled&&c.enabled;bool previous=enabled.exchange(active);if((active&&!previous)||reset)resetRequested=true;
}
inline void submit(const wallscene::Ticket& t){std::atomic_store(&ticket,std::make_shared<const wallscene::Ticket>(t));}
inline void* traceObject=nullptr;inline bool wasEnabled=false,faulted=false,held=false;inline Frame state;inline uint64_t nextVisibility=0;
inline void log(const std::string& s){source_camera::log("Wall experiment: "+s);}
inline void publish(){state.panels=scene.items;std::atomic_store(&snapshot,std::make_shared<const Frame>(state));}
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
 bool active=enabled.load();if(!active){if(wasEnabled){state={};scene.reset();traceObject=nullptr;faulted=false;placeAt=0;clearRequested=false;status("Wall lyrics are off");publish();}wasEnabled=false;held=false;return;}
 bool reset=resetRequested.exchange(false);if(!wasEnabled||reset){wasEnabled=true;faulted=false;traceObject=nullptr;state={};scene.reset();nextVisibility=nextSearch=searchToken=0;searchAttempt=visibilityCursor=0;}
 auto settings=std::atomic_load(&options);const auto& o=*settings;auto request=std::atomic_load(&ticket);const auto& t=*request;
 state.tick=camera.tick;
 if(state.epoch!=camera.epoch){scene.reset();state.epoch=camera.epoch;nextVisibility=nextSearch=0;}
 scene.observe(t,camera.tick,camera.epoch,o);
 bool cleared=clearRequested.exchange(false);if(cleared){scene.clear(t);placeAt=0;status("All wall panels cleared; waiting for the next sentence");}
 CameraPose pose;if(!camera.active||!cameraPose(camera,pose)){scene.reset();placeAt=0;status("Waiting for an active Source camera");publish();return;}
 if(faulted){publish();return;}
 if(!traceObject){if(!connect()){faulted=true;status("Trace interface/profile mismatch; no TraceRay calls made");publish();return;}status(o.automatic?"Automatic wall search ready":"Manual mode: aim at a flat wall and press F8");}
 bool key=(GetAsyncKeyState(VK_F8)&0x8000)!=0;if(key&&!held)placeAt=camera.tick;held=key;
 uint64_t due=placeAt.load();if(due&&camera.tick>=due&&placeAt.compare_exchange_strong(due,0)){
 trace::Result hit;if(cast(pose.eye,pose.eye+pose.forward*1024,hit)&&trace::wallHit(hit)){
 const float widths[]={80,120,180},heights[]={28,40,60};int size=std::clamp(o.size,0,2);wall::Panel panel;
 if(wall::makePanel(hit.end,hit.plane.normal,widths[size],heights[size],camera.epoch,panel)&&wallscene::separated(panel,scene.items,40)&&trace::supported(panel,cast)&&trace::visible(panel,pose.eye,cast)){
 scene.add(panel,false,t,camera.tick,o);status("Manual panel placed; axes are fixed to the wall");log("Manual panel id="+std::to_string(scene.sequence));nextVisibility=0;
 }else if(!faulted)status("Wall patch unsupported, obstructed or too close to an existing panel");
 }else if(!faulted)status("No suitable wall hit (sky/glass/floor/solid start rejected)");
 }
 if(!faulted&&o.automatic&&scene.pending(t)&&!cleared&&camera.tick>=nextSearch){
 if(searchToken!=t.serial){searchToken=t.serial;searchAttempt=0;}
 nextSearch=camera.tick+200;wall::Panel panel;
 if(wallscene::candidate(camera,pose,searchAttempt++,t.serial,o,scene.items,cast,panel)){
 scene.add(panel,true,t,camera.tick,o);status("Lyric assigned to a new wall panel");log("Automatic panel id="+std::to_string(scene.sequence)+" lyric_token="+std::to_string(t.serial));nextVisibility=0;
 }else if(!faulted)status("Searching nearby walls; no suitable visible patch yet");
 }
 // Round-robin visibility budget: at most two panels / ten visibility rays per
 // 50ms tick. Unchecked snapshots become invisible after 350ms in the renderer.
 if(!faulted&&camera.tick>=nextVisibility&&!scene.items.empty()){
 nextVisibility=camera.tick+50;size_t count=std::min<size_t>(2,scene.items.size());
 for(size_t n=0;n<count;++n){auto& item=scene.items[visibilityCursor++%scene.items.size()];item.visible=trace::visible(item.panel,pose.eye,cast);item.visibleTick=camera.tick;if(faulted)break;}
 }
 if(!o.automatic&&!faulted)scene.assignManual(t,camera.tick);
 if(faulted)scene.reset();publish();
}
inline void stop(){enabled=false;placeAt=0;clearRequested=true;}
}
