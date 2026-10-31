#pragma once
#include <cstdint>
namespace dango::hud {
// L4N v2 treats EVERY non-null callback result as a new child menu.
// Only actual forward navigation may return a KeyValues page.
inline bool opensChild(uintptr_t id){
 switch(id){case 10:case 11:case 12:case 13:case 14:case 15:case 16:case 19:case 20:case 21:case 22:case 23:case 24:case 27:case 30:case 31:case 33:case 34:case 35:case 36:case 37:case 38:case 39:case 40:case 41:case 800:case 810:case 811:case 812:case 720:case 721:case 722:case 723:case 711:case 2000:case 2001:case 2002:case 2003:return true;default:break;}
 return (id>=730&&id<=741)||(id>=400&&id<=402)||(id>=410&&id<=412)||(id>=2100&&id<=2110)||(id>=3000&&id<=3103&&id%10<=3)||(id>=3200&&id<=3203);
}
inline const char* callbackResult(uintptr_t id,const char* page){return opensChild(id)?page:nullptr;}
}
