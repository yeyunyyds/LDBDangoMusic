#pragma once
#include "source_entities.hpp"
#include "dialogue.hpp"
#include "caption_resources.hpp"
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
using Dispatch=bool(__thiscall*)(void*,int,void*);
inline std::shared_ptr<const std::vector<std::string>> bindingChecks=std::make_shared<const std::vector<std::string>>();
inline Dispatch original=nullptr;inline int captionId=-1,directId=-1;inline void** hookedTable=nullptr;
inline const char* hookStatus(){switch(hookState.load()){case 1:return "Caption message hook ready";case 2:return "Profile mismatch; hook refused";case 3:return "Waiting for registered caption messages";case 4:return "Caption hook setup exception";default:return "Waiting for Source callback";}}
inline void enqueue(Raw raw){std::lock_guard<std::mutex> lock(queueMutex);if(queue.size()>=256){++dropped;return;}queue.push_back(std::move(raw));awakened.notify_one();}
inline bool __fastcall dispatchHook(void* self,void*,int message,void* input){
 if(enabled.load(std::memory_order_relaxed)&&(message==captionId||message==directId))try{
 Raw raw;raw.direct=message==directId;raw.session=session.load();raw.tick=GetTickCount64();raw.epoch=std::atomic_load(&source_camera::snapshot)->epoch;
 auto value=dialogue::readCaption(source_entities::reader,uint32_t(reinterpret_cast<uintptr_t>(input)));if(value){raw.valid=true;raw.hash=value->hash;raw.duration=value->durationMs;raw.flag=value->flag;}++received;enqueue(std::move(raw));
 }catch(...){++dropped;}
 // Never consume or change the engine's reader. Original dispatch always runs.
 return original(self,message,input);
}
inline bool install(){
 if(installed)return true;auto module=source_camera::clientModule;if(!source_camera::hooked||!module||!source_camera::table)return false;auto base=uint32_t(reinterpret_cast<uintptr_t>(module));auto& reader=source_entities::reader;std::vector<std::string> checks;
 auto finish=[&](int status,const std::string& detail){checks.push_back(detail);std::atomic_store(&bindingChecks,std::make_shared<const std::vector<std::string>>(checks));hookState=status;return status==1;};
 if(!source_camera::matchesImage(module,source_camera::ClientTimestamp,source_camera::ClientImageSize,"dialogue client.dll")){return finish(2,"Client image identity mismatch");}
 const uint8_t dispatchCode[]={0x55,0x8b,0xec,0x8b,0x45,0x08,0x83,0xec,0x28,0x85,0xc0,0x0f,0x88,0x39,0x01,0x00,0x00};if(!source_entities::bytes(module,0x1ac060,dispatchCode)){return finish(2,"Dispatch body fingerprint mismatch");}
 auto table=source_camera::table;uint8_t code[15]{};uint32_t slot=0,managerOperand=0,jump=0;
 checks.push_back("table_rva="+std::to_string(uint32_t(reinterpret_cast<uintptr_t>(table))-base));
 const uint8_t prefix[]={0x55,0x8b,0xec,0x8b,0x0d};
 if(uint32_t(reinterpret_cast<uintptr_t>(table))!=base+0x56441c||!entitywire::load(reader,base+0x56441c+35*4,slot)||slot!=base+0xa9160||!reader.read(slot,code,sizeof(code))||std::memcmp(code,prefix,5)||code[9]!=0x5d||code[10]!=0xe9){return finish(2,"Dispatch slot/entry mismatch; actual_rva="+std::to_string(slot?slot-base:0));}
 std::memcpy(&managerOperand,code+5,4);std::memcpy(&jump,code+11,4);if(managerOperand!=base+0x6de3a4||uint32_t(slot+15+jump)!=base+0x1ac060){return finish(2,"Dispatch manager operand/tail target mismatch");}
 uint32_t manager=0,nodes=0;int count=0;if(!entitywire::load(reader,managerOperand,manager)||!manager||!entitywire::load(reader,manager+4,nodes)||!entitywire::load(reader,manager+0x14,count)||count<1||count>512||!nodes){return finish(3,"Caption registry unavailable; count="+std::to_string(count));}
 int a=-1,b=-1;for(int i=0;i<count;++i){uint32_t object=0,name=0;if(!entitywire::load(reader,nodes+uint32_t(i)*24+20,object)||!object||!entitywire::load(reader,object+4,name))continue;auto text=entitywire::text(reader,name,128);if(text=="CloseCaption")a=i;if(text=="CloseCaptionDirect")b=i;}
 if(a<0||b<0||a==b){return finish(3,"Caption names unavailable; id="+std::to_string(a)+" direct="+std::to_string(b));}captionId=a;directId=b;original=reinterpret_cast<Dispatch>(uintptr_t(slot));hookedTable=table;
 if(!source_camera::replace(35,reinterpret_cast<void*>(original),reinterpret_cast<void*>(&dispatchHook))){return finish(2,"Dispatch slot changed while installing");}installed=true;finish(1,"Verified caption ids="+std::to_string(a)+","+std::to_string(b)+" registry_count="+std::to_string(count));source_camera::log("Dialogue build 1: verified DispatchUserMessage[35] hook; CloseCaption="+std::to_string(a)+" Direct="+std::to_string(b));return true;
}
inline void setEnabled(bool value){if(enabled.load()!=value)++session;enabled=value;source_entities::dialogueRequested=value;if(value)reload=true;awakened.notify_one();}
inline void onFrame(const GameCameraFrame& camera)noexcept{try{if(enabled&&!installed&&camera.tick>=retryAt){retryAt=camera.tick+1000;install();}}catch(...){hookState=4;enabled=false;source_entities::dialogueRequested=false;}}
inline void sample(int role){Raw raw;raw.session=session.load();raw.epoch=std::atomic_load(&source_camera::snapshot)->epoch;raw.tick=GetTickCount64();raw.duration=4000;raw.valid=true;raw.synthetic=true;raw.injected=std::string(dialogue::roleName(role))+u8": 台词跟随测试 · dialogue test";enqueue(std::move(raw));}
inline void stop(){enabled=false;source_entities::dialogueRequested=false;if(installed&&source_camera::readable(hookedTable,36*sizeof(void*))){DWORD old=0;if(VirtualProtect(hookedTable+35,sizeof(void*),PAGE_READWRITE,&old)){InterlockedCompareExchangePointer(hookedTable+35,reinterpret_cast<void*>(original),reinterpret_cast<void*>(&dispatchHook));DWORD ignored=0;VirtualProtect(hookedTable+35,sizeof(void*),old,&ignored);}}installed=false;}
inline std::filesystem::path gameRoot(){wchar_t path[32768]{};auto n=GetModuleFileNameW(nullptr,path,32768);return n&&n<32768?std::filesystem::path(path).parent_path():std::filesystem::path{};}
inline void writeLogs(const std::filesystem::path& directory,const std::vector<nlohmann::json>& logs){static uint64_t bytes=0;static bool full=false;
 if(!logs.empty()&&!full)try{std::filesystem::create_directories(directory);auto path=directory/L"dialogue.jsonl";if(!bytes&&std::filesystem::exists(path))bytes=std::filesystem::file_size(path);std::ofstream out(path,std::ios::app|std::ios::binary);for(auto& entry:logs){auto line=entry.dump(-1,' ',false,nlohmann::json::error_handler_t::replace);if(bytes+line.size()>32*1024*1024){full=true;source_camera::log("Dialogue diagnostic reached 32 MiB; rotate dialogue.jsonl before another run");break;}out<<line<<'\n';bytes+=line.size()+1;}out.flush();if(!out)source_camera::log("Dialogue diagnostic write failed");}catch(const std::exception& e){source_camera::log(std::string("Dialogue diagnostic: ")+e.what());}
}
// Resource IO, text parsing and logging use a dedicated worker, independent of music HTTP.
inline void drain(const std::filesystem::path& directory){
 static captions::Dictionary dictionary;static State state;static bool wasEnabled=false;static std::string language;
 auto now=GetTickCount64();bool active=enabled.load();auto camera=std::atomic_load(&source_camera::snapshot);state.timeline.expire(camera->epoch,now);
 if(!active){if(wasEnabled){state.timeline.lines.clear();state.status="Dialogue experiment off";std::atomic_store(&snapshot,std::make_shared<const State>(state));std::lock_guard<std::mutex> lock(queueMutex);queue.clear();}wasEnabled=false;if(exportRequested.exchange(false))writeLogs(directory,{{{"event","status"},{"dialogue_build",1},{"enabled",false},{"hook",hookStatus()},{"binding_checks",*std::atomic_load(&bindingChecks)},{"received",received.load()},{"dropped",dropped.load()},{"dictionary",state.dictionary},{"resolved",state.resolved},{"matched",state.matched},{"sources",state.sources},{"errors",state.errors}}});return;}
 if(state.session!=session.load()){state.timeline.lines.clear();state.session=session.load();}
 std::vector<nlohmann::json> logs;auto lang=english.load()?"english":"schinese";
 bool requestedReload=reload.exchange(false);if(!wasEnabled||requestedReload||language!=lang){state.timeline.lines.clear();state.status="Loading native caption resources";std::atomic_store(&snapshot,std::make_shared<const State>(state));try{dictionary=captions::load(gameRoot(),directory/"caption-resources",lang);}catch(const std::exception& e){dictionary={};dictionary.errors.push_back(std::string("Resource load failed: ")+e.what());}language=lang;state.language=language;state.dictionary=unsigned(dictionary.text.size());state.sources=dictionary.sources;state.errors=dictionary.errors;state.status=dictionary.text.empty()?"No caption dictionary; see dialogue.jsonl":"Dictionary loaded; waiting for captions";logs.push_back({{"event","resources"},{"dialogue_build",1},{"tick",now},{"entries",dictionary.text.size()},{"language",language},{"sources",dictionary.sources},{"errors",dictionary.errors}});}
 now=GetTickCount64();camera=std::atomic_load(&source_camera::snapshot);state.timeline.expire(camera->epoch,now);wasEnabled=true;std::deque<Raw> input;{std::lock_guard<std::mutex> lock(queueMutex);input.swap(queue);}auto entities=std::atomic_load(&source_entities::snapshot);
 for(const auto& raw:input){if(raw.session!=state.session)continue;std::string text=raw.injected;if(!raw.synthetic&&raw.valid){auto it=dictionary.text.find(raw.hash);if(it!=dictionary.text.end())text=it->second;}if(!text.empty())++state.resolved;auto spoken=dialogue::parse(text);auto match=dialogue::match(*entities,spoken.role,raw.epoch,now);
 std::string status=!raw.valid?"invalid_wire":text.empty()?"hash_not_found":match.status;if(raw.epoch!=camera->epoch)status="wrong_map";
 if(status=="matched"){unsigned duration=raw.duration?std::clamp(raw.duration,500u,30000u):3000;dialogue::Caption caption{raw.epoch,match.handle,raw.hash,spoken.role,raw.tick,raw.tick+duration,spoken.text,raw.synthetic};if(state.timeline.add(std::move(caption),camera->epoch,now))++state.matched;else status="expired_or_duplicate";}
 state.status=status+" · "+dialogue::roleName(spoken.role);nlohmann::json roster=nlohmann::json::array();for(const auto& r:entities->records)if(r.character&&roster.size()<16){nlohmann::json item={{"handle",r.handle},{"class",r.className},{"character",*r.character}};if(r.team)item["team"]=*r.team;if(r.dormant)item["dormant"]=*r.dormant;if(r.health)item["health"]=*r.health;if(r.lifeState)item["life_state"]=*r.lifeState;auto pose=follow::pose(*entities,r.handle);item["pose_ready"]=pose.ready;item["pose_status"]=pose.status;roster.push_back(item);}
 logs.push_back({{"event","caption"},{"dialogue_build",1},{"tick",raw.tick},{"processed",now},{"epoch",raw.epoch},{"session",raw.session},{"inventory_epoch",entities->epoch},{"inventory_tick",entities->tick},{"hash",raw.hash},{"type",raw.synthetic?"test":raw.direct?"CloseCaptionDirect":"CloseCaption"},{"duration_ms",raw.duration},{"wire_flag",raw.flag},{"text",text},{"speaker",spoken.speaker},{"role",spoken.role},{"status",status},{"candidate_count",match.candidates},{"handle",match.handle},{"synthetic",raw.synthetic},{"players",roster}});
 }
 if(exportRequested.exchange(false))logs.push_back({{"event","status"},{"dialogue_build",1},{"tick",now},{"hook",hookStatus()},{"binding_checks",*std::atomic_load(&bindingChecks)},{"received",received.load()},{"dropped",dropped.load()},{"dictionary",state.dictionary},{"resolved",state.resolved},{"matched",state.matched},{"active",state.timeline.lines.size()},{"sources",state.sources},{"errors",state.errors}});
 std::atomic_store(&snapshot,std::make_shared<const State>(state));
 writeLogs(directory,logs);
}
inline void start(const std::filesystem::path& directory)noexcept{try{workerStop=false;worker=std::thread([directory](){while(!workerStop){try{drain(directory);}catch(const std::exception& e){source_camera::log(std::string("Dialogue worker: ")+e.what());}catch(...){source_camera::log("Dialogue worker exception");}std::unique_lock<std::mutex> lock(waitMutex);awakened.wait_for(lock,std::chrono::milliseconds(100));}});}catch(...){source_camera::log("Dialogue worker could not start");}}
inline void shutdown(){stop();workerStop=true;awakened.notify_all();if(worker.joinable())worker.join();}

}
