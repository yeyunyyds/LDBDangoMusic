#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include "source_walls.hpp"
#include <iostream>
#include <stdexcept>
using namespace dango;
void check(bool ok){if(!ok)throw std::runtime_error("x86 TraceRay ABI regression");}
bool overflow=false;
void __fastcall fakeTrace(void*,void*,const trace::Ray& ray,unsigned mask,void* filter,trace::Result* result){
 check(mask==0x400b&&reinterpret_cast<uintptr_t>(&ray)%16==0);auto methods=*reinterpret_cast<void***>(filter);using Kind=int(__thiscall*)(void*);using Hit=bool(__thiscall*)(void*,void*,int);check(reinterpret_cast<Kind>(methods[1])(filter)==1&&!reinterpret_cast<Hit>(methods[0])(filter,nullptr,mask));
 *result={};result->start={ray.start.x,ray.start.y,ray.start.z};result->fraction=.5f;result->end=result->start+Vec3{ray.delta.x,ray.delta.y,ray.delta.z}*.5f;result->plane.normal={-1,0,0};result->plane.distance=-result->end.x;if(overflow)reinterpret_cast<uint8_t*>(result)[sizeof(*result)]=0;
}
int main(){static_assert(sizeof(void*)==4,"MSVC x86 wire test");void* methods[6]{};methods[5]=reinterpret_cast<void*>(&fakeTrace);struct Object{void** methods;};Object object{methods};source_walls::traceObject=&object;source_walls::faulted=false;trace::Result result;
 check(source_walls::cast({},{100,0,0},result)&&result.end.x==50);overflow=true;check(!source_walls::cast({},{100,0,0},result)&&source_walls::faulted);std::cout<<"Passed: x86 thiscall TraceRay, explicit two-method filter ABI, aligned Ray and corrupted output guard\n";}
