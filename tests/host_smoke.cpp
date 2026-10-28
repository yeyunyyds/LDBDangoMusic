#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <d3d9.h>
#include <filesystem>
#include <iostream>
#include <cstring>
#include <fstream>
#include "json.hpp"
#include "ldb_host_plugin.h"
void require(bool ok,const char* message){if(!ok){std::cerr<<message<<'\n';ExitProcess(1);}}
void WINAPI logMessage(LDBHostLogLevel,const char* message){std::cout<<message<<'\n';}
bool hasPixels(IDirect3DDevice9* device,IDirect3DSurface9* back,bool lowerHalf=false){
 D3DSURFACE_DESC desc{};back->GetDesc(&desc);IDirect3DSurface9* read=nullptr;
 require(SUCCEEDED(device->CreateOffscreenPlainSurface(desc.Width,desc.Height,desc.Format,D3DPOOL_SYSTEMMEM,&read,nullptr)),"Readback allocation failed");
 require(SUCCEEDED(device->GetRenderTargetData(back,read)),"Readback failed");
 D3DLOCKED_RECT locked{};require(SUCCEEDED(read->LockRect(&locked,nullptr,D3DLOCK_READONLY)),"Readback lock failed");
 bool painted=false;
 for(UINT y=lowerHalf?desc.Height/2:0;y<desc.Height;++y){auto row=reinterpret_cast<DWORD*>(static_cast<char*>(locked.pBits)+y*locked.Pitch);for(UINT x=0;x<desc.Width;++x)painted|=(row[x]&0xffffff)!=0;}
 read->UnlockRect();read->Release();return painted;
}
void frame(LDBHostPluginV1& plugin,IDirect3DDevice9* device,IDirect3DSwapChain9* chain,bool swapPath,bool supplyChain=true){
 IDirect3DSurface9 *back=nullptr,*original=nullptr;
 require(SUCCEEDED(chain->GetBackBuffer(0,D3DBACKBUFFER_TYPE_MONO,&back)),"GetBackBuffer failed");
 require(SUCCEEDED(device->CreateRenderTarget(256,128,D3DFMT_X8R8G8B8,D3DMULTISAMPLE_NONE,0,FALSE,&original,nullptr)),"Original target allocation failed");
 require(SUCCEEDED(device->ColorFill(back,nullptr,0xff000000)),"Clear selected chain failed");
 D3DCAPS9 caps{};device->GetDeviceCaps(&caps);IDirect3DSurface9* secondary=nullptr;
 if(caps.NumSimultaneousRTs>1){require(SUCCEEDED(device->CreateRenderTarget(256,128,D3DFMT_X8R8G8B8,D3DMULTISAMPLE_NONE,0,FALSE,&secondary,nullptr)),"MRT allocation failed");device->ColorFill(secondary,nullptr,0xff000000);device->SetRenderTarget(1,secondary);}
 device->SetRenderTarget(0,original);D3DVIEWPORT9 vp{4,5,128,64,0,1};device->SetViewport(&vp);device->SetRenderState(D3DRS_ALPHABLENDENABLE,FALSE);
 D3DMATRIX view{};view._11=view._22=view._33=view._44=1;view._41=-10;view._42=-20;view._43=-30;device->SetTransform(D3DTS_VIEW,&view);
 LDBHostPresentInfoV1 present{};present.struct_size=sizeof(present);present.is_swapchain_present=swapPath;present.swap_chain=supplyChain?chain:nullptr;
 plugin.on_before_present(device,&present);
 IDirect3DSurface9* restored=nullptr;device->GetRenderTarget(0,&restored);require(restored==original,"Render target leaked");restored->Release();
 D3DVIEWPORT9 after{};device->GetViewport(&after);require(std::memcmp(&vp,&after,sizeof(vp))==0,"Viewport leaked");
 DWORD blend=TRUE;device->GetRenderState(D3DRS_ALPHABLENDENABLE,&blend);require(blend==FALSE,"Blend state leaked");
 if(secondary){IDirect3DSurface9* restoredExtra=nullptr;device->GetRenderTarget(1,&restoredExtra);require(restoredExtra==secondary,"MRT state leaked");restoredExtra->Release();require(!hasPixels(device,secondary),"Overlay painted into game MRT");}
 bool expected=!swapPath||supplyChain;
 require(hasPixels(device,back)==expected,"Wrong chain: missing pixels or invalid fallback painted");
 require(hasPixels(device,back,true)==expected,"No lower-screen lyric pixels (banner alone is insufficient)");
 // An undersized Present structure must not be read or drawn.
 device->ColorFill(back,nullptr,0xff000000);present.struct_size=8;plugin.on_before_present(device,&present);require(!hasPixels(device,back),"Accepted undersized Present");
 if(secondary){device->SetRenderTarget(1,nullptr);secondary->Release();}device->SetRenderTarget(0,back);original->Release();back->Release();
}
int main(){
 auto root=std::filesystem::current_path();auto dll=root/L"build"/L"LDBDangoMusic.dll";
 auto module=LoadLibraryW(dll.c_str());require(module!=nullptr,"LoadLibrary failed");
 auto address=GetProcAddress(module,"LDBHostPluginQuery");LDBHostPluginQueryFn query=nullptr;static_assert(sizeof(query)==sizeof(address));std::memcpy(&query,&address,sizeof(query));require(query!=nullptr,"Missing export");
 LDBHostPluginV1 plugin{};plugin.struct_size=sizeof(plugin);require(!query(999,&plugin),"Accepted invalid ABI");plugin.struct_size=80;require(!query(1,&plugin),"Accepted incompatible old SDK layout");plugin.struct_size=sizeof(plugin);require(query(1,&plugin),"ABI negotiation failed");
 require(plugin.on_swapchain_created&&plugin.on_swapchain_destroyed,"Missing SwapChain lifecycle callbacks");
 auto hostDir=root.wstring();LDBHostApiV1 host{sizeof(host),1,64,0,hostDir.c_str(),"standalone-smoke",logMessage};require(plugin.on_load(&host),"on_load failed");
 auto window=CreateWindowExW(0,L"STATIC",L"LDBDangoMusic smoke",WS_OVERLAPPEDWINDOW,0,0,1280,720,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);require(window!=nullptr,"Window creation failed");
 auto d3d=Direct3DCreate9(D3D_SDK_VERSION);require(d3d!=nullptr,"D3D9 unavailable");D3DPRESENT_PARAMETERS params{};params.Windowed=TRUE;params.SwapEffect=D3DSWAPEFFECT_DISCARD;params.hDeviceWindow=window;params.BackBufferWidth=1280;params.BackBufferHeight=720;params.BackBufferFormat=D3DFMT_X8R8G8B8;
 IDirect3DDevice9* device=nullptr;require(SUCCEEDED(d3d->CreateDevice(D3DADAPTER_DEFAULT,D3DDEVTYPE_HAL,window,D3DCREATE_SOFTWARE_VERTEXPROCESSING,&params,&device)),"Device creation failed");plugin.on_device_created(device);Sleep(1600);
 IDirect3DSwapChain9* primary=nullptr;require(SUCCEEDED(device->GetSwapChain(0,&primary)),"Primary chain missing");plugin.on_swapchain_created(primary);
 frame(plugin,device,primary,false);frame(plugin,device,primary,false,false);frame(plugin,device,primary,true);frame(plugin,device,primary,true,false);
 D3DPRESENT_PARAMETERS extra=params;extra.BackBufferWidth=640;extra.BackBufferHeight=360;IDirect3DSwapChain9* additional=nullptr;require(SUCCEEDED(device->CreateAdditionalSwapChain(&extra,&additional)),"Additional chain creation failed");plugin.on_swapchain_created(additional);frame(plugin,device,additional,true);plugin.on_swapchain_destroyed(additional);additional->Release();
 IDirect3DSurface9* mainBack=nullptr;primary->GetBackBuffer(0,D3DBACKBUFFER_TYPE_MONO,&mainBack);device->SetRenderTarget(0,mainBack);mainBack->Release();plugin.on_swapchain_destroyed(primary);primary->Release();
 plugin.on_before_reset(device,&params,FALSE);auto hr=device->Reset(&params);plugin.on_after_reset(device,hr,FALSE);require(SUCCEEDED(hr),"Reset failed");require(SUCCEEDED(device->GetSwapChain(0,&primary)),"Post-reset chain missing");plugin.on_swapchain_created(primary);frame(plugin,device,primary,true);plugin.on_swapchain_destroyed(primary);primary->Release();
 plugin.on_device_destroyed(device);device->Release();d3d->Release();plugin.on_shutdown();
 auto settingsPath=root/L"plugins"/L"LDBDangoMusic"/L"config.json";std::ifstream settingsFile(settingsPath);
 if(settingsFile&&nlohmann::json::parse(settingsFile).value("cameraDiagnostics",false)){
 bool captured=false;for(const auto& entry:std::filesystem::directory_iterator(settingsPath.parent_path())){if(entry.path().extension()!=L".jsonl")continue;std::ifstream file(entry.path());std::string line;while(std::getline(file,line)){auto sample=nlohmann::json::parse(line);if(sample["view"][12]==-10&&sample["view"][13]==-20&&sample["viewport"][2]==128)captured=true;require(sample["camera_confirmed"]==false,"Diagnostic incorrectly confirmed camera");}}
 require(captured,"No pre-overlay matrix/viewport diagnostic captured");}
 FreeLibrary(module);DestroyWindow(window);
 std::cout<<"Passed: ABI layout, Device/SwapChain/additional-chain pixels, missing-chain rejection, RT/viewport/blend restoration, Reset redraw, shutdown\n";
}
