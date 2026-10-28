#include "core.hpp"
#include <fstream>
#include <iostream>
int main(int argc,char** argv){if(argc!=4)return 2;try{std::ifstream a(argv[1]),b(argv[2]);auto s=dango::status(nlohmann::json::parse(a),1000);auto t=dango::lyrics(nlohmann::json::parse(b));if(!s.connected||s.key.empty()||t.size()!=3||t[0].words.size()!=2||t[0].translation!="Start")return 1;std::cout<<s.title<<'|'<<(s.playing?"playing":"paused")<<'|'<<s.position<<'\n';return s.title==argv[3]?0:1;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
