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
#include <cstdio>
#include <cstddef>
#include <cstring>
#include "ldb_host_plugin.h"
#include "core.hpp"
#include "present_target.hpp"
#include "presentation.hpp"
#include "camera_diagnostics.hpp"
static_assert(sizeof(LDBHostPluginV1)==96, "Requires updated x64 Host SDK");
static_assert(sizeof(LDBHostPresentInfoV1)==56, "Requires updated x64 Present SDK");
namespace {
using namespace dango;
int64_t now(){return static_cast<int64_t>(GetTickCount64());}
struct Config {int port=23330,size=32;bool cameraDiagnostics=false,demo=true,translation=true,alternate=true;float y=.72f;int offset=500;bool outline=true,glow=true;float glowStrength=.18f,opacity=1.f,width=.44f;int transition=380;std::string mode="character";DWORD textColor=0xfff4eaff,accent=0xffb9dfff;};
Config cfg;std::filesystem::path directory;LDBHostLogFn logger=nullptr;
std::atomic<bool> stopping{false};std::thread worker;std::mutex sleepMutex;std::condition_variable wake;
std::shared_ptr<const State> snapshot=std::make_shared<State>();
bool presentLogged[2]={false,false};uint64_t reportedErrors=0;bool drewPixels=false;
void note(const std::string& message){if(logger)logger(LDB_HOST_LOG_INFO,message.c_str());}
CameraQueue<> cameraQueue;uint64_t nextCameraCapture=0;uint32_t deviceGeneration=0;
void captureCamera(IDirect3DDevice9* device,const LDBHostPresentInfoV1* present){
 if(!cfg.cameraDiagnostics)return;auto tick=uint64_t(now());if(tick<nextCameraCapture)return;nextCameraCapture=tick+250;
 CameraSample s;s.tick=tick;s.chain=reinterpret_cast<uintptr_t>(present->swap_chain);s.path=present->is_swapchain_present;s.generation=deviceGeneration;
 D3DMATRIX view{},projection{};D3DVIEWPORT9 viewport{};
 s.viewResult=device->GetTransform(D3DTS_VIEW,&view);s.projectionResult=device->GetTransform(D3DTS_PROJECTION,&projection);s.viewportResult=device->GetViewport(&viewport);
 std::memcpy(s.view.data(),&view,sizeof(view));std::memcpy(s.projection.data(),&projection,sizeof(projection));
 s.viewport={float(viewport.X),float(viewport.Y),float(viewport.Width),float(viewport.Height),viewport.MinZ,viewport.MaxZ};cameraQueue.push(s);
}
struct CameraWriter {
 std::ofstream file;size_t bytes=0,records=0;bool attempted=false,disabled=false;
 void drain(){if(!cfg.cameraDiagnostics)return;
 if(!attempted){attempted=true;try{std::filesystem::create_directories(directory);auto path=directory/("camera-"+std::to_string(GetCurrentProcessId())+"-"+std::to_string(now())+".jsonl");file.open(path,std::ios::out|std::ios::binary|std::ios::app);if(!file)throw std::runtime_error("Cannot open diagnostic file");note("LDBDangoMusic camera diagnostic="+path.u8string()+" sample=4Hz max=2MiB/2400records; matrices unconfirmed");}catch(const std::exception& e){disabled=true;note(std::string("LDBDangoMusic camera diagnostic disabled: ")+e.what());}}
 CameraSample s;while(cameraQueue.pop(s)){if(disabled)continue;auto json=cameraJson(s);json["dropped_total"]=cameraQueue.dropped.load();auto line=json.dump()+"\n";if(bytes+line.size()>2*1024*1024||records>=2400){disabled=true;note("LDBDangoMusic camera diagnostic reached limit; restart to capture again");continue;}file<<line;if(!file){disabled=true;note("LDBDangoMusic camera diagnostic write failed");continue;}bytes+=line.size();++records;}if(file)file.flush();
 }
};
bool checked(HRESULT hr,unsigned stage,const char* operation){if(SUCCEEDED(hr))return true;auto bit=uint64_t(1)<<stage;if(!(reportedErrors&bit)){reportedErrors|=bit;char msg[256];std::snprintf(msg,sizeof(msg),"LDBDangoMusic render-error operation=%s hr=0x%08lx",operation,static_cast<unsigned long>(hr));if(logger)logger(LDB_HOST_LOG_WARNING,msg);}return false;}
void publish(State s){std::atomic_store(&snapshot,std::make_shared<const State>(std::move(s)));}
struct Internet {HINTERNET h; explicit Internet(HINTERNET v):h(v){if(!h)throw std::runtime_error("WinHTTP handle failed");}~Internet(){WinHttpCloseHandle(h);}Internet(const Internet&)=delete;};
nlohmann::json get(const wchar_t* path,size_t limit){
 Internet session(WinHttpOpen(L"LDBDangoMusic/0.3.1",WINHTTP_ACCESS_TYPE_NO_PROXY,WINHTTP_NO_PROXY_NAME,WINHTTP_NO_PROXY_BYPASS,0));
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
 try {CameraWriter cameraWriter;Timeline timeline;std::string key;int64_t retry=0;auto demoStart=now();
 while(!stopping){cameraWriter.drain();try{State s;
 if(cfg.demo){s.connected=s.playing=true;s.title="Offline demonstration";s.key="demo";s.duration=16000;s.position=(now()-demoStart)%16000;s.sampled=now();s.timeline=parse("[00:01.00]<1000,500>你好 <1500,500>求生之路\n[00:05.00]Left and right floating lyrics\n[00:09.00]暂停、换歌与逐字时间轴\n[00:13.00]LDBDangoMusic");s.message="Offline demo";}
 else {s=status(get(statusPath,65536),now());if(s.key!=key){key=s.key;timeline.clear();retry=0;}
 if(!key.empty()&&timeline.empty()&&now()>=retry){publish(s);retry=now()+5000;auto next=lyrics(get(L"/lyric-all",2*1024*1024));auto verified=status(get(statusPath,65536),now());if(verified.key!=key){key.clear();continue;}s=std::move(verified);timeline=std::move(next);retry=now()+5000;}
 s.timeline=timeline;if(!timeline.empty())s.message="Connected to BLF / LX";}
 publish(std::move(s));
 }catch(const std::exception&){auto old=std::atomic_load(&snapshot);if(now()-old->sampled>3000){State s;s.message="Disconnected: enable BLF / LX local API";publish(std::move(s));}}
 std::unique_lock<std::mutex> lock(sleepMutex);wake.wait_for(lock,std::chrono::milliseconds(cfg.demo?50:150),[]{return stopping.load();});
 }
 cameraWriter.drain();
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
Image image(IDirect3DDevice9* dev,const std::string& text){auto it=cache.find(text);if(it!=cache.end())return it->second;auto im=raster(dev,text);if(im.texture)cache.emplace(text,im);else checked(E_FAIL,7,"Create glyph texture (GDI/CreateTexture/LockRect)");return im;}
struct Vertex{float x,y,z,rhw;DWORD color;float u,v;};
void draw(IDirect3DDevice9* dev,Image im,float x,float y,float scale,float alpha,DWORD tint=0xffffff){if(!im.texture||alpha<=0)return;float r=x+im.width*scale,b=y+im.height*scale;DWORD color=(DWORD(std::clamp(alpha,0.f,1.f)*255)<<24)|(tint&0xffffff);Vertex v[]={{x-.5f,y-.5f,0,1,color,0,0},{r-.5f,y-.5f,0,1,color,1,0},{x-.5f,b-.5f,0,1,color,0,1},{r-.5f,b-.5f,0,1,color,1,1}};dev->SetTexture(0,im.texture);if(checked(dev->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP,2,v,sizeof(Vertex)),6,"DrawPrimitiveUP")&&!drewPixels){drewPixels=true;note("LDBDangoMusic first textured draw submitted");}}
void styled(IDirect3DDevice9* dev,Image im,float x,float y,float scale,float alpha,DWORD color){
 if(alpha<=.002f||!im.texture)return;
 if(cfg.glow&&cfg.glowStrength>0){for(auto delta:{-3.f,3.f}){draw(dev,im,x+delta*scale,y,scale,alpha*cfg.glowStrength,cfg.accent);draw(dev,im,x,y+delta*scale,scale,alpha*cfg.glowStrength,cfg.accent);}}
 if(cfg.outline){for(auto dx:{-1.f,1.f})for(auto dy:{-1.f,1.f})draw(dev,im,x+dx*scale,y+dy*scale,scale,alpha*.8f,0x101018);}
 draw(dev,im,x,y,scale,alpha,color);
}
void lineFrame(IDirect3DDevice9* dev,const Timeline& timeline,int at,int64_t song,float width,float height,float externalAlpha=1,float outgoingLift=0,bool completed=false){
 const auto& line=timeline[at];int64_t next=at+1<int(timeline.size())?timeline[at+1].start:line.start+5000;
 auto whole=cfg.mode=="line"?image(dev,line.text):Image{};auto units=reveal(line,next,cfg.mode=="character");
 float measured=0;for(auto& unit:units)measured+=std::max(0,image(dev,unit.text).width-8);
 if(cfg.mode=="line")measured=float(std::max(0,whole.width-8));
 float scale=std::min(1.f,width*cfg.width/std::max(measured+8,1.f));float center=width*(cfg.alternate?(at%2?.72f:.28f):.5f);
 float left=std::clamp(center-(measured+8)*scale*.5f,8.f,std::max(8.f,width-(measured+8)*scale-8));
 auto entry=motion(song-line.start,180);float alpha=externalAlpha*cfg.opacity;float y=height*cfg.y+(completed?0:entry.lift)-outgoingLift;
 if(cfg.mode=="line")styled(dev,whole,left,y,scale,alpha*(completed?1:entry.alpha),cfg.textColor);
 else{float pen=left;for(auto& unit:units){auto im=image(dev,unit.text);auto frame=motion(song-unit.start,140);if(completed)frame={1,1,0};float pop=scale*frame.scale;if(unit.text.find_first_not_of(" \t\r\n")!=std::string::npos)styled(dev,im,pen-im.width*(pop-scale)*.5f,y+frame.lift,pop,alpha*frame.alpha,cfg.textColor);pen+=std::max(0,im.width-8)*scale;}}
 if(cfg.translation&&!line.translation.empty()){auto tr=image(dev,line.translation);float ts=std::min(scale*.76f,width*cfg.width/std::max(tr.width,1));styled(dev,tr,std::clamp(center-tr.width*ts*.5f,8.f,std::max(8.f,width-tr.width*ts-8)),y+cfg.size*scale*1.5f,ts,alpha*.8f*(completed?1:entry.alpha),cfg.accent);}
}
struct DrawGuard {
 IDirect3DDevice9* dev; IDirect3DStateBlock9* block=nullptr;
 IDirect3DSurface9 *target=nullptr,*depth=nullptr,*back=nullptr;
 IDirect3DSurface9* extra[3]={nullptr,nullptr,nullptr};unsigned extraCount=0;bool captured=false;bool scene=false;
 explicit DrawGuard(IDirect3DDevice9* d):dev(d){}
 ~DrawGuard(){if(scene)dev->EndScene();if(block){if(captured){dev->SetDepthStencilSurface(nullptr);if(target)dev->SetRenderTarget(0,target);for(unsigned i=0;i<extraCount;++i)dev->SetRenderTarget(i+1,extra[i]);dev->SetDepthStencilSurface(depth);}block->Apply();block->Release();}for(auto* surface:extra)if(surface)surface->Release();if(back)back->Release();if(target)target->Release();if(depth)depth->Release();}
};
void WINAPI render(IDirect3DDevice9* dev,const LDBHostPresentInfoV1* present){
 if(!dev||!present||present->struct_size!=sizeof(*present))return;
 captureCamera(dev,present);
 const unsigned path=present->is_swapchain_present?1:0;
 if(!presentLogged[path]){presentLogged[path]=true;note(std::string("LDBDangoMusic first Present path=")+(path?"SwapChain":"Device")+(present->swap_chain?" current-chain=yes":" current-chain=no"));}
 try {if(cache.size()>256)clear();auto state=std::atomic_load(&snapshot);auto position=state->at(now());auto at=position<0?-1:indexAt(state->timeline,lyricPosition(position,cfg.demo?0:cfg.offset));
 std::string text=at<0?state->message:state->timeline[at].text;
 DrawGuard guard(dev);auto& target=guard.target;auto& depth=guard.depth;auto& back=guard.back;auto& block=guard.block;
 if(!checked(dev->CreateStateBlock(D3DSBT_ALL,&block),0,"CreateStateBlock"))return;
 if(!checked(dev->GetRenderTarget(0,&target),1,"GetRenderTarget"))return;dev->GetDepthStencilSurface(&depth);
 D3DCAPS9 caps{};if(!checked(dev->GetDeviceCaps(&caps),8,"GetDeviceCaps"))return;
 guard.extraCount=std::max(1u,std::min<unsigned>(caps.NumSimultaneousRTs,4))-1;
 for(unsigned i=0;i<guard.extraCount;++i)dev->GetRenderTarget(i+1,&guard.extra[i]);guard.captured=true;
 // The supplied chain is borrowed. Only GetBackBuffer's returned reference is released.
 HRESULT backResult=currentBackBuffer(dev,present->swap_chain,path!=0,D3DBACKBUFFER_TYPE_MONO,&back,D3DERR_INVALIDCALL);
 if(checked(backResult,2,"Get current SwapChain backbuffer")){D3DSURFACE_DESC desc{};if(!checked(back->GetDesc(&desc),9,"Get backbuffer description"))return;dev->SetDepthStencilSurface(nullptr);for(unsigned i=0;i<guard.extraCount;++i)dev->SetRenderTarget(i+1,nullptr);if(!checked(dev->SetRenderTarget(0,back),3,"SetRenderTarget"))return;D3DVIEWPORT9 vp{0,0,desc.Width,desc.Height,0,1};if(!checked(dev->SetViewport(&vp),10,"SetViewport"))return;
 if(checked(dev->BeginScene(),4,"BeginScene")){guard.scene=true;
 dev->SetVertexShader(nullptr);dev->SetPixelShader(nullptr);dev->SetFVF(D3DFVF_XYZRHW|D3DFVF_DIFFUSE|D3DFVF_TEX1);
 dev->SetRenderState(D3DRS_FILLMODE,D3DFILL_SOLID);dev->SetRenderState(D3DRS_ZENABLE,FALSE);dev->SetRenderState(D3DRS_ZWRITEENABLE,FALSE);dev->SetRenderState(D3DRS_CULLMODE,D3DCULL_NONE);dev->SetRenderState(D3DRS_LIGHTING,FALSE);dev->SetRenderState(D3DRS_FOGENABLE,FALSE);dev->SetRenderState(D3DRS_STENCILENABLE,FALSE);dev->SetRenderState(D3DRS_SCISSORTESTENABLE,FALSE);dev->SetRenderState(D3DRS_ALPHATESTENABLE,FALSE);dev->SetRenderState(D3DRS_ALPHABLENDENABLE,TRUE);dev->SetRenderState(D3DRS_SRCBLEND,D3DBLEND_SRCALPHA);dev->SetRenderState(D3DRS_DESTBLEND,D3DBLEND_INVSRCALPHA);dev->SetRenderState(D3DRS_BLENDOP,D3DBLENDOP_ADD);dev->SetRenderState(D3DRS_SEPARATEALPHABLENDENABLE,FALSE);dev->SetRenderState(D3DRS_COLORWRITEENABLE,15);dev->SetRenderState(D3DRS_SRGBWRITEENABLE,FALSE);
 for(auto stage:{D3DTSS_COLOROP,D3DTSS_ALPHAOP})dev->SetTextureStageState(0,stage,D3DTOP_MODULATE);
 dev->SetTextureStageState(0,D3DTSS_COLORARG1,D3DTA_TEXTURE);dev->SetTextureStageState(0,D3DTSS_COLORARG2,D3DTA_DIFFUSE);dev->SetTextureStageState(0,D3DTSS_ALPHAARG1,D3DTA_TEXTURE);dev->SetTextureStageState(0,D3DTSS_ALPHAARG2,D3DTA_DIFFUSE);dev->SetTextureStageState(0,D3DTSS_TEXCOORDINDEX,0);dev->SetTextureStageState(0,D3DTSS_TEXTURETRANSFORMFLAGS,D3DTTFF_DISABLE);dev->SetTextureStageState(1,D3DTSS_COLOROP,D3DTOP_DISABLE);dev->SetTextureStageState(1,D3DTSS_ALPHAOP,D3DTOP_DISABLE);
 dev->SetSamplerState(0,D3DSAMP_MINFILTER,D3DTEXF_LINEAR);dev->SetSamplerState(0,D3DSAMP_MAGFILTER,D3DTEXF_LINEAR);dev->SetSamplerState(0,D3DSAMP_ADDRESSU,D3DTADDRESS_CLAMP);dev->SetSamplerState(0,D3DSAMP_ADDRESSV,D3DTADDRESS_CLAMP);dev->SetSamplerState(0,D3DSAMP_SRGBTEXTURE,FALSE);
 if(cfg.demo){auto banner=image(dev,"LDBDangoMusic 0.3.1 DEMO - "+std::string(path?"SwapChain":"Device"));draw(dev,banner,16,16,std::min(1.f,desc.Width*.9f/std::max(banner.width,1)),1.f);}
 if(at>=0){auto song=lyricPosition(position,cfg.demo?0:cfg.offset);auto age=song-state->timeline[at].start;
 if(at>0&&age>=0&&age<cfg.transition){float fade=outgoingAlpha(age,cfg.transition);lineFrame(dev,state->timeline,at-1,song,float(desc.Width),float(desc.Height),fade,14.f*(1-fade),true);}
 lineFrame(dev,state->timeline,at,song,float(desc.Width),float(desc.Height));
 }else{auto im=image(dev,text);float scale=std::min(1.f,desc.Width*.8f/std::max(im.width,1));styled(dev,im,std::max(8.f,(desc.Width-im.width*scale)*.5f),desc.Height*cfg.y,scale,.7f,cfg.textColor);}
 dev->EndScene();guard.scene=false;}
 }

 }catch(...){if(logger)logger(LDB_HOST_LOG_ERROR,"Lyric render failed");}
}
BOOL WINAPI load(const LDBHostApiV1* host){try{if(!host||host->struct_size<sizeof(*host)||host->abi_version!=1||host->host_bits!=64)return FALSE;logger=host->log;directory=std::filesystem::path(host->host_directory)/L"plugins"/L"LDBDangoMusic";auto path=directory/L"config.json";
 if(std::filesystem::exists(path)){std::ifstream f(path);auto j=nlohmann::json::parse(f);cfg.cameraDiagnostics=j.value("cameraDiagnostics",false);cfg.port=std::clamp(j.value("port",23330),1,65535);cfg.size=std::clamp(j.value("fontSize",32),12,96);cfg.demo=j.value("demo",true);cfg.translation=j.value("translation",true);cfg.alternate=j.value("alternate",true);cfg.y=std::clamp(j.value("verticalPosition",.72f),.1f,.85f);cfg.offset=std::clamp(j.value("offsetMs",500),-10000,10000);
 cfg.outline=j.value("outline",true);cfg.glow=j.value("glow",true);cfg.glowStrength=std::clamp(j.value("glowStrength",.18f),0.f,.5f);cfg.opacity=std::clamp(j.value("opacity",1.f),.1f,1.f);cfg.width=std::clamp(j.value("maxWidth",.44f),.2f,.9f);cfg.transition=std::clamp(j.value("transitionMs",380),0,1000);cfg.mode=j.value("displayMode",std::string("character"));if(cfg.mode!="character"&&cfg.mode!="word"&&cfg.mode!="line")throw std::runtime_error("displayMode must be character/word/line");
 auto color=[&](const char* name,DWORD fallback){auto value=j.value(name,std::string{});if(value.empty())return fallback;if(value.size()!=7||value[0]!='#'||value.find_first_not_of("0123456789abcdefABCDEF",1)!=std::string::npos)throw std::runtime_error("Color must be #RRGGBB");return DWORD(std::stoul(value.substr(1),nullptr,16));};cfg.textColor=color("textColor",cfg.textColor);cfg.accent=color("accentColor",cfg.accent);}
 note("LDBDangoMusic 0.3.1 config="+path.u8string()+" mode="+(cfg.demo?"DEMO":"BLF/LX")+" port="+std::to_string(cfg.port)+" offsetMs="+std::to_string(cfg.offset)+" displayMode="+cfg.mode);
 stopping=false;worker=std::thread(run);return TRUE;}catch(const std::exception& e){if(logger)logger(LDB_HOST_LOG_ERROR,e.what());return FALSE;}catch(...){return FALSE;}}
void WINAPI created(IDirect3DDevice9*){++deviceGeneration;clear();reportedErrors=0;drewPixels=false;note("LDBDangoMusic device lifecycle: created/destroyed; textures cleared");}
void WINAPI swapCreated(IDirect3DSwapChain9*){note("LDBDangoMusic SwapChain created (borrowed)");}
void WINAPI swapDestroyed(IDirect3DSwapChain9*){note("LDBDangoMusic SwapChain destroyed (no retained references)");}
void WINAPI beforeReset(IDirect3DDevice9*,const D3DPRESENT_PARAMETERS*,BOOL){clear();}
void WINAPI afterReset(IDirect3DDevice9*,HRESULT hr,BOOL){++deviceGeneration;reportedErrors=0;checked(hr,5,"Reset");note("LDBDangoMusic Reset completed; textures will rebuild");}
void WINAPI shutdown(){stopping=true;wake.notify_all();if(worker.joinable())worker.join();clear();}
}
extern "C" __declspec(dllexport) BOOL WINAPI LDBHostPluginQuery(uint32_t abi,LDBHostPluginV1* out){if(abi!=1||!out||out->struct_size!=sizeof(*out))return FALSE;*out={sizeof(*out),1,"LDBDangoMusic","0.3.1",load,created,swapCreated,render,beforeReset,afterReset,swapDestroyed,created,shutdown};return TRUE;}
