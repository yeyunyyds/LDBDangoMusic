#pragma once
#include <windows.h>
#include <winhttp.h>
#include <filesystem>
#include <future>
#include <atomic>
#include <chrono>
#include "netease_elog.hpp"
namespace dango::netease {
inline int64_t tickNow(){return static_cast<int64_t>(GetTickCount64());}
inline int64_t wallNow(){return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();}
struct Handle{HANDLE h=INVALID_HANDLE_VALUE;explicit Handle(HANDLE value):h(value){}~Handle(){if(h&&h!=INVALID_HANDLE_VALUE)CloseHandle(h);}Handle(const Handle&)=delete;};
struct HttpHandle{HINTERNET h=nullptr;explicit HttpHandle(HINTERNET value):h(value){if(!h)throw std::runtime_error("Netease HTTP handle failed");}~HttpHandle(){WinHttpCloseHandle(h);}HttpHandle(const HttpHandle&)=delete;};
inline nlohmann::json api(const std::wstring& path,const std::atomic<bool>* stopping){
 HttpHandle session(WinHttpOpen(L"Mozilla/5.0 L4D2DangoMusic-native",WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,WINHTTP_NO_PROXY_NAME,WINHTTP_NO_PROXY_BYPASS,0));WinHttpSetTimeouts(session.h,1500,1500,1500,1500);
 HttpHandle connection(WinHttpConnect(session.h,L"music.163.com",INTERNET_DEFAULT_HTTPS_PORT,0));HttpHandle request(WinHttpOpenRequest(connection.h,L"GET",path.c_str(),nullptr,WINHTTP_NO_REFERER,WINHTTP_DEFAULT_ACCEPT_TYPES,WINHTTP_FLAG_SECURE));
 DWORD redirect=WINHTTP_OPTION_REDIRECT_POLICY_NEVER;WinHttpSetOption(request.h,WINHTTP_OPTION_REDIRECT_POLICY,&redirect,sizeof(redirect));
 if(stopping->load())throw std::runtime_error("Netease request cancelled");
 if(!WinHttpSendRequest(request.h,L"Referer: https://music.163.com/\r\n",DWORD(-1),WINHTTP_NO_REQUEST_DATA,0,0,0)||!WinHttpReceiveResponse(request.h,nullptr))throw std::runtime_error("Netease HTTP connection failed");
 DWORD code=0,length=sizeof(code);if(!WinHttpQueryHeaders(request.h,WINHTTP_QUERY_STATUS_CODE|WINHTTP_QUERY_FLAG_NUMBER,WINHTTP_HEADER_NAME_BY_INDEX,&code,&length,WINHTTP_NO_HEADER_INDEX)||code!=200)throw std::runtime_error("Netease HTTP status "+std::to_string(code));
 auto deadline=tickNow()+5000;std::string body;char buffer[8192];DWORD count=0;
 while(!stopping->load()&&tickNow()<=deadline){if(!WinHttpReadData(request.h,buffer,sizeof(buffer),&count))throw std::runtime_error("Netease HTTP read failed");if(!count)return nlohmann::json::parse(body);if(body.size()+count>2*1024*1024)throw std::runtime_error("Netease response too large");body.append(buffer,count);}throw std::runtime_error("Netease request cancelled or timed out");
}
struct Lookup {Track song;Timeline lines;std::string error;};
inline Lookup lookup(Track song,const std::atomic<bool>* stopping){Lookup result;result.song=std::move(song);try{
 std::wstring id(result.song.id.begin(),result.song.id.end());
 if(result.song.title.empty()||result.song.duration<=0){auto j=api(L"/api/song/detail?ids=%5B"+id+L"%5D",stopping);if(j.contains("songs")&&j["songs"].is_array()&&!j["songs"].empty()){auto resolved=track(j["songs"][0]);if(resolved.id==result.song.id)result.song=std::move(resolved);}}
 auto j=api(L"/api/song/lyric?id="+id+L"&lv=1&tv=1&yv=1",stopping);result.lines=lyricResponse(j);if(result.lines.empty())result.error="No synchronized lyrics for this song ID";
 }catch(const std::exception& e){result.error=e.what();}return result;}
class Native {
 Parser parser;std::filesystem::path file;uint64_t offset=0,fileIdentity=0;bool loaded=false;uint32_t checkedPid=0;bool processValid=false;int64_t processCheckAt=0,retryAt=0;std::future<Lookup> pending;std::string requestedKey,cachedKey,problem;Timeline cached;const std::atomic<bool>* stopping;void(*log)(const std::string&);
 std::filesystem::path defaultPath(){wchar_t path[32768]{};auto size=GetEnvironmentVariableW(L"LOCALAPPDATA",path,32768);if(!size||size>=32768)return {};return std::filesystem::path(path)/L"NetEase"/L"CloudMusic"/L"cloudmusic.elog";}
 bool alive(int64_t tick){
  if(parser.pid==checkedPid&&tick<processCheckAt)return processValid;checkedPid=parser.pid;processCheckAt=tick+1000;processValid=false;if(!parser.pid)return false;
  Handle process(OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION|SYNCHRONIZE,FALSE,parser.pid));if(!process.h||WaitForSingleObject(process.h,0)!=WAIT_TIMEOUT)return false;
  wchar_t name[32768]{};DWORD size=32768;if(!QueryFullProcessImageNameW(process.h,0,name,&size))return false;auto base=std::filesystem::path(name).filename().wstring();std::transform(base.begin(),base.end(),base.begin(),[](wchar_t c){return c>=L'A'&&c<=L'Z'?c+32:c;});if(base!=L"cloudmusic.exe")return false;
  FILETIME born{},exit{},kernel{},user{},current{};if(!GetProcessTimes(process.h,&born,&exit,&kernel,&user))return false;GetSystemTimeAsFileTime(&current);ULARGE_INTEGER b{},n{};b.LowPart=born.dwLowDateTime;b.HighPart=born.dwHighDateTime;n.LowPart=current.dwLowDateTime;n.HighPart=current.dwHighDateTime;auto creation=tick-int64_t((n.QuadPart-b.QuadPart)/10000);if(parser.eventTick+1000<creation)return false;processValid=true;return true;
 }
 void read(){
  if(file.empty())file=defaultPath();if(file.empty())throw std::runtime_error("Netease elog: LOCALAPPDATA unavailable");
  Handle handle(CreateFileW(file.c_str(),GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr));if(handle.h==INVALID_HANDLE_VALUE){parser.reset();loaded=false;throw std::runtime_error("Netease elog missing or unreadable; play a song in the desktop client");}
  BY_HANDLE_FILE_INFORMATION info{};LARGE_INTEGER size{};if(!GetFileInformationByHandle(handle.h,&info)||!GetFileSizeEx(handle.h,&size)||size.QuadPart<0)throw std::runtime_error("Cannot stat Netease elog");uint64_t identity=(uint64_t(info.nFileIndexHigh)<<32)|info.nFileIndexLow;
  if(!loaded||identity!=fileIdentity||uint64_t(size.QuadPart)<offset){parser.reset();cached.clear();cachedKey.clear();fileIdentity=identity;offset=uint64_t(size.QuadPart)>8*1024*1024?uint64_t(size.QuadPart)-8*1024*1024:0;parser.discard=offset!=0;loaded=true;processCheckAt=0;}
  LARGE_INTEGER start{};start.QuadPart=static_cast<LONGLONG>(offset);if(!SetFilePointerEx(handle.h,start,nullptr,FILE_BEGIN))throw std::runtime_error("Cannot seek Netease elog");
  size_t budget=8*1024*1024;char buffer[16384];auto tick=tickNow(),wall=wallNow();while(offset<uint64_t(size.QuadPart)&&budget&&!stopping->load()){DWORD count=0;auto amount=DWORD(std::min<uint64_t>({sizeof(buffer),uint64_t(size.QuadPart)-offset,budget}));if(!ReadFile(handle.h,buffer,amount,&count,nullptr))throw std::runtime_error("Cannot read Netease elog");if(!count)break;parser.feed(buffer,count,tick,wall);offset+=count;budget-=count;}
 }
 std::string key()const{return parser.song.id.empty()?std::string{}:std::to_string(parser.pid)+"/"+parser.song.id;}
public:
 explicit Native(const std::atomic<bool>* stop,void(*logger)(const std::string&)):stopping(stop),log(logger){}
 State status(){
  read();auto tick=tickNow();if(!parser.song.id.empty()&&!alive(tick)){parser.reset();cached.clear();cachedKey.clear();problem="Netease elog: recorded player is closed; waiting for a fresh playback event";}
  auto current=key();
  if(current!=cachedKey){cached.clear();cachedKey=current;retryAt=0;if(!current.empty()){problem.clear();if(log)log("Netease elog track pid="+std::to_string(parser.pid)+" id="+parser.song.id+" title="+parser.song.title+" duration_ms="+std::to_string(parser.song.duration));}}
  if(pending.valid()&&pending.wait_for(std::chrono::milliseconds(0))==std::future_status::ready){auto result=pending.get();if(requestedKey==current&&!current.empty()){cached=std::move(result.lines);if(result.song.id==parser.song.id&&!result.song.title.empty()&&result.song.duration>0)parser.song=std::move(result.song);problem=result.error;retryAt=tick+30000;if(log)log("Netease native lookup id="+parser.song.id+" lines="+std::to_string(cached.size())+" result="+(problem.empty()?"OK":problem));}}
  if(!current.empty()&&cached.empty()&&!pending.valid()&&tick>=retryAt){requestedKey=current;auto song=parser.song;auto cancel=stopping;pending=std::async(std::launch::async,[song,cancel](){return lookup(song,cancel);});}
  auto state=parser.state(tick);if(!problem.empty())state.message=problem;return state;
 }
 Timeline lyrics()const{return cached;}
};
}
