#include "sync/lx_control.hpp"
#include <iostream>
using namespace dango;
int main(){try{
 std::string row;std::getline(std::cin,row);auto initial=nlohmann::json::parse(row);int64_t tick=initial.at("tick");auto s=status(initial.at("status"),tick);lx::Command c;c.id=1;c.expectedTrack=s.key;c.action=lx::Action(initial.at("action").get<int>());c.position=initial.value("position",int64_t(0));lx::Executor executor;
 auto result=executor.begin(c,s,tick,[&](const std::string& path){std::cout<<nlohmann::json{{"request",path}}.dump()<<std::endl;if(!std::getline(std::cin,row)||nlohmann::json::parse(row).at("body")!="OK")throw std::runtime_error("HTTP command not acknowledged");});
 std::cout<<nlohmann::json{{"accepted",result.accepted}}.dump()<<std::endl;if(!result.accepted)return 0;
 while(std::getline(std::cin,row)){auto j=nlohmann::json::parse(row);tick=j.at("tick");auto observed=executor.observe(status(j.at("status"),tick),tick);std::cout<<nlohmann::json{{"verified",observed.verified},{"done",!executor.busy()},{"message",observed.message}}.dump()<<std::endl;if(!executor.busy())break;}
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
