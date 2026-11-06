#pragma once
#include "../build_features.hpp"
#include <functional>
#include <windows.h>
#include <memory>
#include <random>
#include <cstddef>
#include "protocol.hpp"
namespace dango::sync::steam {
inline bool readable(const void* p,size_t n){MEMORY_BASIC_INFORMATION m{};return p&&VirtualQuery(p,&m,sizeof(m))==sizeof(m)&&m.State==MEM_COMMIT&&!(m.Protect&(PAGE_GUARD|PAGE_NOACCESS))&&reinterpret_cast<uintptr_t>(p)+n>=reinterpret_cast<uintptr_t>(p)&&reinterpret_cast<uintptr_t>(p)+n<=reinterpret_cast<uintptr_t>(m.BaseAddress)+m.RegionSize;}
inline bool executable(const void* p){MEMORY_BASIC_INFORMATION m{};if(!p||VirtualQuery(p,&m,sizeof(m))!=sizeof(m)||m.State!=MEM_COMMIT||(m.Protect&PAGE_GUARD))return false;auto protection=m.Protect&255;return protection==PAGE_EXECUTE||protection==PAGE_EXECUTE_READ||protection==PAGE_EXECUTE_READWRITE||protection==PAGE_EXECUTE_WRITECOPY;}
template<class R,class... Args>R invoke(void* object,size_t slot,Args... args){if(!readable(object,sizeof(void*)))throw std::runtime_error("Steam interface unreadable");auto table=*reinterpret_cast<void***>(object);if(!readable(table,(slot+1)*sizeof(void*))||!executable(table[slot]))throw std::runtime_error("Steam method unavailable");using Fn=R(__thiscall*)(void*,Args...);return reinterpret_cast<Fn>(table[slot])(object,args...);}
inline uint64_t nonce(){std::random_device random;uint64_t n=(uint64_t(random())<<32)^uint64_t(random());return n?n:1;}
// Minimal interoperability declarations, not a replacement Steam SDK.
// Windows x86 MSVC CSteamID class return uses caller-provided return storage.
inline Peer legacyUserId(void* user){
 if(!readable(user,sizeof(void*)))throw std::runtime_error("Steam user object unavailable");auto table=*reinterpret_cast<void***>(user);if(!readable(table,3*sizeof(void*))||!executable(table[2]))throw std::runtime_error("Steam GetSteamID unavailable");void* method=table[2];uint64_t result=UINT64_MAX;uint32_t low=0,high=0;
 // Explicitly restore the stack: supported clients may return the 8-byte ID in
 // EDX:EAX or via MSVC's hidden class-return pointer. Never assume GCC's class ABI.
#ifdef _MSC_VER
 __asm { mov ebx, esp
         lea eax, result
         push eax
         mov ecx, user
         call method
         mov esp, ebx
         mov low, eax
         mov high, edx }
#else
 __asm__ volatile("movl %%esp, %%ebx\n\tpushl %[storage]\n\tmovl %[object], %%ecx\n\tcall *%[method]\n\tmovl %%ebx, %%esp" : "=a"(low),"=d"(high) : [storage]"r"(&result),[object]"r"(user),[method]"r"(method) : "ebx","ecx","memory","cc");
#endif
 if(result!=UINT64_MAX)return result;return (uint64_t(high)<<32)|low;
}
inline Peer userId(HMODULE api,void* user){using Flat=uint64_t(__cdecl*)(void*);auto flat=reinterpret_cast<Flat>(GetProcAddress(api,"SteamAPI_ISteamUser_GetSteamID"));return flat?flat(user):legacyUserId(user);}
#pragma pack(push,8)
struct MessagePrefix {void* data;int size;uint32_t connection;unsigned char identity[136];alignas(8)int64_t user,time,number;void(__cdecl* freeData)(void*);void(__cdecl* release)(void*);};
#pragma pack(pop)
static_assert(sizeof(void*)==4,"Steam adapter targets the x86 game");
static_assert(offsetof(MessagePrefix,user)==152&&offsetof(MessagePrefix,release)==180,"SteamNetworkingMessage Windows x86 ABI changed");
class Transport final:public sync::Transport {
 HMODULE api=nullptr;void* client=nullptr;void* networking=nullptr;void* sockets=nullptr;int pipe=0,user=0;Peer identity=0;std::set<Peer> permitted;std::deque<Packet> inbox;uint64_t probeCookie=0;int64_t probeDeadline=0;uint32_t pairA=0,pairB=0;int64_t pairDeadline=0;std::string pairPayload;int64_t nextAccept=0;
 const int Channel;std::function<bool()> permission;
 bool authorized()const{return permission&&permission();}
 void* getUser(int hUser,int hPipe){for(auto version:{"SteamUser023","SteamUser022","SteamUser021","SteamUser020","SteamUser019","SteamUser018","SteamUser017"})if(auto p=invoke<void*>(client,5,hUser,hPipe,version))return p;return nullptr;}
 void closePair(){if(!sockets)return;for(auto p:{pairA,pairB})if(p)invoke<bool>(sockets,5,p,0,"Dango local test complete",false);pairA=pairB=0;}
public:
 explicit Transport(int channel=45631,std::function<bool()> allowed={}):Channel(channel),permission(std::move(allowed)){if(channel<1024)throw std::runtime_error("Reserve a non-game Steam virtual channel >=1024");}
 std::string binding="Steam unavailable",selfResult="Not run",pairResult="Not run";uint64_t dropped=0;
 ~Transport(){try{closePair();if(networking){for(auto p:permitted)invoke<bool>(networking,5,p,Channel);if(identity&&probeCookie)invoke<bool>(networking,5,identity,Channel);}/* Borrowed game user/pipe are never released by this module. */}catch(...){}}
 void connect(){
  if(!authorized())throw std::runtime_error("Steam networking disabled");if(networking)return;api=GetModuleHandleW(L"steam_api.dll");if(!api)throw std::runtime_error("Game steam_api.dll not loaded");using HandleFn=int(__cdecl*)();auto gameUser=reinterpret_cast<HandleFn>(GetProcAddress(api,"SteamAPI_GetHSteamUser"));auto gamePipe=reinterpret_cast<HandleFn>(GetProcAddress(api,"SteamAPI_GetHSteamPipe"));if(!gameUser||!gamePipe||!gameUser()||!gamePipe())throw std::runtime_error("Game Steam context not initialized");
  using Factory=void*(__cdecl*)(const char*);auto factory=reinterpret_cast<Factory>(GetProcAddress(api,"SteamInternal_CreateInterface"));if(!factory)throw std::runtime_error("Steam interface factory unavailable");
  std::string version;for(auto v:{"SteamClient020","SteamClient019","SteamClient018","SteamClient017"})if((client=factory(v))){version=v;break;}if(!client)throw std::runtime_error("No supported SteamClient interface");
  pipe=gamePipe();user=gameUser();
  auto steamUser=getUser(user,pipe);if(!steamUser||!invoke<bool>(steamUser,1))throw std::runtime_error("Steam user not logged on");identity=userId(api,steamUser);if(!steamId(identity))throw std::runtime_error("SteamID64 unavailable or unsupported return ABI");
  for(auto v:{"SteamNetworking006","SteamNetworking005"})if((networking=invoke<void*>(client,16,user,pipe,v))){binding=version+" / "+v;break;}if(!networking)throw std::runtime_error("Legacy Steam P2P networking unavailable");
  #if DANGO_DIAGNOSTICS
  sockets=invoke<void*>(client,12,user,pipe,"SteamNetworkingSockets012");
  #endif
 }
 Peer self()const override{return identity;}
 void allow(const std::set<Peer>& peers){if(!authorized())return;for(auto old:permitted)if(!peers.count(old))invoke<bool>(networking,5,old,Channel);permitted=peers;}
 bool send(Peer to,const std::string& bytes)override{if(!authorized()||!networking||bytes.size()>4096||(!permitted.count(to)&&to!=identity))return false;invoke<bool>(networking,3,to);return invoke<bool>(networking,0,to,static_cast<const void*>(bytes.data()),uint32_t(bytes.size()),2,Channel);}
 bool receive(Packet& p)override{if(inbox.empty())return false;p=std::move(inbox.front());inbox.pop_front();return true;}
 #if DANGO_DIAGNOSTICS
 void selfTest(int64_t now){probeCookie=nonce();Message m;m.kind="probe";m.sender=identity;m.cookie=probeCookie;m.t0=now;probeDeadline=now+4000;selfResult=send(identity,encode(m))?"Self Steam P2P sent; awaiting echo":"Self Steam P2P send rejected (not simulated)";if(selfResult.find("rejected")!=std::string::npos)probeDeadline=0;}
 void socketPairTest(int64_t now){
  closePair();if(!sockets){pairResult="UNSUPPORTED: SteamNetworkingSockets012 not exposed by game";return;}
  if(!invoke<bool>(sockets,19,&pairA,&pairB,true,static_cast<const void*>(nullptr),static_cast<const void*>(nullptr))||!pairA||!pairB){pairResult="CreateSocketPair failed";closePair();return;}
  Message m;m.sender=identity;m.kind="state";m.room=nonce();m.seq=1;m.t2=now;m.position=30000;m.track={"Local socket-pair test","No music resources",120000};pairPayload=encode(m);int64_t number=0;
  if(invoke<int>(sockets,11,pairA,static_cast<const void*>(pairPayload.data()),uint32_t(pairPayload.size()),8,&number)!=1){pairResult="Socket pair send failed";closePair();return;}pairDeadline=now+4000;pairResult="Real Steam local socket pair; awaiting receive";
 }
 #endif
 void pump(int64_t now){
  if(!authorized()||!networking)return;if(now>=nextAccept){for(auto p:permitted)invoke<bool>(networking,3,p);nextAccept=now+300;}
  for(int budget=0;budget<16;++budget){uint32_t size=0;if(!invoke<bool>(networking,1,&size,Channel))break;if(size>1024*1024)throw std::runtime_error("Plugin P2P channel packet exceeds 1MiB");std::vector<char> data(std::max<uint32_t>(size,1));uint32_t read=0;uint64_t from=0;if(!invoke<bool>(networking,2,static_cast<void*>(data.data()),uint32_t(data.size()),&read,&from,Channel))break;if(read>4096||read>data.size()){++dropped;continue;}Packet packet{from,std::string(data.data(),read)};
   #if DANGO_DIAGNOSTICS
   if(from==identity&&probeDeadline){try{auto m=decode(packet);if(m.cookie==probeCookie&&m.kind=="probe"){m.kind="echo";send(identity,encode(m));continue;}if(m.cookie==probeCookie&&m.kind=="echo"){selfResult="PASS: real SteamID P2P echo RTT="+std::to_string(now-m.t0)+"ms";probeDeadline=0;continue;}}catch(...){}}
   #endif
   if(permitted.count(from)&&inbox.size()<64)inbox.push_back(std::move(packet));else ++dropped;
  }
  #if DANGO_DIAGNOSTICS
  if(probeDeadline&&now>=probeDeadline){selfResult="TIMEOUT: Steam self P2P did not echo; no synthetic pass";probeDeadline=0;}
  if(pairB){MessagePrefix* message=nullptr;auto count=invoke<int>(sockets,14,pairB,&message,1);if(count==1&&message){bool good=false;if(readable(message,sizeof(MessagePrefix))&&message->size>=0&&message->size<=4096&&readable(message->data,size_t(message->size))&&executable(reinterpret_cast<void*>(message->release))){try{Packet p{identity,std::string(static_cast<const char*>(message->data),size_t(message->size))};good=p.bytes==pairPayload&&decode(p).position==30000;}catch(...){}message->release(message);}pairResult=good?"PASS: real Steam CreateSocketPair A->B decoded 30000ms":"FAIL: Steam socket-pair payload invalid";closePair();}else if(now>=pairDeadline){pairResult="TIMEOUT: Steam socket pair";closePair();}}
  #endif
 }
};
}
