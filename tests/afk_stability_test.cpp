#include "auto_music_source.hpp"
#include "config_save_policy.hpp"
#include <cassert>
#include <stdexcept>
using namespace dango;
int main(){
 using autosource::Probe;
 std::array<Probe,3> p{{{1,true,false},{8,true,true},{3,true,false}}};
 assert(autosource::choose(p,1)==8);
 p[0].playing=true;assert(autosource::choose(p,8)==8);assert(autosource::choose(p,0)==1);
 p[1].connected=false;p[1].playing=false;assert(autosource::choose(p,8)==1);
 p[0].playing=false;p[2].playing=true;assert(autosource::choose(p,1)==3);
 p[0].connected=false;p[2].connected=false;p[2].playing=false;assert(autosource::choose(p,3)==0);
 int writes=0,warnings=0;
 auto warn=[&](const std::string&){++warnings;};
 for(int i=0;i<2;++i)savepolicy::commit([]{},[&]{++writes;},warn);
 savepolicy::commit([]{throw std::runtime_error("File exists");},[&]{++writes;},warn);
 assert(writes==3&&warnings==1);
 bool failed=false;try{savepolicy::commit([]{},[]{throw std::runtime_error("replace failed");},warn);}catch(...){failed=true;}
 assert(failed&&writes==3&&warnings==1);
}
