#pragma once
#include "source_entities.hpp"
#include "dialogue.hpp"
#include "caption_resources.hpp"
#include "caption_binding.hpp"
#include <deque>
#include <mutex>
#include <condition_variable>
#include <thread>
namespace source_dialogue {
using namespace dango;
inline std::atomic<bool> enabled{false},display{true},english{false},reload{true},exportRequested{false};
inline std::atomic<uint64_t> session{0};
inline std::atomic<unsigned> height{84},received{0},dropped{0};inline std::atomic<int> hookState{0};
inline std::atomic<bool> installed{false};inline uint64_t retryAt=0;
struct Raw {uint32_t epoch=0,hash=0;uint64_t tick=0,session=0;unsigned duration=0;bool direct=false,valid=false,synthetic=false,flag=false;std::string injected;};
inline std::thread worker;inline std::atomic<bool> workerStop{false};inline std::mutex waitMutex;inline std::condition_variable awakened;
inline std::mutex queueMutex;inline std::deque<Raw> queue;
struct State {dialogue::Timeline timeline;uint64_t session=0;unsigned dictionary=0,resolved=0,matched=0;std::string status="Dialogue experiment off",language;std::vector<std::string> sources,errors;};
inline std::shared_ptr<const State> snapshot=std::make_shared<State>();
using Callback=void(__cdecl*)(void*);
inline std::shared_ptr<const std::vector<std::string>> bindingChecks=std::make_shared<const std::vector<std::string>>();
inline Callback originalCaption=nullptr,originalDirect=nullptr;inline uint32_t verifiedBase=0;inline uint32_t lastEpoch=0;
inline const char* hookStatus(){switch(hookState.load()){case 1:return "Native caption callback hooks ready";case 2:return "Profile mismatch; hook refused";case 3:return "Waiting for native caption callbacks";case 4:return "Caption hook setup exception";default:return "Waiting for Source callback";}}
inline void enqueue(Raw raw){std::lock_guard<std::mutex> lock(queueMutex);if(queue.size()>=256){++dropped;return;}queue.push_back(std::move(raw));awakened.notify_one();}
inline void capture(void* input,bool direct)noexcept{
 if(!enabled.load(std::memory_order_relaxed))return;try{
 Raw raw;raw.direct=direct;raw.session=session.load();raw.tick=GetTickCount64();raw.epoch=std::atomic_load(&source_camera::snapshot)->epoch;
 auto value=dialogue::readCaption(source_entities::reader,uint32_t(reinterpret_cast<uintptr_t>(input)));if(value){raw.valid=true;raw.hash=value->hash;raw.duration=value->durationMs;raw.flag=value->flag;}++received;enqueue(std::move(raw));
 }catch(...){++dropped;}
}
inline void __cdecl captionHook(void* input){capture(input,false);originalCaption(input);}
inline void __cdecl directHook(void* input){capture(input,true);originalDirect(input);}
inline bool replaceCell(uint32_t address,uint32_t expected,uint32_t wanted){
 if(!source_camera::readable(reinterpret_cast<void*>(uintptr_t(address)),4))return false;DWORD old=0;auto cell=reinterpret_cast<void**>(uintptr_t(address));if(!VirtualProtect(cell,4,PAGE_READWRITE,&old))return false;
 auto actual=InterlockedCompareExchangePointer(cell,reinterpret_cast<void*>(uintptr_t(wanted)),reinterpret_cast<void*>(uintptr_t(expected)));DWORD ignored=0;VirtualProtect(cell,4,old,&ignored);return uint32_t(reinterpret_cast<uintptr_t>(actual))==expected;
}
inline bool callbackCode(HMODULE module,uint32_t rva,uint32_t member){
 uint8_t code[42]{};auto base=uint32_t(reinterpret_cast<uintptr_t>(module));if(!source_entities::reader.read(base+rva,code,sizeof(code)))return false;uint32_t name=0;int32_t call=0;std::memcpy(&name,code+4,4);std::memcpy(&call,code+36,4);
 return code[0]==0x55&&code[1]==0x8b&&code[2]==0xec&&code[3]==0x68&&name==base+0x5591a4&&code[8]==0x6a&&code[9]==0xff&&code[35]==0xe8&&uint32_t(base+rva+40+call)==base+member&&code[40]==0x5d&&code[41]==0xc3;
}
inline captionwire::Registry registry(){auto a=uint32_t(reinterpret_cast<uintptr_t>(originalCaption)),b=uint32_t(reinterpret_cast<uintptr_t>(originalDirect));return captionwire::inspect(source_entities::reader,verifiedBase+0x6de3a4,a,b,uint32_t(reinterpret_cast<uintptr_t>(&captionHook)),uint32_t(reinterpret_cast<uintptr_t>(&directHook)));}
inline bool install(){
 auto module=source_camera::clientModule;if(!source_camera::hooked||!module||!source_camera::table)return false;auto base=uint32_t(reinterpret_cast<uintptr_t>(module));auto& reader=source_entities::reader;std::vector<std::string> checks;
 auto finish=[&](int status,const std::string& detail){checks.push_back(detail);auto previous=std::atomic_load(&bindingChecks);if(*previous!=checks){std::atomic_store(&bindingChecks,std::make_shared<const std::vector<std::string>>(checks));source_camera::log("Dialogue build 2: "+detail);}hookState=status;return status==1;};
 if(verifiedBase!=base){if(!source_camera::matchesImage(module,source_camera::ClientTimestamp,source_camera::ClientImageSize,"dialogue client.dll"))return finish(2,"Client image identity mismatch");}
 const uint8_t dispatchCode[]={0x55,0x8b,0xec,0x8b,0x45,0x08,0x83,0xec,0x28,0x85,0xc0,0x0f,0x88,0x39,0x01,0x00,0x00};
 if(!source_entities::bytes(module,0x1ac060,dispatchCode)||!callbackCode(module,0x11fbc0,0x11f160)||!callbackCode(module,0x11fbf0,0x11f4f0))return finish(2,"Native caption registry/callback fingerprint mismatch");
 if(verifiedBase!=base||!originalCaption){originalCaption=reinterpret_cast<Callback>(uintptr_t(base+0x11fbc0));originalDirect=reinterpret_cast<Callback>(uintptr_t(base+0x11fbf0));verifiedBase=base;}
 uint32_t dispatcher=0;auto table=uint32_t(reinterpret_cast<uintptr_t>(source_camera::table));if(!entitywire::load(reader,table+35*4,dispatcher))return finish(2,"Dispatcher cell unreadable (never modified)");
 checks.push_back("dispatcher_owner="+source_camera::targetOwner(reinterpret_cast<void*>(uintptr_t(dispatcher)))+"; preserved, not chained or replaced");
 auto scan=registry();checks.push_back("registry_count="+std::to_string(scan.count)+" caption="+std::to_string(scan.captionId)+" direct="+std::to_string(scan.directId));
 if(!scan.valid)return finish(3,scan.status);for(auto target:scan.foreign)checks.push_back("other_callback_owner="+source_camera::targetOwner(reinterpret_cast<void*>(uintptr_t(target)))+"; untouched");
 auto result=captionwire::reconcile(scan,true,base+0x11fbc0,base+0x11fbf0,uint32_t(reinterpret_cast<uintptr_t>(&captionHook)),uint32_t(reinterpret_cast<uintptr_t>(&directHook)),replaceCell);
 installed=result.active>0;checks.push_back("active_callback_cells="+std::to_string(result.active)+" raced="+std::to_string(result.raced));return finish(result.active?1:3,result.active?"Native caption callbacks ready; existing dispatcher preserved":scan.status);
}
inline void setEnabled(bool value){if(enabled.load()!=value)++session;enabled=value;source_entities::dialogueRequested=value;if(value)reload=true;awakened.notify_one();}
inline void onFrame(const GameCameraFrame& camera)noexcept{try{if(enabled&&(camera.epoch!=lastEpoch||camera.tick>=retryAt)){lastEpoch=camera.epoch;retryAt=camera.tick+1000;install();}}catch(...){hookState=4;enabled=false;source_entities::dialogueRequested=false;}}
inline void sample(int role){Raw raw;raw.session=session.load();raw.epoch=std::atomic_load(&source_camera::snapshot)->epoch;raw.tick=GetTickCount64();raw.duration=4000;raw.valid=true;raw.synthetic=true;raw.injected=std::string(dialogue::roleName(role))+u8": 台词跟随测试 · dialogue test";enqueue(std::move(raw));}
inline void stop(){enabled=false;source_entities::dialogueRequested=false;if(verifiedBase&&originalCaption&&originalDirect){auto scan=registry();captionwire::reconcile(scan,false,verifiedBase+0x11fbc0,verifiedBase+0x11fbf0,uint32_t(reinterpret_cast<uintptr_t>(&captionHook)),uint32_t(reinterpret_cast<uintptr_t>(&directHook)),replaceCell);}installed=false;}

inline std::filesystem::path gameRoot(){wchar_t path[32768]{};auto n=GetModuleFileNameW(nullptr,path,32768);return n&&n<32768?std::filesystem::path(path).parent_path():std::filesystem::path{};}
inline void writeLogs(const std::filesystem::path& directory,const std::vector<nlohmann::json>& logs){static uint64_t bytes=0;static bool full=false;
 if(!logs.empty()&&!full)try{std::filesystem::create_directories(directory);auto path=directory/L"dialogue.jsonl";if(!bytes&&std::filesystem::exists(path))bytes=std::filesystem::file_size(path);std::ofstream out(path,std::ios::app|std::ios::binary);for(auto& entry:logs){auto line=entry.dump(-1,' ',false,nlohmann::json::error_handler_t::replace);if(bytes+line.size()>32*1024*1024){full=true;source_camera::log("Dialogue diagnostic reached 32 MiB; rotate dialogue.jsonl before another run");break;}out<<line<<'\n';bytes+=line.size()+1;}out.flush();if(!out)source_camera::log("Dialogue diagnostic write failed");}catch(const std::exception& e){source_camera::log(std::string("Dialogue diagnostic: ")+e.what());}
}
// Resource IO, text parsing and logging use a dedicated worker, independent of music HTTP.
inline void drain(const std::filesystem::path& directory){
 static captions::Dictionary dictionary;static State state;static bool wasEnabled=false;static std::string language;static std::vector<std::string> lastBindings;
 auto now=GetTickCount64();bool active=enabled.load();auto camera=std::atomic_load(&source_camera::snapshot);state.timeline.expire(camera->epoch,now);
 if(!active){if(wasEnabled){state.timeline.lines.clear();state.status="Dialogue experiment off";std::atomic_store(&snapshot,std::make_shared<const State>(state));std::lock_guard<std::mutex> lock(queueMutex);queue.clear();}wasEnabled=false;if(exportRequested.exchange(false))writeLogs(directory,{{{"event","status"},{"dialogue_build",2},{"enabled",false},{"hook",hookStatus()},{"binding_checks",*std::atomic_load(&bindingChecks)},{"received",received.load()},{"dropped",dropped.load()},{"dictionary",state.dictionary},{"resolved",state.resolved},{"matched",state.matched},{"sources",state.sources},{"errors",state.errors}}});return;}
 if(state.session!=session.load()){state.timeline.lines.clear();state.session=session.load();}
 std::vector<nlohmann::json> logs;auto currentBindings=std::atomic_load(&bindingChecks);if(*currentBindings!=lastBindings){lastBindings=*currentBindings;logs.push_back({{"event","bindings"},{"dialogue_build",2},{"tick",now},{"hook",hookStatus()},{"binding_checks",lastBindings}});}auto lang=english.load()?"english":"schinese";
 bool requestedReload=reload.exchange(false);if(!wasEnabled||requestedReload||language!=lang){state.timeline.lines.clear();state.status="Loading native caption resources";std::atomic_store(&snapshot,std::make_shared<const State>(state));try{dictionary=captions::load(gameRoot(),directory/"caption-resources",lang);}catch(const std::exception& e){dictionary={};dictionary.errors.push_back(std::string("Resource load failed: ")+e.what());}language=lang;state.language=language;state.dictionary=unsigned(dictionary.text.size());state.sources=dictionary.sources;state.errors=dictionary.errors;state.status=dictionary.text.empty()?"No caption dictionary; see dialogue.jsonl":"Dictionary loaded; waiting for captions";logs.push_back({{"event","resources"},{"dialogue_build",2},{"tick",now},{"entries",dictionary.text.size()},{"language",language},{"sources",dictionary.sources},{"errors",dictionary.errors}});}
 now=GetTickCount64();camera=std::atomic_load(&source_camera::snapshot);state.timeline.expire(camera->epoch,now);wasEnabled=true;std::deque<Raw> input;{std::lock_guard<std::mutex> lock(queueMutex);input.swap(queue);}auto entities=std::atomic_load(&source_entities::snapshot);
 for(const auto& raw:input){if(raw.session!=state.session)continue;std::string text=raw.injected;if(!raw.synthetic&&raw.valid){auto it=dictionary.text.find(raw.hash);if(it!=dictionary.text.end())text=it->second;}if(!text.empty())++state.resolved;auto spoken=dialogue::parse(text);auto match=dialogue::match(*entities,spoken.role,raw.epoch,now);
 std::string status=!raw.valid?"invalid_wire":text.empty()?"hash_not_found":match.status;if(raw.epoch!=camera->epoch)status="wrong_map";
 if(status=="matched"){unsigned duration=raw.duration?std::clamp(raw.duration,500u,30000u):3000;dialogue::Caption caption{raw.epoch,match.handle,raw.hash,spoken.role,raw.tick,raw.tick+duration,spoken.text,raw.synthetic};if(state.timeline.add(std::move(caption),camera->epoch,now))++state.matched;else status="expired_or_duplicate";}
 state.status=status+" · "+dialogue::roleName(spoken.role);nlohmann::json roster=nlohmann::json::array();for(const auto& r:entities->records)if(r.character&&roster.size()<16){nlohmann::json item={{"handle",r.handle},{"class",r.className},{"character",*r.character}};if(r.team)item["team"]=*r.team;if(r.dormant)item["dormant"]=*r.dormant;if(r.health)item["health"]=*r.health;if(r.lifeState)item["life_state"]=*r.lifeState;auto pose=follow::pose(*entities,r.handle);item["pose_ready"]=pose.ready;item["pose_status"]=pose.status;roster.push_back(item);}
 logs.push_back({{"event","caption"},{"dialogue_build",2},{"tick",raw.tick},{"processed",now},{"epoch",raw.epoch},{"session",raw.session},{"inventory_epoch",entities->epoch},{"inventory_tick",entities->tick},{"hash",raw.hash},{"type",raw.synthetic?"test":raw.direct?"CloseCaptionDirect":"CloseCaption"},{"duration_ms",raw.duration},{"wire_flag",raw.flag},{"text",text},{"speaker",spoken.speaker},{"role",spoken.role},{"status",status},{"candidate_count",match.candidates},{"handle",match.handle},{"synthetic",raw.synthetic},{"players",roster}});
 }
 if(exportRequested.exchange(false))logs.push_back({{"event","status"},{"dialogue_build",2},{"tick",now},{"hook",hookStatus()},{"binding_checks",*std::atomic_load(&bindingChecks)},{"received",received.load()},{"dropped",dropped.load()},{"dictionary",state.dictionary},{"resolved",state.resolved},{"matched",state.matched},{"active",state.timeline.lines.size()},{"sources",state.sources},{"errors",state.errors}});
 std::atomic_store(&snapshot,std::make_shared<const State>(state));
 writeLogs(directory,logs);
}
inline void start(const std::filesystem::path& directory)noexcept{try{workerStop=false;worker=std::thread([directory](){while(!workerStop){try{drain(directory);}catch(const std::exception& e){source_camera::log(std::string("Dialogue worker: ")+e.what());}catch(...){source_camera::log("Dialogue worker exception");}std::unique_lock<std::mutex> lock(waitMutex);awakened.wait_for(lock,std::chrono::milliseconds(100));}});}catch(...){source_camera::log("Dialogue worker could not start");}}
inline void shutdown(){stop();workerStop=true;awakened.notify_all();if(worker.joinable())worker.join();}

}
