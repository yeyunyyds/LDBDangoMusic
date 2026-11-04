#pragma once
#include <windows.h>
#include <filesystem>
#include <fstream>
#include "media_protocol.hpp"
namespace dango::media {
class Bridge {
 HANDLE process=nullptr;int64_t retryAt=0;int requested=-1;uint32_t parent=GetCurrentProcessId();
 std::filesystem::path folder;
 void request(int source){if(requested==source)return;auto path=folder/("media-request-"+std::to_string(parent)+".json");auto temp=path;temp+=L".tmp";{std::ofstream f(temp,std::ios::binary);f<<nlohmann::json{{"source",source}}.dump();if(!f)throw std::runtime_error("Media bridge request write failed");}if(!MoveFileExW(temp.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))throw std::runtime_error("Media bridge request replace failed");requested=source;}
public:
 ~Bridge(){if(process){TerminateProcess(process,0);WaitForSingleObject(process,500);CloseHandle(process);}if(!folder.empty()){std::error_code ec;std::filesystem::remove(folder/("media-state-"+std::to_string(parent)+".json"),ec);std::filesystem::remove(folder/("media-request-"+std::to_string(parent)+".json"),ec);}}
 void ensure(const std::filesystem::path& directory,int source,int64_t tick){
 folder=directory;std::filesystem::create_directories(folder);request(source);
 if(process&&WaitForSingleObject(process,0)==WAIT_TIMEOUT)return;
 if(process){CloseHandle(process);process=nullptr;}if(tick<retryAt)return;retryAt=tick+10000;
 auto executable=folder/L"DangoMediaBridge.exe";if(!std::filesystem::exists(executable))throw std::runtime_error("DangoMediaBridge.exe missing; install the complete music-platform package");
 FILETIME created{},exit{},kernel{},user{};if(!GetProcessTimes(GetCurrentProcess(),&created,&exit,&kernel,&user))throw std::runtime_error("Cannot identify media parent");ULARGE_INTEGER start{};start.LowPart=created.dwLowDateTime;start.HighPart=created.dwHighDateTime;
 std::wstring command=L"\""+executable.wstring()+L"\" "+std::to_wstring(parent)+L" "+std::to_wstring(start.QuadPart)+L" \""+folder.wstring()+L"\"";
 std::error_code ec;std::filesystem::remove(folder/("media-state-"+std::to_string(parent)+".json"),ec);
 STARTUPINFOW si{};si.cb=sizeof(si);PROCESS_INFORMATION pi{};
 if(!CreateProcessW(executable.c_str(),command.data(),nullptr,nullptr,FALSE,CREATE_NO_WINDOW,nullptr,folder.c_str(),&si,&pi))throw std::runtime_error("Cannot start Windows media bridge (Windows 10 2004+ x64 required)");
 process=pi.hProcess;CloseHandle(pi.hThread);
 }
 nlohmann::json read(int64_t tick){auto path=folder/("media-state-"+std::to_string(parent)+".json");if(!std::filesystem::exists(path)||std::filesystem::file_size(path)>4*1024*1024)throw std::runtime_error("Waiting for Windows media bridge snapshot");std::ifstream file(path,std::ios::binary);auto j=nlohmann::json::parse(file);readStatus(j,tick,parent);if(j.at("selection").get<int>()!=requested)throw std::runtime_error("Waiting for selected music source");return j;}
 State status(int64_t tick){return readStatus(read(tick),tick,parent);}
 Timeline lyrics(int64_t tick){return readLyrics(read(tick));}
};
}
