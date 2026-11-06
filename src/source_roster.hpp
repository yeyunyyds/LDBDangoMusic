#pragma once
#include "source_camera.hpp"
#include "sync/roster.hpp"
namespace source_roster {
inline std::atomic<bool> enabled{false};
inline std::shared_ptr<const dango::sync::Roster> snapshot=std::make_shared<dango::sync::Roster>();
inline uint64_t nextCapture=0,generation=1;inline uint32_t lastEpoch=0;inline HMODULE checkedModule=nullptr;inline bool validated=false;
// Static inspection of the uploaded engine image: GetPlayerInfo copies/clears
// exactly 0x90 bytes. Names at +8/+0x54 are 32 bytes; userID is +0x28.
inline bool contract(){
 using namespace source_camera;if(checkedModule==engineModule)return validated;checkedModule=engineModule;validated=false;if(!engineModule||!engine||!hooked)return false;
 if(!matchesImage(engineModule,EngineTimestamp,EngineImageSize,"roster engine.dll"))return false;
 struct Method{unsigned slot;uint32_t rva;unsigned char prefix[12];};
 static constexpr Method methods[]={
 {8,0x5f4d0,{0x55,0x8b,0xec,0x56,0x8b,0x75,0x08,0x4e,0xe8,0xc3,0x37,0x02}},
 {12,0x5d030,{0x6a,0xff,0xe8,0x39,0x5c,0x02,0x00,0x8b,0x80,0xd8,0x00,0x00}},
 {21,0x5d290,{0xe8,0x0b,0x5a,0x02,0x00,0x8b,0x80,0x58,0x01,0x00,0x00,0xc3}}};
 for(auto& m:methods){auto code=reinterpret_cast<const unsigned char*>(engineModule)+m.rva;if(!readable(code,sizeof(m.prefix))||!executable(code)||std::memcmp(code,m.prefix,sizeof(m.prefix))){log("Roster contract rejected: engine method bytes differ");return false;}}
 validated=true;log("Roster contract verified: playerInfo[8] size=144, localPlayer[12], maxClients[21]; game-thread capture");return true;
}
inline dango::sync::Peer readPlayer(int index){
 using namespace source_camera;alignas(8)unsigned char guarded[16+144+16];std::memset(guarded,0xa5,sizeof(guarded));std::memset(guarded+16,0,144);
 using Fn=bool(__thiscall*)(void*,int,void*);auto fn=reinterpret_cast<Fn>(reinterpret_cast<uintptr_t>(engineModule)+0x5f4d0);if(!fn(engine,index,guarded+16))return 0;
 for(int i=0;i<16;++i)if(guarded[i]!=0xa5||guarded[160+i]!=0xa5)throw std::runtime_error("PlayerInfo output canary changed");
 return dango::sync::playerSteamId(guarded+16,144);
}
inline void capture(const dango::GameCameraFrame& frame){
 auto previous=std::atomic_load(&snapshot);bool active=enabled.load()&&frame.active&&source_camera::hooked;
 if(!active){if(previous->active){dango::sync::Roster out;out.tick=int64_t(frame.tick);out.generation=++generation;out.status="Not in an active game";std::atomic_store(&snapshot,std::make_shared<const dango::sync::Roster>(out));}nextCapture=0;return;}
 if(frame.tick<nextCapture&&frame.epoch==lastEpoch)return;nextCapture=frame.tick+1000;
 dango::sync::Roster out;out.tick=int64_t(frame.tick);out.generation=generation;try{
 if(!contract())throw std::runtime_error("Verified Source player interface unavailable");using Int= int(__thiscall*)(void*);auto maxClients=reinterpret_cast<Int>(reinterpret_cast<uintptr_t>(source_camera::engineModule)+0x5d290)(source_camera::engine);auto localIndex=reinterpret_cast<Int>(reinterpret_cast<uintptr_t>(source_camera::engineModule)+0x5d030)(source_camera::engine);
 if(maxClients<1||maxClients>64||localIndex<1||localIndex>maxClients)throw std::runtime_error("Source player indices unavailable");out.local=readPlayer(localIndex);if(!dango::sync::steamId(out.local))throw std::runtime_error("Local game SteamID unavailable");
 std::vector<std::pair<int,dango::sync::Peer>> ordered{{localIndex,out.local}};std::set<dango::sync::Peer> seen{out.local};for(int i=1;i<=maxClients&&ordered.size()<4;++i){if(i==localIndex)continue;auto id=readPlayer(i);if(id&&seen.insert(id).second)ordered.push_back({i,id});}std::sort(ordered.begin(),ordered.end());for(auto& player:ordered)out.players.push_back(player.second);
 out.active=true;out.status="Current game humans="+std::to_string(out.players.size())+"; discovery limited to 3 other players";
 if(previous->players!=out.players||previous->local!=out.local||lastEpoch!=frame.epoch||!previous->active)out.generation=++generation;
 }catch(const std::exception& e){out.status=e.what();out.generation=++generation;}
 lastEpoch=frame.epoch;std::atomic_store(&snapshot,std::make_shared<const dango::sync::Roster>(std::move(out)));
}
}
