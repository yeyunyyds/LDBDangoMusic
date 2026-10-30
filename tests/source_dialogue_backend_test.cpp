#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include "source_dialogue.hpp"
#include <iostream>
#include <stdexcept>
using namespace dango;
void check(bool b){if(!b)throw std::runtime_error("Dialogue backend mock regression");}
unsigned forwarded=0;
__declspec(noinline) bool __fastcall forward(void*,void*,int,void*){++forwarded;return false;}
uint32_t ptr(const void* p){return uint32_t(reinterpret_cast<uintptr_t>(p));}
int main(){
 static_assert(sizeof(void*)==4,"x86 mock only");source_dialogue::original=reinterpret_cast<source_dialogue::Dispatch>(&forward);source_dialogue::captionId=10;source_dialogue::directId=11;source_dialogue::enabled=true;source_dialogue::session=1;
 uint8_t payload[]={0x78,0x56,0x34,0x12,25,0};dialogue::BitRead input;input.bits=48;input.bytes=6;input.data=ptr(payload);input.next=ptr(payload)+2;input.end=ptr(payload)+6;input.available=16;auto saved=input;
 check(!source_dialogue::dispatchHook(nullptr,nullptr,10,&input)&&forwarded==1&&std::memcmp(&input,&saved,sizeof(input))==0);
 check(source_dialogue::queue.size()==1&&source_dialogue::queue.front().hash==0x12345678&&source_dialogue::queue.front().duration==2500);source_dialogue::queue.clear();
 input.overflow=1;source_dialogue::dispatchHook(nullptr,nullptr,11,&input);check(forwarded==2&&source_dialogue::queue.size()==1&&!source_dialogue::queue.front().valid);source_dialogue::queue.clear();
 source_dialogue::enabled=false;source_dialogue::dispatchHook(nullptr,nullptr,10,&input);check(forwarded==3&&source_dialogue::queue.empty());
 // Actual worker-side pipeline over immutable entity/camera fixtures, no game code.
 auto tick=GetTickCount64();GameCameraFrame camera;camera.epoch=7;camera.active=1;camera.tick=tick;std::atomic_store(&source_camera::snapshot,std::make_shared<const GameCameraFrame>(camera));
 inventory::Frame frame;frame.epoch=7;frame.tick=tick;inventory::Record player;player.handle=0x9005;player.className="CTerrorPlayer";player.character=5;player.team=2;player.health=100;player.lifeState=0;player.dormant=false;player.parent=0xffffffffu;player.networkOrigin=Vec3{100,0,0};player.networkAngles=Vec3{};frame.records.push_back(player);std::atomic_store(&source_entities::snapshot,std::make_shared<const inventory::Frame>(frame));
 auto directory=std::filesystem::temp_directory_path()/("dango-dialogue-mock-"+std::to_string(GetCurrentProcessId()));source_dialogue::setEnabled(true);source_dialogue::sample(5);source_dialogue::drain(directory);auto state=std::atomic_load(&source_dialogue::snapshot);check(state->timeline.lines.size()==1&&state->timeline.lines[0].handle==0x9005&&state->timeline.lines[0].synthetic);
 auto oldSession=state->session;source_dialogue::setEnabled(false);source_dialogue::setEnabled(true);check(source_dialogue::session.load()!=oldSession);source_dialogue::drain(directory);check(std::atomic_load(&source_dialogue::snapshot)->timeline.lines.empty());
 source_dialogue::setEnabled(false);source_dialogue::exportRequested=true;source_dialogue::drain(directory);std::ifstream log(directory/"dialogue.jsonl");std::string line;bool synthetic=false,offStatus=false;while(std::getline(log,line)){auto row=nlohmann::json::parse(line);synthetic|=row.value("synthetic",false);offStatus|=row.value("event","")=="status"&&!row.value("enabled",true);}check(synthetic&&offStatus);std::filesystem::remove_all(directory);
 source_dialogue::stop();std::cout<<"Passed: original message forwarding, non-consuming reads, malformed/off pass-through, synthetic worker match, toggle generation and disabled export\n";
}
