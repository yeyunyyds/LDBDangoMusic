#include "hud_menu_policy.hpp"
#include <stdexcept>
#include <vector>
#include <iostream>
using namespace dango;
void check(bool v){if(!v)throw std::runtime_error("L4N menu stack regression");}
int main(){
 std::vector<const char*> stack{"root"};auto click=[&](uintptr_t id){auto child=hud::callbackResult(id,"child");if(child)stack.push_back(child);};
 click(20);check(stack.size()==2);for(int i=0;i<100;++i)click(1);check(stack.size()==2);stack.pop_back();check(stack.size()==1);
 for(auto id:{2,3,4,18,28,32,100,103,500,584,585,586,587,588,589,590,591,600,620,623,626,700,718,719,724,726,750,40002,42304,801,802,851,852,853,854,855,856,861,868,820,821,822,823,824,825,830,831,832,833,840,846,704,709,712,717,2004,2006,2098,2099,2209,2310,30002}){check(!hud::opensChild(id));click(id);check(stack.size()==1);}
 for(auto id:{10,20,21,22,23,24,27,30,31,33,34,35,36,37,38,39,40,41,711,720,721,722,723,730,741,800,810,811,812,2000,2001,2002,2003,2110,3103,3202,400})check(hud::opensChild(id));
 std::cout<<"Passed: repeated toggles/adjustments/save/preview exit never push L4N menus; only navigation opens children\n";
}
