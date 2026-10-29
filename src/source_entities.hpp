#pragma once
#include "source_camera.hpp"
#include "source_entity_contract.hpp"
#include "source_entity_profile.hpp"
#include "json.hpp"
#include <filesystem>
#include <fstream>
#include <map>
namespace source_entities {
using namespace dango;
namespace wire=dango::entitywire;
struct Record {
 uint32_t handle=0,index=0,serial=0;std::string className,modelName;int classId=-1;
 std::optional<bool> dormant;std::optional<Vec3> networkOrigin,networkAngles;
 std::optional<int> team,modelIndex;std::optional<uint32_t> owner,activeWeapon,viewModel;
};
struct Frame {uint32_t epoch=0;uint64_t tick=0;unsigned occupied=0,unknownClass=0,unknownBase=0,unknownTables=0;std::vector<Record> records;std::vector<std::string> getterHints;std::string status="Entity diagnostics off";};
inline std::shared_ptr<const Frame> snapshot=std::make_shared<Frame>();
inline std::atomic<bool> enabled{false},dumpRequested{false},resetRequested{false};
inline void* list=nullptr;inline void* modelInfo=nullptr;inline bool bound=false,faulted=false,wasEnabled=false;
inline uint32_t epoch=0;inline uint64_t nextCapture=0;inline std::atomic<uint64_t> fileFrames{0};
struct Reader {
 bool read(uint32_t address,void* out,size_t size)const{SIZE_T got=0;return address&&size<=256*1024&&uint64_t(address)+size<=0x100000000ull&&ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<const void*>(uintptr_t(address)),out,size,&got)&&got==size;}
};
inline Reader reader;
struct ClassInfo {std::string name;int id=-1;wire::Fields fields;bool schema=false;};
inline std::map<uint32_t,ClassInfo> classes;inline std::map<int,std::string> models;
inline void publish(Frame frame){std::atomic_store(&snapshot,std::make_shared<const Frame>(std::move(frame)));}
inline void log(const std::string& text){source_camera::log("Entity inventory: "+text);}
template<size_t N> bool bytes(HMODULE module,uint32_t rva,const uint8_t(&wanted)[N]){uint8_t actual[N];return reader.read(uint32_t(reinterpret_cast<uintptr_t>(module))+rva,actual,N)&&!std::memcmp(actual,wanted,N);}
template<size_t N> bool table(void* object,HMODULE module,uint32_t objectRva,uint32_t tableRva,const uint32_t(&methods)[N]){
 auto base=uint32_t(reinterpret_cast<uintptr_t>(module));if(reinterpret_cast<uintptr_t>(object)!=base+objectRva)return false;uint32_t vt;
 if(!wire::load(reader,base+objectRva,vt)||vt!=base+tableRva)return false;
 for(unsigned i=0;i<N;++i){uint32_t method;if(!wire::load(reader,vt+i*4,method)||method!=base+methods[i]||!source_camera::executable(reinterpret_cast<void*>(uintptr_t(method))))return false;}return true;
}
inline bool connect(){
 using namespace source_camera;
 if(!hooked||!clientModule||!engineModule||!matchesImage(clientModule,ClientTimestamp,ClientImageSize,"entity client.dll")||!matchesImage(engineModule,EngineTimestamp,EngineImageSize,"entity engine.dll"))return false;
 using Factory=void*(__cdecl*)(const char*,int*);auto cf=reinterpret_cast<Factory>(GetProcAddress(clientModule,"CreateInterface"));auto ef=reinterpret_cast<Factory>(GetProcAddress(engineModule,"CreateInterface"));if(!cf||!ef)return false;
 list=cf("VClientEntityList003",nullptr);modelInfo=ef("VModelInfoClient004",nullptr);
 if(!table(list,clientModule,profile::Object,profile::Vtable,profile::Methods)||!table(modelInfo,engineModule,profile::ModelObject,profile::ModelVtable,profile::ModelMethods))return false;
 return bytes(clientModule,profile::Methods[6],profile::HighestCode)&&bytes(clientModule,profile::Methods[2],profile::HandleCode)&&bytes(engineModule,profile::ModelMethods[1],profile::ModelCode)&&bytes(engineModule,profile::ModelMethods[3],profile::NameCode);
}
inline bool method(uint32_t object,unsigned slot,uint32_t& function){uint32_t vt;return wire::load(reader,object,vt)&&source_camera::owns(reinterpret_cast<void*>(uintptr_t(vt)),source_camera::clientModule)&&wire::load(reader,vt+slot*4,function)&&source_camera::owns(reinterpret_cast<void*>(uintptr_t(function)),source_camera::clientModule)&&source_camera::executable(reinterpret_cast<void*>(uintptr_t(function)));}
inline std::optional<uint32_t> classAddress(uint32_t networkable){uint32_t fn;if(!method(networkable,2,fn))return {};return wire::addressGetter(reader,fn,networkable);}
inline ClassInfo readClass(uint32_t address){
 ClassInfo out;wire::ClientClass c;if(!source_camera::owns(reinterpret_cast<void*>(uintptr_t(address)),source_camera::clientModule)||!wire::load(reader,address,c))return out;
 out.name=wire::text(reader,c.name);if(out.name.empty()||out.name[0]!='C'||c.id<0||c.id>4096){out.name.clear();return out;}out.id=c.id;out.schema=wire::fields(reader,c.table,out.fields);return out;
}
template<class T> std::optional<T> field(uint32_t base,const ClassInfo& c,const char* name,int type){auto it=c.fields.find(name);if(!c.schema||it==c.fields.end()||it->second.type!=type||uint64_t(base)+it->second.offset+sizeof(T)>0x100000000ull)return {};T value;if(!wire::load(reader,base+it->second.offset,value))return {};return value;}
inline std::optional<Vec3> vectorField(uint32_t base,const ClassInfo& c,const char* name){auto v=field<Vec3>(base,c,name,2);if(!v||!std::isfinite(v->x)||!std::isfinite(v->y)||!std::isfinite(v->z)||std::max({std::abs(v->x),std::abs(v->y),std::abs(v->z)})>1000000)return {};return v;}
inline std::string modelName(int index){
 if(index<=0||index>=4096)return {};auto known=models.find(index);if(known!=models.end())return known->second;
 // Only these two profiled engine read-only virtuals are dispatched. Entity
 // virtuals are decoded as tiny getters; unknown forms are not called.
 using GetModel=void*(__thiscall*)(void*,int);using Name=const char*(__thiscall*)(void*,const void*);
 if(!table(modelInfo,source_camera::engineModule,profile::ModelObject,profile::ModelVtable,profile::ModelMethods)){faulted=true;return {};}
 auto vt=*reinterpret_cast<void***>(modelInfo);auto model=reinterpret_cast<GetModel>(vt[1])(modelInfo,index);std::string name;
 if(model&&source_camera::readable(model,4)){auto ptr=reinterpret_cast<Name>(vt[3])(modelInfo,model);name=wire::text(reader,uint32_t(reinterpret_cast<uintptr_t>(ptr)),260);}
 if(!name.empty())models[index]=name;return name;
}
inline void getterHint(Frame& frame,const char* kind,uint32_t fn){
 if(!fn||frame.getterHints.size()>=16||!source_camera::owns(reinterpret_cast<void*>(uintptr_t(fn)),source_camera::clientModule))return;uint8_t code[12];if(!reader.read(fn,code,sizeof(code)))return;char prefix[96];std::snprintf(prefix,sizeof(prefix),"%s client+0x%x bytes=",kind,fn-uint32_t(reinterpret_cast<uintptr_t>(source_camera::clientModule)));std::string hint=prefix;const char* digits="0123456789abcdef";for(auto c:code){hint+=digits[c>>4];hint+=digits[c&15];}if(std::find(frame.getterHints.begin(),frame.getterHints.end(),hint)==frame.getterHints.end())frame.getterHints.push_back(std::move(hint));
}
inline void capture(const GameCameraFrame& camera){
 Frame frame;frame.epoch=camera.epoch;frame.tick=camera.tick;
 if(!camera.active){frame.status="Waiting for active Source camera";publish(std::move(frame));return;}
 if(faulted){frame.status="Entity ABI/profile mismatch; toggle diagnostics to retry";publish(std::move(frame));return;}
 if(!bound){bound=connect();if(!bound){faulted=true;frame.status="Entity ABI/profile mismatch; no entity calls made";log(frame.status);publish(std::move(frame));return;}log("Bound verified entity list and model-info profiles; runtime data validation pending");}
 uint32_t object=uint32_t(reinterpret_cast<uintptr_t>(list));int highest=-1;
 if(!wire::load(reader,object+profile::HighestOffset,highest)||highest< -1||highest>=4096){faulted=true;frame.status="Entity highest-index validation failed";publish(std::move(frame));return;}
 std::array<wire::Entry,profile::MaxNetworked> entries{};struct Cache {uint32_t networkable;uint16_t dormant,padding;};std::array<Cache,profile::MaxNetworked> cache{};
 uint32_t entryBase=uint32_t(int64_t(object)+profile::EntryBaseDelta);
 if(!reader.read(entryBase,entries.data(),sizeof(entries))||!reader.read(object+profile::CacheOffset,cache.data(),sizeof(cache))){faulted=true;frame.status="Entity list memory unavailable";publish(std::move(frame));return;}
 for(unsigned i=0;i<unsigned(std::min(highest+1,int(profile::MaxNetworked)));++i){auto entry=entries[i];auto net=cache[i].networkable;if(!entry.entity||!net)continue;++frame.occupied;
 auto h=wire::handle(i,entry.serial);if(!h)continue;Record r;r.index=i;r.serial=entry.serial;r.handle=*h;
 auto cls=classAddress(net);if(!cls){++frame.unknownClass;uint32_t getter=0;if(method(net,2,getter))getterHint(frame,"class",getter);continue;}auto it=classes.find(*cls);if(it==classes.end())it=classes.emplace(*cls,readClass(*cls)).first;const auto& info=it->second;if(info.name.empty()){++frame.unknownClass;continue;}r.className=info.name;r.classId=info.id;
 uint32_t fn;if(method(net,8,fn))r.dormant=wire::byteGetter(reader,fn,net);
 // The cached IsDormant bit is only a diagnostic comparison, not authority.
 uint32_t base=0,dataGetter=0;if(method(net,11,dataGetter)){auto b=wire::addressGetter(reader,dataGetter,net);if(b&&*b==entry.entity)base=*b;}
 if(!base){++frame.unknownBase;getterHint(frame,"data_base",dataGetter);}else if(!info.schema)++frame.unknownTables;else{
 r.networkOrigin=vectorField(base,info,"m_vecOrigin");r.networkAngles=vectorField(base,info,"m_angRotation");r.team=field<int>(base,info,"m_iTeamNum",0);
 auto index=field<int16_t>(base,info,"m_nModelIndex",0);if(index){r.modelIndex=int(*index);r.modelName=modelName(*index);}
 r.owner=field<uint32_t>(base,info,"m_hOwnerEntity",0);r.activeWeapon=field<uint32_t>(base,info,"m_hActiveWeapon",0);r.viewModel=field<uint32_t>(base,info,"m_hViewModel[0]",0);
 }frame.records.push_back(std::move(r));}
 frame.status="Network inventory: "+std::to_string(frame.records.size())+" objects; position source=network properties; not render poses";publish(std::move(frame));
}
inline void onFrame(const GameCameraFrame& camera)noexcept{
 try{bool active=enabled.load();if(!active){if(wasEnabled){classes.clear();models.clear();bound=faulted=false;list=modelInfo=nullptr;Frame f;f.epoch=camera.epoch;f.tick=camera.tick;publish(std::move(f));}wasEnabled=false;return;}
 if(resetRequested.exchange(false)||!wasEnabled||epoch!=camera.epoch){classes.clear();models.clear();bound=faulted=false;nextCapture=0;epoch=camera.epoch;wasEnabled=true;}
 if(camera.tick<nextCapture)return;nextCapture=camera.tick+200;capture(camera);
 }catch(...){faulted=true;log("Capture stopped after an exception; toggle diagnostics to retry");}
}
inline nlohmann::json jsonFrame(const Frame& f){
 nlohmann::json out={{"schema",1},{"epoch",f.epoch},{"tick",f.tick},{"status",f.status},{"pose_space","network_raw"},{"occupied",f.occupied},{"unknown_class",f.unknownClass},{"unknown_base",f.unknownBase},{"unknown_tables",f.unknownTables},{"getter_hints",f.getterHints},{"objects",nlohmann::json::array()}};
 for(auto& r:f.records){nlohmann::json item={{"handle",r.handle},{"index",r.index},{"serial",r.serial},{"class",r.className},{"class_id",r.classId},{"model",r.modelName}};
 auto vector=[&](const char* key,const std::optional<Vec3>& v){if(v)item[key]={v->x,v->y,v->z};};vector("origin",r.networkOrigin);vector("angles",r.networkAngles);
 if(r.dormant)item["dormant_candidate"]=*r.dormant;if(r.team)item["team"]=*r.team;if(r.modelIndex)item["model_index"]=*r.modelIndex;if(r.owner)item["owner_handle"]=*r.owner;if(r.activeWeapon)item["active_weapon_handle"]=*r.activeWeapon;if(r.viewModel)item["viewmodel_handle"]=*r.viewModel;out["objects"].push_back(std::move(item));}return out;
}
// Only called from the existing background worker; never writes on game/render threads.
inline void drain(const std::filesystem::path& directory){
 static uint64_t next=0,last=0;uint64_t tick=GetTickCount64();bool requested=dumpRequested.exchange(false);if(!requested&&(!enabled||tick<next))return;next=tick+1000;auto frame=std::atomic_load(&snapshot);if(!frame->tick||(!requested&&frame->tick==last))return;
 try{std::filesystem::create_directories(directory);std::ofstream file(directory/L"entities.jsonl",std::ios::app|std::ios::binary);file<<jsonFrame(*frame).dump(-1,' ',false,nlohmann::json::error_handler_t::replace)<<'\n';file.flush();if(!file)throw std::runtime_error("write failed");last=frame->tick;++fileFrames;}
 catch(const std::exception& e){enabled=false;log(std::string("Diagnostic output failed: ")+e.what());}
}
}
