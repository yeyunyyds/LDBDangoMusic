#pragma once
#include <atomic>
namespace dango::lifetime {
inline std::atomic<bool> closing{false};
inline std::atomic<unsigned> callbacks{0};
struct Guard {
 bool admitted=false;
 Guard(){if(closing.load())return;callbacks.fetch_add(1);if(closing.load())callbacks.fetch_sub(1);else admitted=true;}
 ~Guard(){if(admitted)callbacks.fetch_sub(1);}
 Guard(const Guard&)=delete;Guard& operator=(const Guard&)=delete;
 explicit operator bool()const{return admitted;}
};
inline void requestStop() noexcept {closing.store(true);}
}
