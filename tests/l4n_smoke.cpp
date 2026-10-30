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
#include <future>
#include <thread>
#include "json.hpp"
#include "l4n/l4n_plugin.h"
void require(bool v,const char* text){if(!v){std::cerr<<text<<'\n';ExitProcess(1);}}
using PresentFn=HRESULT(WINAPI*)(IDirect3DSwapChain9*,const RECT*,const RECT*,HWND,const RGNDATA*,DWORD);
HRESULT WINAPI unavailableCreation(IDirect3DDevice9*,D3DDEVICE_CREATION_PARAMETERS*){return E_NOTIMPL;}
PresentFn nativeChainPresent=nullptr;bool visible=false;unsigned hudPixels=0;uint64_t signature=0;
HRESULT WINAPI observePresent(IDirect3DSwapChain9* chain,const RECT* source,const RECT* dest,HWND window,const RGNDATA* dirty,DWORD flags){
 IDirect3DDevice9* d=nullptr;IDirect3DSurface9 *back=nullptr,*cpu=nullptr;require(SUCCEEDED(chain->GetDevice(&d)),"Observer device");require(SUCCEEDED(chain->GetBackBuffer(0,D3DBACKBUFFER_TYPE_MONO,&back)),"Observer backbuffer");D3DSURFACE_DESC desc{};back->GetDesc(&desc);require(SUCCEEDED(d->CreateOffscreenPlainSurface(desc.Width,desc.Height,desc.Format,D3DPOOL_SYSTEMMEM,&cpu,nullptr)),"Observer allocation");require(SUCCEEDED(d->GetRenderTargetData(back,cpu)),"Observer capture");D3DLOCKED_RECT lock{};require(SUCCEEDED(cpu->LockRect(&lock,nullptr,D3DLOCK_READONLY)),"Observer lock");visible=false;hudPixels=0;signature=1469598103934665603ull;for(UINT y=0;y<desc.Height;++y){auto row=reinterpret_cast<DWORD*>(static_cast<char*>(lock.pBits)+y*lock.Pitch);for(UINT x=0;x<desc.Width;++x){auto pixel=row[x]&0xffffff;visible|=pixel!=0;if(x>=8&&x<680&&y+60>=desc.Height/2&&y<desc.Height/2+60&&pixel)++hudPixels;signature=(signature^pixel)*1099511628211ull;}}cpu->UnlockRect();cpu->Release();back->Release();d->Release();return nativeChainPresent(chain,source,dest,window,dirty,flags);
}
int main(){
 static_assert(sizeof(void*)==4,"x86 smoke only");auto root=std::filesystem::current_path();auto library=root/L"build/L4D2DangoMusic.dll";HMODULE module=LoadLibraryW(library.c_str());require(module!=nullptr,"Load plugin");
 auto address=GetProcAddress(module,"GetL4NPluginInstance");GetL4NPluginInstanceFunc query=nullptr;std::memcpy(&query,&address,sizeof(query));require(query!=nullptr,"L4N export");require(!GetProcAddress(module,"LDBHostPluginQuery"),"Unexpected Host dependency");auto plugin=query();auto vt=*reinterpret_cast<void***>(plugin);
 require(reinterpret_cast<unsigned(__thiscall*)(void*)>(vt[1])(plugin)==2,"MSVC L4N ABI");reinterpret_cast<void(__thiscall*)(void*)>(vt[5])(plugin);auto menu=reinterpret_cast<const char*(__thiscall*)(void*,bool)>(vt[8]);require(std::string(menu(plugin,true))=="L4D2DangoMusic 0.9.4","Title");
 auto entry=std::string(menu(plugin,false));require(entry.find(u8"打开歌词控制面板")!=std::string::npos&&entry.find(u8"翻译：")==std::string::npos,"L4N should expose only the native HUD entry");
 std::smatch callback;require(std::regex_search(entry,callback,std::regex(R"rx("callback"\s+"(0x[0-9a-fA-F]+)")rx")),"Entry callback");auto open=reinterpret_cast<const char*(__cdecl*)(void*)>(uintptr_t(std::stoul(callback[1],nullptr,16)));require(open(nullptr)==nullptr,"Entry callback must not add a L4N page");
 // A separate window thread reproduces the environment in which a render-
 // thread SetWindowSubclass fails. The HUD must still draw and accept keys.
 std::promise<HWND> windowPromise;auto windowFuture=windowPromise.get_future();std::thread windowThread([&](){auto w=CreateWindowExW(0,L"STATIC",L"Dango native HUD smoke",WS_OVERLAPPEDWINDOW,0,0,1280,720,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);ShowWindow(w,SW_SHOW);windowPromise.set_value(w);MSG message{};while(GetMessageW(&message,nullptr,0,0)>0){if(message.message==WM_APP+42)break;TranslateMessage(&message);DispatchMessageW(&message);}DestroyWindow(w);});
 auto window=windowFuture.get();require(window!=nullptr,"Window");SetForegroundWindow(window);
 auto api=Direct3DCreate9(D3D_SDK_VERSION);require(api!=nullptr,"D3D9");D3DPRESENT_PARAMETERS params{};params.Windowed=TRUE;params.SwapEffect=D3DSWAPEFFECT_DISCARD;params.hDeviceWindow=window;params.BackBufferWidth=1280;params.BackBufferHeight=720;params.BackBufferFormat=D3DFMT_X8R8G8B8;IDirect3DDevice9* device=nullptr;require(SUCCEEDED(api->CreateDevice(0,D3DDEVTYPE_HAL,window,D3DCREATE_SOFTWARE_VERTEXPROCESSING,&params,&device)),"Device");
 params.SwapEffect=D3DSWAPEFFECT_COPY;IDirect3DSwapChain9* probe=nullptr;require(SUCCEEDED(device->CreateAdditionalSwapChain(&params,&probe)),"Probe");auto table=*reinterpret_cast<void***>(probe);nativeChainPresent=reinterpret_cast<PresentFn>(table[3]);DWORD protection=0,ignored=0;require(VirtualProtect(table+3,sizeof(void*),PAGE_READWRITE,&protection)!=FALSE,"Observer patch");table[3]=reinterpret_cast<void*>(&observePresent);VirtualProtect(table+3,sizeof(void*),protection,&ignored);
 reinterpret_cast<void(__thiscall*)(void*,void*,bool)>(vt[7])(plugin,device,false);probe->Release();IDirect3DSwapChain9* chain=nullptr;require(SUCCEEDED(device->CreateAdditionalSwapChain(&params,&chain)),"Chain");IDirect3DSurface9* back=nullptr;chain->GetBackBuffer(0,D3DBACKBUFFER_TYPE_MONO,&back);device->ColorFill(back,nullptr,0xff000000);
 auto present=[&](){MSG message{};while(PeekMessageW(&message,nullptr,0,0,PM_REMOVE)){TranslateMessage(&message);DispatchMessageW(&message);}require(SUCCEEDED(chain->Present(nullptr,nullptr,window,nullptr,0)),"Present");};
 auto press=[&](WORD key){INPUT in{};in.type=INPUT_KEYBOARD;in.ki.wVk=key;require(SendInput(1,&in,sizeof(in))==1,"Key down");Sleep(30);present();in.ki.dwFlags=KEYEVENTF_KEYUP;require(SendInput(1,&in,sizeof(in))==1,"Key up");Sleep(30);present();};
 // No creation parameters are available during initial HUD setup. The
 // swapchain/owning-process window fallback must not prevent initialization.
 auto deviceTable=*reinterpret_cast<void***>(device);auto originalCreation=deviceTable[9];DWORD creationProtection=0;require(VirtualProtect(deviceTable+9,sizeof(void*),PAGE_READWRITE,&creationProtection)!=FALSE,"Creation query patch");deviceTable[9]=reinterpret_cast<void*>(&unavailableCreation);VirtualProtect(deviceTable+9,sizeof(void*),creationProtection,&ignored);
 present();require(hudPixels>10000,"HUD panel pixels missing with unavailable creation parameters / foreign window thread");require(VirtualProtect(deviceTable+9,sizeof(void*),PAGE_READWRITE,&creationProtection)!=FALSE,"Creation query restore");deviceTable[9]=originalCreation;VirtualProtect(deviceTable+9,sizeof(void*),creationProtection,&ignored);Sleep(200);present();
 // Every change stays on its current page. Save verifies real native-keyboard
 // routing rather than only inspecting a regenerated KeyValues string.
 press('2');for(int i=0;i<11;++i){open(nullptr);press('1');}press('0');press('7');
 auto configPath=root/L"build/L4D2DangoMusic/config.json";auto saved=[&](auto predicate){for(int i=0;i<60;++i){Sleep(50);std::ifstream f(configPath);if(f)try{if(predicate(nlohmann::json::parse(f)))return;}catch(...){}}require(false,"Native menu settings were not saved");};
 saved([](const auto& c){return c.value("translation",false);});
 // Reopen, custom reanchor: +1 and +0.1 seconds, then save from the root.
 press(VK_ESCAPE);menu(plugin,false);present();present();press('2');press('5');press('4');press('5');press('5');press('4');press('0');press('0');press('0');press('0');press('7');saved([](const auto& c){return c.value("reanchorMs",0)==1100;});
 press(VK_ESCAPE);menu(plugin,false);present();present();press('3');press('1');press('2');press('4');press('7');saved([](const auto& c){return std::abs(c.at("layout").at("lyrics")[0].at("distance").template get<float>()-250)<.01f;});
 // HUD continues working when the lyric switch itself is off.
 press('1');present();require(hudPixels>10000,"Disabling lyrics also hid the menu");press('1');press('8');press('1');press('1');press('0');press('0');press('7');saved([](const auto& c){return c.at("hudAppearance").value("preset",0)==1;});press(VK_ESCAPE);
 // Wall reuse, floor options and the fixed layout preview use real native keys.
 menu(plugin,false);present();present();press('6');press('7');press('8');press('4');press('5');press('3');press('3');press('1');press('3');press('6');
 saved([](const auto& c){return c.value("wallReuse",false)&&c.value("wallSurface",0)==1&&c.value("wallFloorFollow",false)&&std::abs(c.at("wallLayout").at("lyrics").at("x").template get<float>()-.51f)<.001f;});press(VK_ESCAPE);
 D3DVIEWPORT9 vp{2,3,100,80,0,1};device->SetViewport(&vp);device->SetRenderState(D3DRS_ALPHABLENDENABLE,FALSE);present();DWORD blend=TRUE;device->GetRenderState(D3DRS_ALPHABLENDENABLE,&blend);require(blend==FALSE,"Blend state leaked");D3DVIEWPORT9 after{};device->GetViewport(&after);require(!std::memcmp(&vp,&after,sizeof(vp)),"Viewport leaked");
 IDirect3DSurface9* cpu=nullptr;require(SUCCEEDED(device->CreateOffscreenPlainSurface(1280,720,D3DFMT_X8R8G8B8,D3DPOOL_SYSTEMMEM,&cpu,nullptr)),"Read surface");require(SUCCEEDED(device->GetRenderTargetData(back,cpu)),"Read target");D3DLOCKED_RECT lock{};require(SUCCEEDED(cpu->LockRect(&lock,nullptr,D3DLOCK_READONLY)),"Read lock");bool stale=false;for(int y=0;y<720;++y){auto row=reinterpret_cast<DWORD*>(static_cast<char*>(lock.pBits)+y*lock.Pitch);for(int x=0;x<1280;++x)stale|=(row[x]&0xffffff)!=0;}cpu->UnlockRect();require(!stale,"HUD/lyric pixels remained in pristine scene");cpu->Release();back->Release();chain->Release();
 // Duplicate-name protection still covers native menu and render hooks.
 auto alias=root/L"build/L4D2DangoMusic-copy.dll";std::filesystem::copy_file(library,alias,std::filesystem::copy_options::overwrite_existing);auto extra=LoadLibraryW(alias.c_str());require(extra!=nullptr,"Duplicate load");auto extraAddress=GetProcAddress(extra,"GetL4NPluginInstance");GetL4NPluginInstanceFunc extraQuery=nullptr;std::memcpy(&extraQuery,&extraAddress,sizeof(extraQuery));auto extraPlugin=extraQuery();auto extraTable=*reinterpret_cast<void***>(extraPlugin);require(std::string(reinterpret_cast<const char*(__thiscall*)(void*,bool)>(extraTable[8])(extraPlugin,true)).find(u8"重复副本")!=std::string::npos,"Duplicate renderer enabled");FreeLibrary(extra);std::filesystem::remove(alias);
 params.SwapEffect=D3DSWAPEFFECT_DISCARD;menu(plugin,false);require(SUCCEEDED(device->Reset(&params)),"Reset while HUD open");require(SUCCEEDED(device->Present(nullptr,nullptr,window,nullptr)),"HUD after Reset");auto stopAddress=GetProcAddress(module,"L4D2DangoMusicShutdown");void(*stop)()=nullptr;std::memcpy(&stop,&stopAddress,sizeof(stop));require(stop!=nullptr,"Shutdown export");stop();require(VirtualProtect(table+3,sizeof(void*),PAGE_READWRITE,&protection)!=FALSE,"Observer restore");table[3]=reinterpret_cast<void*>(nativeChainPresent);VirtualProtect(table+3,sizeof(void*),protection,&ignored);device->Release();api->Release();FreeLibrary(module);PostMessageW(window,WM_APP+42,0,0);windowThread.join();
 std::cout<<"Passed: native HUD entry/keyboard navigation/live state/save, lyric-off controls, pristine buffer/state restoration, duplicate guard, Reset and unload\n";
}
