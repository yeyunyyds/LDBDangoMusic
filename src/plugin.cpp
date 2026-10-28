#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <winhttp.h>
#include <d3d9.h>
#include <atomic>
#include <memory>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <fstream>
#include <filesystem>
#include <map>
#include "ldb_host_plugin.h"
#include "core.hpp"
namespace {
using namespace dango;
int64_t now(){return static_cast<int64_t>(GetTickCount64());}
struct Config {int port=23330,size=32;bool demo=false,translation=true,alternate=true;float y=.72f;int offset=0;};
Config cfg;std::filesystem::path directory;LDBHostLogFn logger=nullptr;
std::atomic<bool> stopping{false};std::thread worker;std::mutex sleepMutex;std::condition_variable wake;
std::shared_ptr<const State> snapshot=std::make_shared<State>();
void publish(State s){std::atomic_store(&snapshot,std::make_shared<const State>(std::move(s)));}
struct Internet {HINTERNET h; explicit Internet(HINTERNET v):h(v){if(!h)throw std::runtime_error("WinHTTP handle failed");}~Internet(){WinHttpCloseHandle(h);}Internet(const Internet&)=delete;};
nlohmann::json get(const wchar_t* path,size_t limit){
 Internet session(WinHttpOpen(L"LDBDangoMusic/0.1",WINHTTP_ACCESS_TYPE_NO_PROXY,WINHTTP_NO_PROXY_NAME,WINHTTP_NO_PROXY_BYPASS,0));
 WinHttpSetTimeouts(session.h,500,500,500,500);
 Internet connection(WinHttpConnect(session.h,L"127.0.0.1",static_cast<INTERNET_PORT>(cfg.port),0));
 Internet request(WinHttpOpenRequest(connection.h,L"GET",path,nullptr,WINHTTP_NO_REFERER,WINHTTP_DEFAULT_ACCEPT_TYPES,0));
 DWORD policy=WINHTTP_OPTION_REDIRECT_POLICY_NEVER;WinHttpSetOption(request.h,WINHTTP_OPTION_REDIRECT_POLICY,&policy,sizeof(policy));
 if(!WinHttpSendRequest(request.h,WINHTTP_NO_ADDITIONAL_HEADERS,0,WINHTTP_NO_REQUEST_DATA,0,0,0)||!WinHttpReceiveResponse(request.h,nullptr))throw std::runtime_error("Player API unavailable");
 DWORD code=0,length=sizeof(code);if(!WinHttpQueryHeaders(request.h,WINHTTP_QUERY_STATUS_CODE|WINHTTP_QUERY_FLAG_NUMBER,WINHTTP_HEADER_NAME_BY_INDEX,&code,&length,WINHTTP_NO_HEADER_INDEX)||code!=200)throw std::runtime_error("Player HTTP status rejected");
 std::string body;char buffer[8192];DWORD count=0;auto deadline=now()+2000;
 for(;;){if(stopping||now()>deadline)throw std::runtime_error("Request cancelled or timed out");if(!WinHttpReadData(request.h,buffer,sizeof(buffer),&count))throw std::runtime_error("Read failed");if(!count)break;if(body.size()+count>limit)throw std::runtime_error("Response too large");body.append(buffer,count);}
 return nlohmann::json::parse(body);
}
constexpr auto statusPath=L"/status?filter=status,name,singer,duration,progress,playbackRate";
void run() noexcept {
 try {Timeline timeline;std::string key;int64_t retry=0;auto demoStart=now();
 while(!stopping){try{State s;
 if(cfg.demo){s.connected=s.playing=true;s.title="Offline demonstration";s.key="demo";s.duration=16000;s.position=(now()-demoStart)%16000;s.sampled=now();s.timeline=parse("[00:01.00]<1000,500>你好 <1500,500>求生之路\n[00:05.00]Left and right floating lyrics\n[00:09.00]暂停、换歌与逐字时间轴\n[00:13.00]LDBDangoMusic");s.message="Offline demo";}
 else {s=status(get(statusPath,65536),now());if(s.key!=key){key=s.key;timeline.clear();retry=0;}
 if(!key.empty()&&timeline.empty()&&now()>=retry){publish(s);retry=now()+5000;auto next=lyrics(get(L"/lyric-all",2*1024*1024));auto verified=status(get(statusPath,65536),now());if(verified.key!=key){key.clear();continue;}s=std::move(verified);timeline=std::move(next);retry=now()+5000;}
 s.timeline=timeline;if(!timeline.empty())s.message="Connected to BLF / LX";}
 publish(std::move(s));
 }catch(const std::exception&){auto old=std::atomic_load(&snapshot);if(now()-old->sampled>3000){State s;s.message="Disconnected: enable BLF / LX local API";publish(std::move(s));}}
 std::unique_lock<std::mutex> lock(sleepMutex);wake.wait_for(lock,std::chrono::milliseconds(cfg.demo?50:500),[]{return stopping.load();});
 }
 }catch(...){if(logger)logger(LDB_HOST_LOG_ERROR,"Music worker stopped unexpectedly");}
}
std::wstring wide(const std::string& s){int n=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,s.data(),static_cast<int>(s.size()),nullptr,0);if(n<=0)return L"";std::wstring w(n,0);MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,s.data(),static_cast<int>(s.size()),w.data(),n);return w;}
struct Image {IDirect3DTexture9* texture=nullptr;int width=0,height=0;};
std::map<std::string,Image> cache;
void clear(){for(auto& pair:cache)if(pair.second.texture)pair.second.texture->Release();cache.clear();}
Image raster(IDirect3DDevice9* dev,const std::string& text){
 auto w=wide(text);if(w.empty()||w.size()>1024)return {};
 HDC dc=CreateCompatibleDC(nullptr);if(!dc)return {};HFONT font=CreateFontW(-cfg.size,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,ANTIALIASED_QUALITY,DEFAULT_PITCH,L"Microsoft YaHei");auto oldFont=SelectObject(dc,font);
 SIZE size{};GetTextExtentPoint32W(dc,w.data(),static_cast<int>(w.size()),&size);int width=std::clamp<int>(size.cx+8,8,4096),height=cfg.size*2;
 BITMAPINFO bi{};bi.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);bi.bmiHeader.biWidth=width;bi.bmiHeader.biHeight=-height;bi.bmiHeader.biPlanes=1;bi.bmiHeader.biBitCount=32;bi.bmiHeader.biCompression=BI_RGB;void* bits=nullptr;
 HBITMAP bmp=CreateDIBSection(dc,&bi,DIB_RGB_COLORS,&bits,nullptr,0);Image result;
 if(bmp&&bits){auto old=SelectObject(dc,bmp);PatBlt(dc,0,0,width,height,BLACKNESS);SetBkMode(dc,TRANSPARENT);SetTextColor(dc,RGB(255,255,255));TextOutW(dc,4,2,w.data(),static_cast<int>(w.size()));GdiFlush();
 IDirect3DTexture9* texture=nullptr;if(SUCCEEDED(dev->CreateTexture(width,height,1,D3DUSAGE_DYNAMIC,D3DFMT_A8R8G8B8,D3DPOOL_DEFAULT,&texture,nullptr))){D3DLOCKED_RECT rect{};if(SUCCEEDED(texture->LockRect(0,&rect,nullptr,D3DLOCK_DISCARD))){for(int y=0;y<height;++y){auto dst=reinterpret_cast<DWORD*>(static_cast<char*>(rect.pBits)+y*rect.Pitch);auto src=static_cast<DWORD*>(bits)+y*width;for(int x=0;x<width;++x){DWORD alpha=std::max({src[x]&255,(src[x]>>8)&255,(src[x]>>16)&255});dst[x]=(alpha<<24)|0xffffff;}}texture->UnlockRect(0);result={texture,width,height};}else texture->Release();}
 SelectObject(dc,old);DeleteObject(bmp);}
 SelectObject(dc,oldFont);DeleteObject(font);DeleteDC(dc);return result;
}
Image image(IDirect3DDevice9* dev,const std::string& text){auto it=cache.find(text);if(it!=cache.end())return it->second;auto im=raster(dev,text);cache.emplace(text,im);return im;}
struct Vertex{float x,y,z,rhw;DWORD color;float u,v;};
void draw(IDirect3DDevice9* dev,Image im,float x,float y,float scale,float alpha){if(!im.texture||alpha<=0)return;float r=x+im.width*scale,b=y+im.height*scale;DWORD color=(DWORD(std::clamp(alpha,0.f,1.f)*255)<<24)|0xffffff;Vertex v[]={{x-.5f,y-.5f,0,1,color,0,0},{r-.5f,y-.5f,0,1,color,1,0},{x-.5f,b-.5f,0,1,color,0,1},{r-.5f,b-.5f,0,1,color,1,1}};dev->SetTexture(0,im.texture);dev->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP,2,v,sizeof(Vertex));}
struct DrawGuard {
 IDirect3DDevice9* dev; IDirect3DStateBlock9* block=nullptr;
 IDirect3DSurface9 *target=nullptr,*depth=nullptr,*back=nullptr; bool scene=false;
 explicit DrawGuard(IDirect3DDevice9* d):dev(d){}
 ~DrawGuard(){if(scene)dev->EndScene();if(block){if(target)dev->SetRenderTarget(0,target);dev->SetDepthStencilSurface(depth);block->Apply();block->Release();}if(back)back->Release();if(target)target->Release();if(depth)depth->Release();}
};
void render(IDirect3DDevice9* dev,const LDBHostPresentInfoV1* present){
 try {if(cache.size()>128)clear();auto state=std::atomic_load(&snapshot);auto position=state->at(now());auto at=position<0?-1:indexAt(state->timeline,position+cfg.offset);
 std::string text=at<0?state->message:state->timeline[at].text;
 DrawGuard guard(dev);auto& target=guard.target;auto& depth=guard.depth;auto& back=guard.back;auto& block=guard.block;
 if(FAILED(dev->CreateStateBlock(D3DSBT_ALL,&block)))return;
 dev->GetRenderTarget(0,&target);dev->GetDepthStencilSurface(&depth);
 // SwapChain Present does not identify its swap chain in ABI v1. Skip it rather than paint the wrong target.
 if(present&&present->is_swapchain_present){return;}
 if(SUCCEEDED(dev->GetBackBuffer(0,0,D3DBACKBUFFER_TYPE_MONO,&back))){D3DSURFACE_DESC desc{};back->GetDesc(&desc);dev->SetRenderTarget(0,back);dev->SetDepthStencilSurface(nullptr);D3DVIEWPORT9 vp{0,0,desc.Width,desc.Height,0,1};dev->SetViewport(&vp);
 if(SUCCEEDED(dev->BeginScene())){guard.scene=true;
 dev->SetVertexShader(nullptr);dev->SetPixelShader(nullptr);dev->SetFVF(D3DFVF_XYZRHW|D3DFVF_DIFFUSE|D3DFVF_TEX1);
 dev->SetRenderState(D3DRS_FILLMODE,D3DFILL_SOLID);dev->SetRenderState(D3DRS_ZENABLE,FALSE);dev->SetRenderState(D3DRS_ZWRITEENABLE,FALSE);dev->SetRenderState(D3DRS_CULLMODE,D3DCULL_NONE);dev->SetRenderState(D3DRS_LIGHTING,FALSE);dev->SetRenderState(D3DRS_FOGENABLE,FALSE);dev->SetRenderState(D3DRS_STENCILENABLE,FALSE);dev->SetRenderState(D3DRS_SCISSORTESTENABLE,FALSE);dev->SetRenderState(D3DRS_ALPHATESTENABLE,FALSE);dev->SetRenderState(D3DRS_ALPHABLENDENABLE,TRUE);dev->SetRenderState(D3DRS_SRCBLEND,D3DBLEND_SRCALPHA);dev->SetRenderState(D3DRS_DESTBLEND,D3DBLEND_INVSRCALPHA);dev->SetRenderState(D3DRS_BLENDOP,D3DBLENDOP_ADD);dev->SetRenderState(D3DRS_SEPARATEALPHABLENDENABLE,FALSE);dev->SetRenderState(D3DRS_COLORWRITEENABLE,15);dev->SetRenderState(D3DRS_SRGBWRITEENABLE,FALSE);
 for(auto stage:{D3DTSS_COLOROP,D3DTSS_ALPHAOP})dev->SetTextureStageState(0,stage,D3DTOP_MODULATE);
 dev->SetTextureStageState(0,D3DTSS_COLORARG1,D3DTA_TEXTURE);dev->SetTextureStageState(0,D3DTSS_COLORARG2,D3DTA_DIFFUSE);dev->SetTextureStageState(0,D3DTSS_ALPHAARG1,D3DTA_TEXTURE);dev->SetTextureStageState(0,D3DTSS_ALPHAARG2,D3DTA_DIFFUSE);dev->SetTextureStageState(0,D3DTSS_TEXCOORDINDEX,0);dev->SetTextureStageState(0,D3DTSS_TEXTURETRANSFORMFLAGS,D3DTTFF_DISABLE);dev->SetTextureStageState(1,D3DTSS_COLOROP,D3DTOP_DISABLE);dev->SetTextureStageState(1,D3DTSS_ALPHAOP,D3DTOP_DISABLE);
 dev->SetSamplerState(0,D3DSAMP_MINFILTER,D3DTEXF_LINEAR);dev->SetSamplerState(0,D3DSAMP_MAGFILTER,D3DTEXF_LINEAR);dev->SetSamplerState(0,D3DSAMP_ADDRESSU,D3DTADDRESS_CLAMP);dev->SetSamplerState(0,D3DSAMP_ADDRESSV,D3DTADDRESS_CLAMP);dev->SetSamplerState(0,D3DSAMP_SRGBTEXTURE,FALSE);
 auto im=image(dev,text);float scale=std::min(1.f,desc.Width*.7f/std::max(im.width,1));float center=desc.Width*((cfg.alternate&&at>=0)?(at%2?.67f:.33f):.5f);float x=std::clamp(center-im.width*scale*.5f,8.f,std::max(8.f,desc.Width-im.width*scale-8));float y=desc.Height*cfg.y;
 if(at>=0){const auto& line=state->timeline[at];auto elapsed=position+cfg.offset-line.start;float fade=std::clamp(elapsed/250.f,0.f,1.f);y+=(1-fade)*8;
 if(line.words.empty())draw(dev,im,x,y,scale,fade);
 else {float pen=x;for(auto& word:line.words){auto wi=image(dev,word.text);auto age=position+cfg.offset-word.start;float alpha=std::clamp(age/120.f,0.f,1.f);draw(dev,wi,pen,y+(1-alpha)*6,scale,alpha);pen+=std::max(0,wi.width-8)*scale;}}
 if(cfg.translation&&!line.translation.empty()){auto tr=image(dev,line.translation);draw(dev,tr,std::max(8.f,center-tr.width*scale*.4f),y+cfg.size*1.5f,scale*.8f,fade*.8f);}
 }else draw(dev,im,x,y,scale,.7f);
 dev->EndScene();guard.scene=false;}
 }
 
 }catch(...){if(logger)logger(LDB_HOST_LOG_ERROR,"Lyric render failed");}
}
BOOL WINAPI load(const LDBHostApiV1* host){try{if(!host||host->struct_size<sizeof(*host)||host->abi_version!=1||host->host_bits!=64)return FALSE;logger=host->log;directory=std::filesystem::path(host->host_directory)/L"plugins"/L"LDBDangoMusic";auto path=directory/L"config.json";
 if(std::filesystem::exists(path)){std::ifstream f(path);auto j=nlohmann::json::parse(f);cfg.port=std::clamp(j.value("port",23330),1,65535);cfg.size=std::clamp(j.value("fontSize",32),12,96);cfg.demo=j.value("demo",false);cfg.translation=j.value("translation",true);cfg.alternate=j.value("alternate",true);cfg.y=std::clamp(j.value("verticalPosition",.72f),.1f,.85f);cfg.offset=std::clamp(j.value("offsetMs",0),-10000,10000);}
 stopping=false;worker=std::thread(run);if(logger)logger(LDB_HOST_LOG_INFO,"LDBDangoMusic loaded (x64 prototype)");return TRUE;}catch(...){return FALSE;}}
void WINAPI created(IDirect3DDevice9*){clear();}
void WINAPI beforeReset(IDirect3DDevice9*,const D3DPRESENT_PARAMETERS*,BOOL){clear();}
void WINAPI afterReset(IDirect3DDevice9*,HRESULT,BOOL){}
void WINAPI shutdown(){stopping=true;wake.notify_all();if(worker.joinable())worker.join();clear();}
}
extern "C" __declspec(dllexport) BOOL WINAPI LDBHostPluginQuery(uint32_t abi,LDBHostPluginV1* out){if(abi!=1||!out||out->struct_size<sizeof(*out))return FALSE;*out={sizeof(*out),1,"LDBDangoMusic","0.1.0",load,created,render,beforeReset,afterReset,created,shutdown};return TRUE;}
