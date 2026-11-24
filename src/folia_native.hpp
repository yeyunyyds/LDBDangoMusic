#pragma once
#include "folia_protocol.hpp"
#include <windows.h>
#include <filesystem>
#include <fstream>
namespace dango::folia {
class Native {
 Connection current;State latest;
 static Connection discover(){
  wchar_t appdata[32768]{};auto n=GetEnvironmentVariableW(L"APPDATA",appdata,32768);
  if(!n||n>=32768)throw std::runtime_error("Folia APPDATA unavailable");
  for(const auto* name:{L"Folia",L"folia-major"}){
   auto path=std::filesystem::path(appdata)/name/L"stage"/L"connection.json";
   std::error_code ec;auto size=std::filesystem::file_size(path,ec);if(ec||size>16384||size==0)continue;
   std::ifstream file(path,std::ios::binary);std::string data(static_cast<size_t>(size),'\0');
   if(!file.read(data.data(),static_cast<std::streamsize>(data.size())))continue;
   Connection info;try{info=connection(Json::parse(data));}catch(...){continue;}
   HANDLE proc=OpenProcess(SYNCHRONIZE,FALSE,info.pid);if(!proc)continue;
   auto alive=WaitForSingleObject(proc,0)==WAIT_TIMEOUT;CloseHandle(proc);
   if(alive)return info;
  }
  throw std::runtime_error("Folia: enable Stage API in the desktop app");
 }
public:
 void reset(){current={};latest={};}
 template<class Get,class Clock> State status(Get get,Clock clock){
  try{
   if(!current.port)current=discover();
   const std::wstring header=L"Authorization: Bearer "+std::wstring(current.token.begin(),current.token.end())+L"\r\n";
   auto j=get(L"/stage/player/status",65536,current.port,header.c_str());
   latest=playback(j,clock());return latest;
  }catch(...){reset();throw std::runtime_error("Folia Stage API unavailable; check Stage mode and connection");}
 }
 template<class Get> Timeline lyrics(Get get){
  if(latest.key.empty())return {};
  return folia::lyrics(get(L"/v1/lyric",2*1024*1024,32109,nullptr),latest);
 }
};
}
