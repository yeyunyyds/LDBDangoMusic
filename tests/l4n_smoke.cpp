#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <d3d9.h>
#include <filesystem>
#include <cstring>
#include <iostream>
#include "l4n/l4n_plugin.h"
void require(bool v,const char* text){if(!v){std::cerr<<text<<'\n';ExitProcess(1);}}
int main(){
 static_assert(sizeof(void*)==4,"Run x86 L4N smoke");auto root=std::filesystem::current_path();auto library=root/L"build/LDBDangoMusic.dll";HMODULE module=LoadLibraryW(library.c_str());require(module!=nullptr,"DLL load failed");
 auto address=GetProcAddress(module,"GetL4NPluginInstance");GetL4NPluginInstanceFunc query=nullptr;std::memcpy(&query,&address,sizeof(query));require(query!=nullptr,"Missing L4N export");require(!GetProcAddress(module,"LDBHostPluginQuery"),"Unexpected Host plugin export");
 auto plugin=query();require(plugin!=nullptr,"No plugin");auto vt=*reinterpret_cast<void***>(plugin);auto version=reinterpret_cast<unsigned(__thiscall*)(void*)>(vt[1]);require(version(plugin)==2,"Invalid MSVC L4N vtable layout");reinterpret_cast<void(__thiscall*)(void*)>(vt[5])(plugin);
 auto window=CreateWindowExW(0,L"STATIC",L"L4N smoke",WS_OVERLAPPEDWINDOW,0,0,1280,720,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);require(window!=nullptr,"Window failed");ShowWindow(window,SW_SHOW);
 auto api=Direct3DCreate9(D3D_SDK_VERSION);require(api!=nullptr,"D3D9 unavailable");D3DPRESENT_PARAMETERS params{};params.Windowed=TRUE;params.SwapEffect=D3DSWAPEFFECT_DISCARD;params.hDeviceWindow=window;params.BackBufferWidth=1280;params.BackBufferHeight=720;params.BackBufferFormat=D3DFMT_X8R8G8B8;
 IDirect3DDevice9* device=nullptr;require(SUCCEEDED(api->CreateDevice(0,D3DDEVTYPE_HAL,window,D3DCREATE_SOFTWARE_VERTEXPROCESSING,&params,&device)),"Device failed");reinterpret_cast<void(__thiscall*)(void*,void*,bool)>(vt[7])(plugin,device,false);Sleep(1700);
 // COPY chain keeps the drawn backbuffer readable after Present.
 params.SwapEffect=D3DSWAPEFFECT_COPY;IDirect3DSwapChain9* chain=nullptr;require(SUCCEEDED(device->CreateAdditionalSwapChain(&params,&chain)),"Additional chain failed");IDirect3DSurface9* back=nullptr;chain->GetBackBuffer(0,D3DBACKBUFFER_TYPE_MONO,&back);device->ColorFill(back,nullptr,0xff000000);
 D3DVIEWPORT9 vp{2,3,100,80,0,1};device->SetViewport(&vp);device->SetRenderState(D3DRS_ALPHABLENDENABLE,FALSE);
 require(SUCCEEDED(chain->Present(nullptr,nullptr,window,nullptr,0)),"Intercepted Present failed");DWORD blend=TRUE;device->GetRenderState(D3DRS_ALPHABLENDENABLE,&blend);require(blend==FALSE,"Leaked blend state");D3DVIEWPORT9 restored{};device->GetViewport(&restored);require(!std::memcmp(&vp,&restored,sizeof(vp)),"Leaked viewport");
 IDirect3DSurface9* read=nullptr;require(SUCCEEDED(device->CreateOffscreenPlainSurface(1280,720,D3DFMT_X8R8G8B8,D3DPOOL_SYSTEMMEM,&read,nullptr)),"Read surface failed");require(SUCCEEDED(device->GetRenderTargetData(back,read)),"Readback failed");D3DLOCKED_RECT rect{};require(SUCCEEDED(read->LockRect(&rect,nullptr,D3DLOCK_READONLY)),"Lock failed");bool pixels=false;for(int y=360;y<720;++y){auto row=reinterpret_cast<DWORD*>(static_cast<char*>(rect.pBits)+y*rect.Pitch);for(int x=0;x<1280;++x)pixels|=(row[x]&0xffffff)!=0;}read->UnlockRect();require(pixels,"No lyric pixels through L4N SwapChain hook");read->Release();back->Release();chain->Release();params.SwapEffect=D3DSWAPEFFECT_DISCARD;
 require(SUCCEEDED(device->Reset(&params)),"Reset failed");device->Release();api->Release();auto stopAddress=GetProcAddress(module,"L4NLyricsShutdown");void(*stop)()=nullptr;std::memcpy(&stop,&stopAddress,sizeof(stop));require(stop!=nullptr,"Missing shutdown utility");stop();FreeLibrary(module);DestroyWindow(window);std::cout<<"Passed: x86 L4N loading, SwapChain interception, rendered pixels, state restore, Reset, unload\n";
}
