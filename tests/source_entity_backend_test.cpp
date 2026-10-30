#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include "source_entities.hpp"
#include <iostream>
#include <stdexcept>
using namespace dango;
namespace wire=dango::entitywire;
void check(bool v){if(!v)throw std::runtime_error("Entity diagnostic producer mock failed");}
static wire::ClientClass mockClass;
__declspec(noinline) void* __fastcall classGetter(void*,void*){return &mockClass;}
__declspec(noinline) void* __fastcall baseGetter(void* self,void*){return static_cast<char*>(self)-8;}
uint32_t ptr(const void* p){return uint32_t(reinterpret_cast<uintptr_t>(p));}
struct FakeEntity {uint32_t prefix[2]{};void** networkTable=nullptr;Vec3 origin{10,20,30};int team=2;uint32_t owner=0xffffffff,weapon=0x50005;};
int main(){
 static_assert(sizeof(void*)==4,"Windows x86 only");source_camera::clientModule=GetModuleHandleW(nullptr);
 static void* netMethods[12]{};netMethods[2]=reinterpret_cast<void*>(&classGetter);netMethods[11]=reinterpret_cast<void*>(&baseGetter);
 FakeEntity entity;entity.networkTable=netMethods;
 wire::Prop props[4]{};const char* names[]={"m_vecOrigin","m_iTeamNum","m_hOwnerEntity","m_hActiveWeapon"};const int offsets[]={offsetof(FakeEntity,origin),offsetof(FakeEntity,team),offsetof(FakeEntity,owner),offsetof(FakeEntity,weapon)};
 for(int i=0;i<4;++i){props[i].name=ptr(names[i]);props[i].type=i?0:2;props[i].offset=offsets[i];}
 wire::Table recv;recv.props=ptr(props);recv.count=4;recv.name=ptr("DT_MockEntity");mockClass.name=ptr("CMockEntity");mockClass.table=ptr(&recv);mockClass.id=42;
 // The list layout is the real producer's profiled layout. Binding is skipped
 // only in this mock; no actual game/model-info methods are invoked.
 std::vector<uint8_t> storage(0x15000);auto listAddress=ptr(storage.data()+0x10010);int highest=1;std::memcpy(reinterpret_cast<void*>(uintptr_t(listAddress+0x24)),&highest,4);
 wire::Entry entry;entry.entity=ptr(&entity);entry.serial=9;std::memcpy(storage.data()+sizeof(wire::Entry),&entry,sizeof(entry));auto net=ptr(&entity.networkTable);std::memcpy(reinterpret_cast<void*>(uintptr_t(listAddress+0x28+8)),&net,4);
 source_entities::list=reinterpret_cast<void*>(uintptr_t(listAddress));source_entities::bound=true;source_entities::wasEnabled=true;source_entities::enabled=true;source_entities::epoch=7;source_entities::nextCapture=0;
 GameCameraFrame camera;camera.active=1;camera.epoch=7;camera.tick=1000;source_entities::onFrame(camera);auto frame=std::atomic_load(&source_entities::snapshot);
 check(frame->records.size()==1);const auto& r=frame->records[0];check(r.className=="CMockEntity"&&r.classId==42&&r.handle==0x9001&&r.networkOrigin&&r.networkOrigin->y==20&&r.team==2&&r.activeWeapon==0x50005);
 auto json=source_entities::jsonFrame(*frame);check(!json["model_lookup_available"].get<bool>()&&json["binding_checks"].is_array());
 check(json["pose_space"]=="network_raw"&&json["objects"][0]["origin"][2]==30);
 entry.entity=0;std::memcpy(storage.data()+sizeof(wire::Entry),&entry,sizeof(entry));camera.tick=1200;source_entities::onFrame(camera);check(std::atomic_load(&source_entities::snapshot)->records.empty());
 source_entities::list=reinterpret_cast<void*>(32);camera.tick=1400;source_entities::onFrame(camera);check(source_entities::faulted); // Read failure must not crash or call the address.
 source_entities::enabled=false;camera.tick=1600;source_entities::onFrame(camera);check(!source_entities::bound&&std::atomic_load(&source_entities::snapshot)->records.empty());
 std::cout<<"Passed: real diagnostic producer over mock x86 memory, metadata/schema/raw origin/handles, JSON, deletion, failed reads and disable reset\n";
}
