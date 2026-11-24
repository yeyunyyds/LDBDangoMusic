#pragma once
#include <exception>
#include <string>
namespace dango::savepolicy {
// Backup is optional; replacing a successfully written temporary file is not.
// A replace exception must propagate so the saved snapshot is never advanced.
template<class Backup,class Replace,class Warn>
void commit(Backup backup,Replace replace,Warn warn){
 try{backup();}catch(const std::exception& e){warn(std::string("Config backup failed (continuing save): ")+e.what());}
 catch(...){warn("Config backup failed (continuing save): unknown error");}
 replace();
}
}
