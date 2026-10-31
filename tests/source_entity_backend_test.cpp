#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include "source_entities.hpp"
#include <iostream>
#include <stdexcept>
using namespace dango;
namespace wire=dango::entitywire;
void check(bool v){if(!v)throw std::runtime_error("Entity diagnostic producer mock failed");}
static wire::ClientClass mockClass,mockHolderClass;
__declspec(noinline) void* __fastcall classGetter(void*,void*){return &mockClass;}
__declspec(noinline) void* __fastcall baseGetter(void* self,void*){return static_cast<char*>(self)-8;}
__declspec(noinline) void* __fastcall holderClassGetter(void*,void*){return &mockHolderClass;}
static const uint8_t notDormant[16]={0x32,0xc0,0xc3}; // decoded only, never executed
uint32_t ptr(const void* p){return uint32_t(reinterpret_cast<uintptr_t>(p));}
struct FakeEntity {uint32_t prefix[2]{};void** networkTable=nullptr;Vec3 origin{10,20,30};int team=2;uint32_t owner=0xffffffff,weaponOwner=0xffffffff,weapon=0x50005,parent=0xffffffff;uint8_t parentAttachment=0;int character=5,health=100;uint8_t life=0,carryable=1;int worldModel=42,effects=0;};
int main(){
 static_assert(sizeof(void*)==4,"Windows x86 only");source_camera::clientModule=GetModuleHandleW(nullptr);
 // Match the independently verified game slots; 2 and 11 are deliberately empty.
 static void* netMethods[12]{};netMethods[1]=reinterpret_cast<void*>(&classGetter);netMethods[10]=reinterpret_cast<void*>(&baseGetter);
 FakeEntity entity;entity.networkTable=netMethods;
 wire::Prop props[13]{};const char* names[]={"m_vecOrigin","m_iTeamNum","m_hOwnerEntity","m_hActiveWeapon","moveparent","m_iParentAttachment","m_hOwner","m_survivorCharacter","m_iHealth","m_lifeState","m_iWorldModelIndex","m_isCarryable","m_fEffects"};const int offsets[]={offsetof(FakeEntity,origin),offsetof(FakeEntity,team),offsetof(FakeEntity,owner),offsetof(FakeEntity,weapon),offsetof(FakeEntity,parent),offsetof(FakeEntity,parentAttachment),offsetof(FakeEntity,weaponOwner),offsetof(FakeEntity,character),offsetof(FakeEntity,health),offsetof(FakeEntity,life),offsetof(FakeEntity,worldModel),offsetof(FakeEntity,carryable),offsetof(FakeEntity,effects)};
 for(int i=0;i<13;++i){props[i].name=ptr(names[i]);props[i].type=i?0:2;props[i].offset=offsets[i];}
 wire::Table recv;recv.props=ptr(props);recv.count=13;recv.name=ptr("DT_MockEntity");mockClass.name=ptr("MovieExplosion");mockClass.table=ptr(&recv);mockClass.id=42;
 // The list layout is the real producer's profiled layout. Binding is skipped
 // only in this mock; no actual game/model-info methods are invoked.
 std::vector<uint8_t> storage(0x15000);auto listAddress=ptr(storage.data()+0x10010);int highest=1;std::memcpy(reinterpret_cast<void*>(uintptr_t(listAddress+0x24)),&highest,4);
 wire::Entry entry;entry.entity=ptr(&entity);entry.serial=9;std::memcpy(storage.data()+sizeof(wire::Entry),&entry,sizeof(entry));auto net=ptr(&entity.networkTable);std::memcpy(reinterpret_cast<void*>(uintptr_t(listAddress+0x28+8)),&net,4);
 source_entities::list=reinterpret_cast<void*>(uintptr_t(listAddress));source_entities::bound=true;source_entities::wasEnabled=true;source_entities::enabled=true;source_entities::epoch=7;source_entities::nextCapture=0;
 GameCameraFrame camera;camera.active=1;camera.epoch=7;camera.tick=1000;source_entities::onFrame(camera);auto frame=std::atomic_load(&source_entities::snapshot);
 check(frame->records.size()==1);const auto& r=frame->records[0];check(r.className=="MovieExplosion"&&r.classId==42&&r.handle==0x9001&&r.networkOrigin&&r.networkOrigin->y==20&&r.team==2&&r.activeWeapon==0x50005);
 auto json=source_entities::jsonFrame(*frame);check(!json["model_lookup_available"].get<bool>()&&json["binding_checks"].is_array());
 check(json["pose_space"]=="network_raw"&&json["objects"][0]["origin"][2]==30);
 check(r.character==5&&r.health==100&&r.lifeState==0);check(r.worldModelIndex==42&&r.isCarryable&&*r.isCarryable&&r.effects==0);
 check(json["diagnostic_build"]==9&&json["objects"][0]["world_model_index"]==42&&json["objects"][0]["is_carryable"]==true);
 check(r.parent==0xffffffffu&&r.parentAttachment==0&&r.weaponOwner==0xffffffffu);
 check(json["objects"][0]["weapon_owner_handle"]==0xffffffffu);
 source_entities::enabled=false;auto request=std::make_shared<const inventory::FollowRequest>(inventory::FollowRequest{true,7,0x9001});std::atomic_store(&source_entities::followRequest,request);camera.tick=1033;source_entities::onFrame(camera);auto selected=std::atomic_load(&source_entities::followSnapshot);check(selected->records.size()==1&&std::atomic_load(&source_entities::snapshot)->tick==1000);
 entity.origin.x=123;entity.weaponOwner=0x9002;camera.tick=1066;source_entities::onFrame(camera);check(std::atomic_load(&source_entities::followSnapshot)->records[0].networkOrigin->x==123&&std::atomic_load(&source_entities::followSnapshot)->records[0].weaponOwner==0x9002);
 // The real target-only producer must include the carry item's holder so
 // release detection works with diagnostics and candidate browsing both off.
 mockClass.name=ptr("CGnome");mockHolderClass=mockClass;mockHolderClass.name=ptr("CTerrorPlayer");source_entities::classes.clear();
 static void* holderMethods[12]{};std::copy(std::begin(netMethods),std::end(netMethods),holderMethods);holderMethods[1]=reinterpret_cast<void*>(&holderClassGetter);holderMethods[7]=netMethods[7]=const_cast<uint8_t*>(notDormant);
 FakeEntity holder;holder.networkTable=holderMethods;holder.weapon=0x9001;
 wire::Entry holderEntry;holderEntry.entity=ptr(&holder);holderEntry.serial=11;std::memcpy(storage.data()+2*sizeof(wire::Entry),&holderEntry,sizeof(holderEntry));auto holderNet=ptr(&holder.networkTable);std::memcpy(reinterpret_cast<void*>(uintptr_t(listAddress+0x28+16)),&holderNet,4);
 entity.owner=entity.weaponOwner=0xb002;camera.tick=1080;source_entities::captureFollow(camera,*request);selected=std::atomic_load(&source_entities::followSnapshot);
 check(selected->records.size()==2&&selected->records[1].handle==0xb002&&selected->records[1].activeWeapon==0x9001);check(!follow::resolve(*selected,*request,7,1080).ready);
 holder.weapon=0x9005;camera.tick=1081;source_entities::captureFollow(camera,*request);selected=std::atomic_load(&source_entities::followSnapshot);check(follow::resolve(*selected,*request,7,1081).ready);
 holderEntry.serial=12;std::memcpy(storage.data()+2*sizeof(wire::Entry),&holderEntry,sizeof(holderEntry));camera.tick=1082;source_entities::captureFollow(camera,*request);selected=std::atomic_load(&source_entities::followSnapshot);check(selected->records.size()==1&&!follow::resolve(*selected,*request,7,1082).ready);
 entity.owner=entity.weaponOwner=0xffffffffu;mockClass.name=ptr("MovieExplosion");source_entities::classes.clear();
 entry.serial=10;std::memcpy(storage.data()+sizeof(wire::Entry),&entry,sizeof(entry));camera.tick=1099;source_entities::onFrame(camera);check(std::atomic_load(&source_entities::followSnapshot)->records.empty());entry.serial=9;std::memcpy(storage.data()+sizeof(wire::Entry),&entry,sizeof(entry));std::atomic_store(&source_entities::followRequest,std::make_shared<const inventory::FollowRequest>());source_entities::enabled=true;
 entry.entity=0;std::memcpy(storage.data()+sizeof(wire::Entry),&entry,sizeof(entry));camera.tick=1200;source_entities::onFrame(camera);check(std::atomic_load(&source_entities::snapshot)->records.empty());
 source_entities::list=reinterpret_cast<void*>(32);camera.tick=1400;source_entities::onFrame(camera);check(source_entities::faulted); // Read failure must not crash or call the address.
 source_entities::enabled=false;camera.tick=1600;source_entities::onFrame(camera);check(!source_entities::bound&&std::atomic_load(&source_entities::snapshot)->records.empty());
 std::cout<<"Passed: real diagnostic producer over mock x86 memory, metadata/schema/raw origin/handles, JSON, deletion, failed reads and disable reset\n";
}
