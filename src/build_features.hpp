#pragma once
#ifndef DANGO_DIAGNOSTICS
#define DANGO_DIAGNOSTICS 0
#endif
namespace dango::build { inline constexpr bool diagnostics=DANGO_DIAGNOSTICS!=0;
inline constexpr bool diagnosticAction(unsigned long long id){return id==800||id==801||id==802||id==705||id==9601||id==9603||(id>=9620&&id<=9627)||(id>=9640&&id<=9644);}
}
