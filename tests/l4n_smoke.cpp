#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <d3d9.h>
#include <filesystem>
#include <cstring>
#include <iostream>
#include <vector>
#include <regex>
#include <fstream>
#include "json.hpp"
#include "l4n/l4n_plugin.h"
void require(bool v,const char* text){if(!v){std::cerr<<text<<'\n';ExitProcess(1);}}
const char* click(const std::string& menu,unsigned id){
 std::regex row(R"menu("callback"\s+"(0x[0-9a-fA-F]+)"\s+"user_data"\s+"(0x[0-9a-fA-F]+)")menu");
 for(auto i=std::sregex_iterator(menu.begin(),menu.end(),row);i!=std::sregex_iterator();++i){auto match=*i;if(std::stoul(match[2],nullptr,16)!=id)continue;using Callback=const char*(__cdecl*)(void*);auto callback=reinterpret_cast<Callback>(uintptr_t(std::stoul(match[1],nullptr,16)));return callback(reinterpret_cast<void*>(uintptr_t(id)));}
 require(false,"Menu item missing");return nullptr;
}
using PresentFn=HRESULT(WINAPI*)(IDirect3DSwapChain9*,const RECT*,const RECT*,HWND,const RGNDATA*,DWORD);
PresentFn nativeChainPresent=nullptr;unsigned visiblePresentations=0;
HRESULT WINAPI observePresent(IDirect3DSwapChain9* chain,const RECT* source,const RECT* dest,HWND window,const RGNDATA* dirty,DWORD flags){
 IDirect3DDevice9* device=nullptr;IDirect3DSurface9 *back=nullptr,*cpu=nullptr;
 require(SUCCEEDED(chain->GetDevice(&device)),"Observer device query failed");require(SUCCEEDED(chain->GetBackBuffer(0,D3DBACKBUFFER_TYPE_MONO,&back)),"Observer target failed");
 D3DSURFACE_DESC desc{};back->GetDesc(&desc);require(SUCCEEDED(device->CreateOffscreenPlainSurface(desc.Width,desc.Height,desc.Format,D3DPOOL_SYSTEMMEM,&cpu,nullptr)),"Observer readback allocation failed");require(SUCCEEDED(device->GetRenderTargetData(back,cpu)),"Observer readback failed");D3DLOCKED_RECT rect{};require(SUCCEEDED(cpu->LockRect(&rect,nullptr,D3DLOCK_READONLY)),"Observer lock failed");bool words=false;for(UINT y=desc.Height/2;y<desc.Height;++y){auto row=reinterpret_cast<DWORD*>(static_cast<char*>(rect.pBits)+y*rect.Pitch);for(UINT x=0;x<desc.Width;++x)words|=(row[x]&0xffffff)!=0;}cpu->UnlockRect();cpu->Release();back->Release();device->Release();require(words,"No current lyric pixels at actual Present");++visiblePresentations;
 return nativeChainPresent(chain,source,dest,window,dirty,flags);
}
int main(){
 static_assert(sizeof(void*)==4,"Run x86 L4N smoke");auto root=std::filesystem::current_path();auto library=root/L"build/L4D2DangoMusic.dll";HMODULE module=LoadLibraryW(library.c_str());require(module!=nullptr,"DLL load failed");
 auto address=GetProcAddress(module,"GetL4NPluginInstance");GetL4NPluginInstanceFunc query=nullptr;std::memcpy(&query,&address,sizeof(query));require(query!=nullptr,"Missing L4N export");require(!GetProcAddress(module,"LDBHostPluginQuery"),"Unexpected Host plugin export");
 auto plugin=query();require(plugin!=nullptr,"No plugin");auto vt=*reinterpret_cast<void***>(plugin);auto version=reinterpret_cast<unsigned(__thiscall*)(void*)>(vt[1]);require(version(plugin)==2,"Invalid MSVC L4N vtable layout");reinterpret_cast<void(__thiscall*)(void*)>(vt[5])(plugin);
 auto menu=reinterpret_cast<const char*(__thiscall*)(void*,bool)>(vt[8]);require(std::string(menu(plugin,true))=="L4D2DangoMusic","Menu ABI/title mismatch");
 std::string rootMenu=menu(plugin,false);std::string display=click(rootMenu,20);click(display,2);display=click(std::string(menu(plugin,false)),20);require(display.find(u8"翻译：开")!=std::string::npos,"Live translation menu did not update");
 click(display,2);auto presets=std::string(click(std::string(menu(plugin,false)),10));click(presets,103);require(std::string(menu(plugin,false)).find(u8"冰蓝")!=std::string::npos,"Preset menu failed");
 click(std::string(menu(plugin,false)),18);auto configPath=root/L"build/L4D2DangoMusic/config.json";bool saved=false;for(int i=0;i<40&&!saved;++i){Sleep(50);std::ifstream file(configPath);if(file)try{auto config=nlohmann::json::parse(file);saved=config.value("presetId",-1)==3;}catch(...){}}require(saved,"Menu config was not saved by worker");
 auto window=CreateWindowExW(0,L"STATIC",L"L4N smoke",WS_OVERLAPPEDWINDOW,0,0,1280,720,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);require(window!=nullptr,"Window failed");ShowWindow(window,SW_SHOW);
 auto api=Direct3DCreate9(D3D_SDK_VERSION);require(api!=nullptr,"D3D9 unavailable");D3DPRESENT_PARAMETERS params{};params.Windowed=TRUE;params.SwapEffect=D3DSWAPEFFECT_DISCARD;params.hDeviceWindow=window;params.BackBufferWidth=1280;params.BackBufferHeight=720;params.BackBufferFormat=D3DFMT_X8R8G8B8;
 IDirect3DDevice9* device=nullptr;require(SUCCEEDED(api->CreateDevice(0,D3DDEVTYPE_HAL,window,D3DCREATE_SOFTWARE_VERTEXPROCESSING,&params,&device)),"Device failed");params.SwapEffect=D3DSWAPEFFECT_COPY;IDirect3DSwapChain9* probe=nullptr;require(SUCCEEDED(device->CreateAdditionalSwapChain(&params,&probe)),"Probe chain failed");auto chainTable=*reinterpret_cast<void***>(probe);nativeChainPresent=reinterpret_cast<PresentFn>(chainTable[3]);DWORD oldProtection=0;require(VirtualProtect(chainTable+3,sizeof(void*),PAGE_READWRITE,&oldProtection)!=FALSE,"Observer patch failed");chainTable[3]=reinterpret_cast<void*>(&observePresent);DWORD ignored=0;VirtualProtect(chainTable+3,sizeof(void*),oldProtection,&ignored);
 reinterpret_cast<void(__thiscall*)(void*,void*,bool)>(vt[7])(plugin,device,false);probe->Release();Sleep(1700);
 // COPY chain keeps the drawn backbuffer readable after Present.
 params.SwapEffect=D3DSWAPEFFECT_COPY;IDirect3DSwapChain9* chain=nullptr;require(SUCCEEDED(device->CreateAdditionalSwapChain(&params,&chain)),"Additional chain failed");IDirect3DSurface9* back=nullptr;chain->GetBackBuffer(0,D3DBACKBUFFER_TYPE_MONO,&back);device->ColorFill(back,nullptr,0xff000000);
 D3DVIEWPORT9 vp{2,3,100,80,0,1};device->SetViewport(&vp);device->SetRenderState(D3DRS_ALPHABLENDENABLE,FALSE);
 auto capturePixels=[&](){IDirect3DSurface9* cpu=nullptr;require(SUCCEEDED(device->CreateOffscreenPlainSurface(1280,720,D3DFMT_X8R8G8B8,D3DPOOL_SYSTEMMEM,&cpu,nullptr)),"Capture allocation failed");require(SUCCEEDED(device->GetRenderTargetData(back,cpu)),"Capture read failed");D3DLOCKED_RECT locked{};require(SUCCEEDED(cpu->LockRect(&locked,nullptr,D3DLOCK_READONLY)),"Capture lock failed");std::vector<DWORD> pixels(1280*720);for(int row=0;row<720;++row)std::memcpy(pixels.data()+row*1280,static_cast<char*>(locked.pBits)+row*locked.Pitch,1280*sizeof(DWORD));cpu->UnlockRect();cpu->Release();return pixels;};
 require(SUCCEEDED(chain->Present(nullptr,nullptr,window,nullptr,0)),"Intercepted Present failed");auto firstFrame=capturePixels();require(SUCCEEDED(chain->Present(nullptr,nullptr,window,nullptr,0)),"Repeated Present failed");require(capturePixels()==firstFrame,"Repeated Present did not restore the same pristine scene");DWORD blend=TRUE;device->GetRenderState(D3DRS_ALPHABLENDENABLE,&blend);require(blend==FALSE,"Leaked blend state");D3DVIEWPORT9 restored{};device->GetViewport(&restored);require(!std::memcmp(&vp,&restored,sizeof(vp)),"Leaked viewport");
 IDirect3DSurface9* read=nullptr;require(SUCCEEDED(device->CreateOffscreenPlainSurface(1280,720,D3DFMT_X8R8G8B8,D3DPOOL_SYSTEMMEM,&read,nullptr)),"Read surface failed");require(SUCCEEDED(device->GetRenderTargetData(back,read)),"Readback failed");D3DLOCKED_RECT rect{};require(SUCCEEDED(read->LockRect(&rect,nullptr,D3DLOCK_READONLY)),"Lock failed");bool pixels=false;for(int y=360;y<720;++y){auto row=reinterpret_cast<DWORD*>(static_cast<char*>(rect.pBits)+y*rect.Pitch);for(int x=0;x<1280;++x)pixels|=(row[x]&0xffffff)!=0;}read->UnlockRect();require(!pixels,"Overlay pixels leaked into application-owned backbuffer");require(visiblePresentations==2,"Observer did not see fresh lyrics in both Presents");read->Release();back->Release();chain->Release();params.SwapEffect=D3DSWAPEFFECT_DISCARD;
 require(SUCCEEDED(device->Reset(&params)),"Reset failed");auto stopAddress=GetProcAddress(module,"L4D2DangoMusicShutdown");void(*stop)()=nullptr;std::memcpy(&stop,&stopAddress,sizeof(stop));require(stop!=nullptr,"Missing shutdown utility");stop();require(VirtualProtect(chainTable+3,sizeof(void*),PAGE_READWRITE,&oldProtection)!=FALSE,"Observer restore failed");chainTable[3]=reinterpret_cast<void*>(nativeChainPresent);VirtualProtect(chainTable+3,sizeof(void*),oldProtection,&ignored);device->Release();api->Release();FreeLibrary(module);DestroyWindow(window);std::cout<<"Passed: x86 L4N loading, SwapChain interception, visible pixels at Present + pristine backbuffer after Present, state restore, Reset, unload\n";
}
