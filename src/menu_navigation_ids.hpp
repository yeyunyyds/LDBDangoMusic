#pragma once
#include <cstdint>
namespace dango::navigation {
inline bool page(uintptr_t id){
 switch(id){case 5000:case 5010:case 5020:case 5030:case 5040:case 5050:case 5060:case 5100:case 5110:case 5111:case 5112:case 5113:case 5114:case 5115:case 5116:case 5140:case 5200:case 5201:case 5202:case 5203:case 5230:case 5231:case 5232:case 5233:case 5300:case 5400:case 5410:case 5420:case 5430:case 5402:case 5500:case 5501:return true;default:return id>=5600&&id<5628;}
}
inline bool ordinaryPreview(uintptr_t id){return id==5201||id==5202||id==5203||id==2003||(id>=2100&&id<2111)||(id>=3000&&id<=3103&&id%10<=3)||(id>=3200&&id<=3203);}
inline bool wallPreview(uintptr_t id){return id==720||(id>=721&&id<=723)||(id>=730&&id<=741);}
inline bool followPreview(uintptr_t id){return id==812||id==818||(id>=813&&id<=816);}
}
