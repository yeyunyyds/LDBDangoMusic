#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <d3d9.h>
#include <filesystem>
#include <iostream>
#include <cstring>
#include "ldb_host_plugin.h"
void require(bool b,const char* message){if(!b){std::cerr<<message<<'\n';ExitProcess(1);}}
void WINAPI logMessage(LDBHostLogLevel,const char* message){std::cout<<message<<'\n';}
int main(){
 auto root=std::filesystem::current_path();auto dll=root/L"build"/L"LDBDangoMusic.dll";
 auto module=LoadLibraryW(dll.c_str());require(module!=nullptr,"LoadLibrary failed (check runtime dependencies)");
 auto address=GetProcAddress(module,"LDBHostPluginQuery");LDBHostPluginQueryFn query=nullptr;static_assert(sizeof(query)==sizeof(address));std::memcpy(&query,&address,sizeof(query));require(query!=nullptr,"Missing export");
 LDBHostPluginV1 plugin{};plugin.struct_size=sizeof(plugin);require(!query(999,&plugin),"Accepted invalid ABI");require(query(1,&plugin),"ABI negotiation failed");
 auto hostDir=root.wstring();LDBHostApiV1 host{sizeof(host),1,64,0,hostDir.c_str(),"standalone-smoke",logMessage};require(plugin.on_load(&host),"on_load failed");
 auto window=CreateWindowExW(0,L"STATIC",L"LDBDangoMusic smoke",WS_OVERLAPPEDWINDOW,0,0,1280,720,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);require(window!=nullptr,"Window creation failed");
 auto d3d=Direct3DCreate9(D3D_SDK_VERSION);require(d3d!=nullptr,"D3D9 unavailable");D3DPRESENT_PARAMETERS params{};params.Windowed=TRUE;params.SwapEffect=D3DSWAPEFFECT_DISCARD;params.hDeviceWindow=window;params.BackBufferWidth=1280;params.BackBufferHeight=720;params.BackBufferFormat=D3DFMT_X8R8G8B8;
 IDirect3DDevice9* device=nullptr;require(SUCCEEDED(d3d->CreateDevice(D3DADAPTER_DEFAULT,D3DDEVTYPE_HAL,window,D3DCREATE_SOFTWARE_VERTEXPROCESSING,&params,&device)),"Device creation failed");plugin.on_device_created(device);Sleep(1600);
 device->Clear(0,nullptr,D3DCLEAR_TARGET,0xff000000,1,0);device->SetRenderState(D3DRS_ALPHABLENDENABLE,FALSE);
 LDBHostPresentInfoV1 present{};present.struct_size=sizeof(present);plugin.on_before_present(device,&present);
 DWORD blend=TRUE;device->GetRenderState(D3DRS_ALPHABLENDENABLE,&blend);require(blend==FALSE,"Render state leaked");
 IDirect3DSurface9 *back=nullptr,*read=nullptr;require(SUCCEEDED(device->GetBackBuffer(0,0,D3DBACKBUFFER_TYPE_MONO,&back)),"Backbuffer unavailable");require(SUCCEEDED(device->CreateOffscreenPlainSurface(1280,720,D3DFMT_X8R8G8B8,D3DPOOL_SYSTEMMEM,&read,nullptr)),"Readback allocation failed");require(SUCCEEDED(device->GetRenderTargetData(back,read)),"Readback failed");D3DLOCKED_RECT locked{};require(SUCCEEDED(read->LockRect(&locked,nullptr,D3DLOCK_READONLY)),"Readback lock failed");bool painted=false;for(int y=0;y<720;++y){auto row=reinterpret_cast<DWORD*>(static_cast<char*>(locked.pBits)+y*locked.Pitch);for(int x=0;x<1280;++x)painted|=(row[x]&0xffffff)!=0;}read->UnlockRect();read->Release();back->Release();require(painted,"No lyric/status pixels rendered");
 plugin.on_before_reset(device,&params,FALSE);auto hr=device->Reset(&params);plugin.on_after_reset(device,hr,FALSE);require(SUCCEEDED(hr),"Reset failed");plugin.on_before_present(device,&present);plugin.on_device_destroyed(device);device->Release();d3d->Release();plugin.on_shutdown();FreeLibrary(module);DestroyWindow(window);
 std::cout<<"Passed: ABI/load, painted pixels, state restoration, Reset, shutdown\n";
}
