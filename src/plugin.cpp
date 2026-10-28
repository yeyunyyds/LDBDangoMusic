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
#include "l4n/l4n_plugin.h"
#include "core.hpp"
#include "present_target.hpp"
#include "presentation.hpp"
#include "peak_style.hpp"
#include "lyrics_settings.hpp"
#include "render_lifecycle.hpp"
#include "camera_diagnostics.hpp"
#include "source_camera.hpp"
static_assert(sizeof(void*)==4,"Pure L4N plugin must be x86");
struct PresentInfo {bool is_swapchain_present=false;IDirect3DSwapChain9* swap_chain=nullptr;};
namespace {
using namespace dango;
int64_t now(){return static_cast<int64_t>(GetTickCount64());}
thread_local Config cfg;
std::shared_ptr<const Config> settings=std::make_shared<Config>(),pendingSave;
std::mutex settingsMutex;std::atomic<int> saveState{0};thread_local std::shared_ptr<const Config> renderedSettings;
std::filesystem::path directory;using LogFn=void(*)(int,const char*);LogFn logger=nullptr;
constexpr int LDB_HOST_LOG_INFO=0,LDB_HOST_LOG_WARNING=1,LDB_HOST_LOG_ERROR=2;
std::atomic<uint64_t> sceneSerial{1};thread_local bool overlayDrawing=false;FrameGate frameGate;
std::atomic<bool> stopping{false};std::thread worker;std::mutex sleepMutex;std::condition_variable wake;
std::shared_ptr<const State> snapshot=std::make_shared<State>();
bool presentLogged[2]={false,false};uint64_t reportedErrors=0;bool drewPixels=false;
void note(const std::string& message){if(logger)logger(LDB_HOST_LOG_INFO,message.c_str());}
struct WorldAnchors {
 std::string key;uint32_t epoch=0;bool ready=false;int currentIndex=-1,previousIndex=-1;Vec3 current{},previous{},test{},lastEye{};
 void reset(){ready=false;currentIndex=previousIndex=-1;key.clear();}
 void update(const GameCameraFrame& frame,const CameraPose& pose,const std::string& song,int at){
 if(!ready||epoch!=frame.epoch||key!=song||length(pose.eye-lastEye)>1024){reset();ready=true;epoch=frame.epoch;key=song;test=anchorAt(pose,cfg.worldDistance,0,18);}
 lastEye=pose.eye;
 if(at>=0&&currentIndex!=at){previous=current;previousIndex=currentIndex;current=peak::position(pose,cfg.worldDistance,cfg.alternate?cfg.spread:0,unsigned(at),3.f+10.f*(peak::noise(unsigned(at),0,1,peak::seed(song))*.5f+.5f));currentIndex=at;}
 }
};
WorldAnchors anchors;
CameraQueue<> cameraQueue;uint64_t nextCameraCapture=0;uint32_t deviceGeneration=0;
void captureCamera(IDirect3DDevice9* device,const PresentInfo* present){
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
 if(!attempted){attempted=true;try{std::filesystem::create_directories(directory);auto path=directory/("camera-"+std::to_string(GetCurrentProcessId())+"-"+std::to_string(now())+".jsonl");file.open(path,std::ios::out|std::ios::binary|std::ios::app);if(!file)throw std::runtime_error("Cannot open diagnostic file");note("L4D2DangoMusic camera diagnostic="+path.u8string()+" sample=4Hz max=2MiB/2400records; matrices unconfirmed");}catch(const std::exception& e){disabled=true;note(std::string("L4D2DangoMusic camera diagnostic disabled: ")+e.what());}}
 CameraSample s;while(cameraQueue.pop(s)){if(disabled)continue;auto json=cameraJson(s);json["dropped_total"]=cameraQueue.dropped.load();json["overlay_scene_serial"]=sceneSerial.load();json["overlay_submitted"]=frameGate.painted.load();json["overlay_duplicates_skipped"]=frameGate.duplicates.load();auto camera=std::atomic_load(&source_camera::snapshot);json["source_camera"]={{"capture_state",source_camera::captureState.load()},{"captured_frames",source_camera::capturedFrames.load()},{"active",camera->active},{"tick_ms",camera->tick},{"epoch",camera->epoch},{"world_to_view",camera->worldToView},{"world_to_clip",camera->worldToClip},{"width",camera->width},{"height",camera->height}};auto line=json.dump()+"\n";if(bytes+line.size()>2*1024*1024||records>=2400){disabled=true;note("L4D2DangoMusic camera diagnostic reached limit; restart to capture again");continue;}file<<line;if(!file){disabled=true;note("L4D2DangoMusic camera diagnostic write failed");continue;}bytes+=line.size();++records;}if(file)file.flush();
 }
};
bool checked(HRESULT hr,unsigned stage,const char* operation){if(SUCCEEDED(hr))return true;auto bit=uint64_t(1)<<stage;if(!(reportedErrors&bit)){reportedErrors|=bit;char msg[256];std::snprintf(msg,sizeof(msg),"L4D2DangoMusic render-error operation=%s hr=0x%08lx",operation,static_cast<unsigned long>(hr));if(logger)logger(LDB_HOST_LOG_WARNING,msg);}return false;}
void publish(State s){std::atomic_store(&snapshot,std::make_shared<const State>(std::move(s)));}
struct Internet {HINTERNET h; explicit Internet(HINTERNET v):h(v){if(!h)throw std::runtime_error("WinHTTP handle failed");}~Internet(){WinHttpCloseHandle(h);}Internet(const Internet&)=delete;};
nlohmann::json get(const wchar_t* path,size_t limit){
 Internet session(WinHttpOpen(L"L4D2DangoMusic/0.6.0",WINHTTP_ACCESS_TYPE_NO_PROXY,WINHTTP_NO_PROXY_NAME,WINHTTP_NO_PROXY_BYPASS,0));
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
 try {cfg=*std::atomic_load(&settings);CameraWriter cameraWriter;Timeline timeline;std::string key;int64_t retry=0;auto demoStart=now();
 while(!stopping||std::atomic_load(&pendingSave)){
 if(auto requested=std::atomic_exchange(&pendingSave,std::shared_ptr<const Config>{})){try{auto path=directory/L"config.json";nlohmann::json stored=nlohmann::json::object();if(std::filesystem::exists(path)){std::ifstream old(path);stored=nlohmann::json::parse(old);if(!stored.is_object())throw std::runtime_error("Existing config is not an object");}stored.update(encodeConfig(*requested));std::filesystem::create_directories(directory);auto temp=directory/L"config.json.tmp";{std::ofstream file(temp,std::ios::binary|std::ios::trunc);file<<stored.dump(2)<<'\n';file.flush();if(!file)throw std::runtime_error("Config write failed");}if(std::filesystem::exists(path))std::filesystem::copy_file(path,directory/L"config.json.bak",std::filesystem::copy_options::overwrite_existing);if(!MoveFileExW(temp.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))throw std::runtime_error("Config replace failed");saveState=2;note("Settings saved");}catch(const std::exception& e){saveState=-1;note(e.what());}}
 cameraWriter.drain();try{State s;
 if(cfg.demo){s.connected=s.playing=true;s.title="Offline demonstration";s.key="demo";s.duration=16000;s.position=(now()-demoStart)%16000;s.sampled=now();s.timeline=parse("[00:01.00]<1000,500>你好 <1500,500>求生之路\n[00:05.00]Left and right floating lyrics\n[00:09.00]暂停、换歌与逐字时间轴\n[00:13.00]L4D2DangoMusic");s.message="Offline demo";}
 else {s=status(get(statusPath,65536),now());if(s.key!=key){key=s.key;timeline.clear();retry=0;}
 if(!key.empty()&&timeline.empty()&&now()>=retry){publish(s);retry=now()+5000;auto next=lyrics(get(L"/lyric-all",2*1024*1024));auto verified=status(get(statusPath,65536),now());if(verified.key!=key){key.clear();continue;}s=std::move(verified);timeline=peak::splitTimeline(next);retry=now()+5000;}
 s.timeline=timeline;if(!timeline.empty())s.message="Connected to BLF / LX";}
 publish(std::move(s));
 }catch(const std::exception&){auto old=std::atomic_load(&snapshot);if(now()-old->sampled>3000){State s;s.message="Disconnected: enable BLF / LX local API";publish(std::move(s));}}
 std::unique_lock<std::mutex> lock(sleepMutex);wake.wait_for(lock,std::chrono::milliseconds(cfg.demo?50:150),[]{return stopping.load();});
 }
 cameraWriter.drain();
 }catch(...){if(logger)logger(LDB_HOST_LOG_ERROR,"Music worker stopped unexpectedly");}
}
std::wstring wide(const std::string& s){int n=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,s.data(),static_cast<int>(s.size()),nullptr,0);if(n<=0)return L"";std::wstring w(n,0);MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,s.data(),static_cast<int>(s.size()),w.data(),n);return w;}
struct Image {IDirect3DTexture9* texture=nullptr;int width=0,height=0;IDirect3DTexture9* halo=nullptr;float advance=0;};
std::map<std::string,Image> cache;
void clear(){for(auto& pair:cache){if(pair.second.texture)pair.second.texture->Release();if(pair.second.halo)pair.second.halo->Release();}cache.clear();}
Image raster(IDirect3DDevice9* dev,const std::string& text){
 auto w=wide(text);if(w.empty()||w.size()>1024)return {};
 HDC dc=CreateCompatibleDC(nullptr);if(!dc)return {};HFONT font=CreateFontW(-cfg.size,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,ANTIALIASED_QUALITY,DEFAULT_PITCH,L"Microsoft YaHei");auto oldFont=SelectObject(dc,font);
 SIZE size{};GetTextExtentPoint32W(dc,w.data(),static_cast<int>(w.size()),&size);int width=std::clamp<int>(size.cx+32,32,4096),height=cfg.size+32;
 BITMAPINFO bi{};bi.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);bi.bmiHeader.biWidth=width;bi.bmiHeader.biHeight=-height;bi.bmiHeader.biPlanes=1;bi.bmiHeader.biBitCount=32;bi.bmiHeader.biCompression=BI_RGB;void* bits=nullptr;
 HBITMAP bmp=CreateDIBSection(dc,&bi,DIB_RGB_COLORS,&bits,nullptr,0);Image result;
 if(bmp&&bits){auto old=SelectObject(dc,bmp);PatBlt(dc,0,0,width,height,BLACKNESS);SetBkMode(dc,TRANSPARENT);SetTextColor(dc,RGB(255,255,255));TextOutW(dc,16,16,w.data(),static_cast<int>(w.size()));GdiFlush();
 IDirect3DTexture9* texture=nullptr;if(SUCCEEDED(dev->CreateTexture(width,height,1,D3DUSAGE_DYNAMIC,D3DFMT_A8R8G8B8,D3DPOOL_DEFAULT,&texture,nullptr))){D3DLOCKED_RECT rect{};if(SUCCEEDED(texture->LockRect(0,&rect,nullptr,D3DLOCK_DISCARD))){for(int y=0;y<height;++y){auto dst=reinterpret_cast<DWORD*>(static_cast<char*>(rect.pBits)+y*rect.Pitch);auto src=static_cast<DWORD*>(bits)+y*width;for(int x=0;x<width;++x){DWORD alpha=std::max({src[x]&255,(src[x]>>8)&255,(src[x]>>16)&255});dst[x]=(alpha<<24)|0xffffff;}}texture->UnlockRect(0);result={texture,width,height,nullptr,float(size.cx)};}else texture->Release();}
 if(result.texture&&cfg.glow){std::vector<float> mask(size_t(width)*height);auto pixels=static_cast<DWORD*>(bits);for(size_t i=0;i<mask.size();++i)mask[i]=float(std::max({pixels[i]&255,(pixels[i]>>8)&255,(pixels[i]>>16)&255}))/255;
 auto blurred=peak::blur(mask,width,height,2+int(cfg.glowRadius*4));IDirect3DTexture9* halo=nullptr;
 if(SUCCEEDED(dev->CreateTexture(width,height,1,D3DUSAGE_DYNAMIC,D3DFMT_A8R8G8B8,D3DPOOL_DEFAULT,&halo,nullptr))){D3DLOCKED_RECT lock{};if(SUCCEEDED(halo->LockRect(0,&lock,nullptr,D3DLOCK_DISCARD))){for(int y=0;y<height;++y){auto row=reinterpret_cast<DWORD*>(static_cast<char*>(lock.pBits)+y*lock.Pitch);for(int x=0;x<width;++x)row[x]=(DWORD(std::clamp(blurred[y*width+x],0.f,1.f)*255)<<24)|0xffffff;}halo->UnlockRect(0);result.halo=halo;}else halo->Release();}}
 SelectObject(dc,old);DeleteObject(bmp);}
 SelectObject(dc,oldFont);DeleteObject(font);DeleteDC(dc);return result;
}
Image image(IDirect3DDevice9* dev,const std::string& text){auto it=cache.find(text);if(it!=cache.end())return it->second;auto im=raster(dev,text);if(im.texture)cache.emplace(text,im);else checked(E_FAIL,7,"Create glyph texture (GDI/CreateTexture/LockRect)");return im;}
struct Vertex{float x,y,z,rhw;DWORD color;float u,v;};
void draw(IDirect3DDevice9* dev,Image im,float x,float y,float scale,float alpha,DWORD tint=0xffffff,float rotation=0){if(!im.texture||alpha<=0)return;float r=x+im.width*scale,b=y+im.height*scale;DWORD color=(DWORD(std::clamp(alpha,0.f,1.f)*255)<<24)|(tint&0xffffff);Vertex v[]={{x-.5f,y-.5f,0,1,color,0,0},{r-.5f,y-.5f,0,1,color,1,0},{x-.5f,b-.5f,0,1,color,0,1},{r-.5f,b-.5f,0,1,color,1,1}};if(rotation!=0){float cx=(x+r)*.5f,cy=(y+b)*.5f;for(auto& point:v){float dx=point.x-cx,dy=point.y-cy;point.x=cx+dx*std::cos(rotation)-dy*std::sin(rotation);point.y=cy+dx*std::sin(rotation)+dy*std::cos(rotation);}}dev->SetTexture(0,im.texture);if(checked(dev->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP,2,v,sizeof(Vertex)),6,"DrawPrimitiveUP")&&!drewPixels){drewPixels=true;note("L4D2DangoMusic first textured draw submitted");}}
void styled(IDirect3DDevice9* dev,Image im,float x,float y,float scale,float alpha,DWORD color,float rotation=0){
 if(alpha<=.002f||!im.texture)return;
 if(cfg.glow&&cfg.glowStrength>0&&im.halo){auto glow=im;glow.texture=im.halo;draw(dev,glow,x,y,scale,alpha*std::min(1.f,cfg.glowStrength*2),cfg.accent,rotation);}
 if(cfg.outline){for(auto dx:{-1.f,1.f})for(auto dy:{-1.f,1.f})draw(dev,im,x+dx*scale,y+dy*scale,scale,alpha*.8f,0x101018,rotation);}
 draw(dev,im,x,y,scale,alpha,color,rotation);
}
struct PeakEntry {
 Line line;peak::Animation animation;peak::Layout layout;int index;double born,retired=-1,lastPlaced;Vec3 anchor;uint32_t epoch;
 PeakEntry(const Line& l,int64_t next,int i,double tick,Vec3 point,uint32_t e):line(l),animation(l,next),index(i),born(tick),lastPlaced(tick),anchor(point),epoch(e){}
};
std::vector<std::unique_ptr<PeakEntry>> peakEntries;std::string peakSong;int peakIndex=-1;int64_t lastSongPosition=-1,lastFrameTick=0;bool lastWorldMode=false;float lastWidth=0,lastHeight=0;
void peakFrame(IDirect3DDevice9* dev,const State& state,int at,int64_t song,bool world,const GameCameraFrame& camera,CameraPose pose,float width,float height){
 double clock=now()/1000.;
 bool jump=lastSongPosition>=0&&std::abs(song-lastSongPosition-(state.playing?int64_t((now()-lastFrameTick)*state.rate):0))>1800;
 if(peakSong!=state.key||jump||lastWorldMode!=world||lastWidth!=width||lastHeight!=height||(!peakEntries.empty()&&world&&peakEntries.back()->epoch!=camera.epoch)){peakEntries.clear();peakIndex=-1;peakSong=state.key;}
 lastSongPosition=song;lastFrameTick=now();lastWorldMode=world;lastWidth=width;lastHeight=height;
 if(at!=peakIndex){for(auto& e:peakEntries)if(e->retired<0)e->retired=clock;peakIndex=at;
 if(at>=0&&!state.timeline[at].text.empty()){auto& line=state.timeline[at];auto next=at+1<int(state.timeline.size())?state.timeline[at+1].start:line.start+5000;
 auto existing=std::find_if(peakEntries.begin(),peakEntries.end(),[&](auto& e){return e->index==at&&e->line.text==line.text;});
 if(existing!=peakEntries.end()){(*existing)->retired=-1;}
 else {
 peakEntries.erase(std::remove_if(peakEntries.begin(),peakEntries.end(),[&](auto& e){return e->line.text==line.text;}),peakEntries.end());
 Vec3 anchor=world?anchors.current:Vec3{};auto entry=std::make_unique<PeakEntry>(line,next,at,clock,anchor,camera.epoch);
 float maxPixels=world?std::min(cfg.worldDistance*.6f,130.f)/(cfg.worldHeight*cfg.worldDistance/200.f)*cfg.size:width*.32f/std::clamp(height/1080.f*28/cfg.size,.5f,2.6f);
 entry->layout=peak::layout(entry->animation.glyphs,[&](const std::string& text){return image(dev,text).advance;},maxPixels,cfg.size*1.35f);
 peakEntries.push_back(std::move(entry));while(peakEntries.size()>4)peakEntries.erase(peakEntries.begin());}}}
 for(size_t i=0;i<peakEntries.size();){auto& entry=*peakEntries[i];ExitFrame life;if(entry.retired>=0)life=exitFrame((clock-entry.retired)*1000,cfg.lingerMs,cfg.transition);
 if(life.done){peakEntries.erase(peakEntries.begin()+i);continue;}++i;
 Placement place;float scale=std::clamp(height/1080.f*28/cfg.size,.5f,2.6f),center=width*(entry.index%2?.78f:.22f),top=height*.61f;
 if(world){if(entry.retired<0&&needsReanchor(clock,entry.lastPlaced,cfg.reanchorMs)){entry.anchor=peak::position(pose,cfg.worldDistance,cfg.alternate?cfg.spread:0,unsigned(entry.index),6);entry.lastPlaced=clock;}
 auto displayAnchor=cfg.keepAway?peak::keepDistance(entry.anchor,pose,cfg.worldDistance):entry.anchor;if(!projectAnchor(camera,displayAnchor,width,height,cfg.worldHeight*cfg.worldDistance/200.f,cfg.size,place))continue;center=place.x;scale=place.scale;top=place.y-entry.layout.height*scale*.5f;}
 top-=life.lift*(world?20.f*scale*cfg.size/cfg.worldHeight:height*.025f);
 for(auto& glyph:entry.layout.glyphs){auto frame=entry.animation.sample(glyph.index,entry.animation.starts[glyph.index]+int64_t((song-entry.animation.starts[glyph.index])*cfg.animationSpeed),clock,cfg.pop,cfg.wiggle,cfg.wiggleFrequency,cfg.wiggleAmount);if(entry.retired>=0)frame.alpha=1;if(!cfg.pop&&!cfg.wiggle)frame={1,1,0,0,0};if(frame.alpha<=0)continue;
 auto im=image(dev,entry.animation.glyphs[glyph.index].text);float size=scale*frame.scale;float x=center+(glyph.x+glyph.width*.5f+frame.x)*scale-im.width*size*.5f;float y=top+(glyph.y+cfg.size*.5f+frame.y)*scale-im.height*size*.5f;
 styled(dev,im,x,y,size,life.alpha*frame.alpha*cfg.opacity,cfg.textColor,frame.rotation);}
 if(cfg.translation&&entry.retired<0&&!entry.line.translation.empty()){auto tr=image(dev,entry.line.translation);float ts=std::min(scale*.72f,width*.32f/std::max(tr.width,1));styled(dev,tr,center-tr.width*ts*.5f,top+entry.layout.height*scale+8,ts,life.alpha,cfg.textColor);}
 }
}
void drawSongInfo(IDirect3DDevice9* dev,const State& state,const GameCameraFrame& camera,bool valid,const CameraPose& pose,float width,float height){
 static std::string song;static Vec3 anchor;static uint32_t epoch=0;static int64_t placed=0;
 if(!cfg.showTitle&&!cfg.showArtist){song.clear();return;}if(state.key.empty())return;
 if(song!=state.key||epoch!=camera.epoch||!placed||(cfg.reanchorMs>0&&now()-placed>=cfg.reanchorMs)){song=state.key;epoch=camera.epoch;anchor=anchorAt(pose,cfg.worldDistance*1.8f,0,cfg.worldDistance*.28f);placed=now();}
 auto limited=[](const std::string& text){std::string out;auto chars=graphemes(text);for(size_t i=0;i<std::min<size_t>(80,chars.size());++i)out+=chars[i].text;return out;};
 float center=width*.5f,y=height*.16f,scale=.65f;Placement point;
 if(valid){if(!projectAnchor(camera,anchor,width,height,cfg.worldHeight*1.5f,cfg.size,point))return;center=point.x;y=point.y;scale=point.scale;}else{song.clear();placed=0;}
 if(cfg.showTitle&&!state.title.empty()){auto im=image(dev,limited(state.title));float s=std::min(scale,width*.7f/std::max(im.width,1));styled(dev,im,center-im.width*s*.5f,y,s,.85f,cfg.textColor);y+=cfg.size*s*1.5f;}
 if(cfg.showArtist&&!state.artist.empty()){auto im=image(dev,limited(state.artist));float s=std::min(scale*.75f,width*.7f/std::max(im.width,1));styled(dev,im,center-im.width*s*.5f,y,s,.7f,cfg.accent);}
}
struct DrawGuard {
 IDirect3DDevice9* dev; IDirect3DStateBlock9* block=nullptr;
 IDirect3DSurface9 *target=nullptr,*depth=nullptr,*back=nullptr;
 IDirect3DSurface9* extra[3]={nullptr,nullptr,nullptr};unsigned extraCount=0;bool captured=false;bool scene=false;
 explicit DrawGuard(IDirect3DDevice9* d):dev(d){}
 ~DrawGuard(){if(scene)dev->EndScene();if(block){if(captured){dev->SetDepthStencilSurface(nullptr);if(target)dev->SetRenderTarget(0,target);for(unsigned i=0;i<extraCount;++i)dev->SetRenderTarget(i+1,extra[i]);dev->SetDepthStencilSurface(depth);}block->Apply();block->Release();}for(auto* surface:extra)if(surface)surface->Release();if(back)back->Release();if(target)target->Release();if(depth)depth->Release();}
};
void WINAPI render(IDirect3DDevice9* dev,const PresentInfo* present){
 if(!dev||!present)return;
 auto currentSettings=std::atomic_load(&settings);cfg=*currentSettings;if(!cfg.enabled)return;
 if(renderedSettings!=currentSettings){clear();peakEntries.clear();peakIndex=-1;renderedSettings=currentSettings;}
 struct OverlayScope {OverlayScope(){overlayDrawing=true;}~OverlayScope(){overlayDrawing=false;}} overlayScope;
 captureCamera(dev,present);
 const unsigned path=present->is_swapchain_present?1:0;
 if(!presentLogged[path]){presentLogged[path]=true;note(std::string("L4D2DangoMusic first Present path=")+(path?"SwapChain":"Device")+(present->swap_chain?" current-chain=yes":" current-chain=no"));}
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
 if(checked(backResult,2,"Get current SwapChain backbuffer")){D3DSURFACE_DESC desc{};IDirect3DSwapChain9* ownedChain=nullptr;IDirect3DSwapChain9* targetChain=present->swap_chain;if(!targetChain&&SUCCEEDED(dev->GetSwapChain(0,&ownedChain)))targetChain=ownedChain;IUnknown* targetIdentity=nullptr;if(targetChain)targetChain->QueryInterface(__uuidof(IUnknown),reinterpret_cast<void**>(&targetIdentity));if(!targetIdentity)back->QueryInterface(__uuidof(IUnknown),reinterpret_cast<void**>(&targetIdentity));uintptr_t targetKey=reinterpret_cast<uintptr_t>(targetIdentity?targetIdentity:static_cast<IUnknown*>(back));bool drawThisScene=frameGate.allow(targetKey,sceneSerial.load(std::memory_order_acquire));if(targetIdentity)targetIdentity->Release();if(ownedChain)ownedChain->Release();if(!drawThisScene)return;if(!checked(back->GetDesc(&desc),9,"Get backbuffer description"))return;dev->SetDepthStencilSurface(nullptr);for(unsigned i=0;i<guard.extraCount;++i)dev->SetRenderTarget(i+1,nullptr);if(!checked(dev->SetRenderTarget(0,back),3,"SetRenderTarget"))return;D3DVIEWPORT9 vp{0,0,desc.Width,desc.Height,0,1};if(!checked(dev->SetViewport(&vp),10,"SetViewport"))return;
 if(checked(dev->BeginScene(),4,"BeginScene")){guard.scene=true;
 dev->SetVertexShader(nullptr);dev->SetPixelShader(nullptr);dev->SetFVF(D3DFVF_XYZRHW|D3DFVF_DIFFUSE|D3DFVF_TEX1);
 dev->SetRenderState(D3DRS_FILLMODE,D3DFILL_SOLID);dev->SetRenderState(D3DRS_ZENABLE,FALSE);dev->SetRenderState(D3DRS_ZWRITEENABLE,FALSE);dev->SetRenderState(D3DRS_CULLMODE,D3DCULL_NONE);dev->SetRenderState(D3DRS_LIGHTING,FALSE);dev->SetRenderState(D3DRS_FOGENABLE,FALSE);dev->SetRenderState(D3DRS_STENCILENABLE,FALSE);dev->SetRenderState(D3DRS_SCISSORTESTENABLE,FALSE);dev->SetRenderState(D3DRS_ALPHATESTENABLE,FALSE);dev->SetRenderState(D3DRS_ALPHABLENDENABLE,TRUE);dev->SetRenderState(D3DRS_SRCBLEND,D3DBLEND_SRCALPHA);dev->SetRenderState(D3DRS_DESTBLEND,D3DBLEND_INVSRCALPHA);dev->SetRenderState(D3DRS_BLENDOP,D3DBLENDOP_ADD);dev->SetRenderState(D3DRS_SEPARATEALPHABLENDENABLE,FALSE);dev->SetRenderState(D3DRS_COLORWRITEENABLE,15);dev->SetRenderState(D3DRS_SRGBWRITEENABLE,FALSE);
 for(auto stage:{D3DTSS_COLOROP,D3DTSS_ALPHAOP})dev->SetTextureStageState(0,stage,D3DTOP_MODULATE);
 dev->SetTextureStageState(0,D3DTSS_COLORARG1,D3DTA_TEXTURE);dev->SetTextureStageState(0,D3DTSS_COLORARG2,D3DTA_DIFFUSE);dev->SetTextureStageState(0,D3DTSS_ALPHAARG1,D3DTA_TEXTURE);dev->SetTextureStageState(0,D3DTSS_ALPHAARG2,D3DTA_DIFFUSE);dev->SetTextureStageState(0,D3DTSS_TEXCOORDINDEX,0);dev->SetTextureStageState(0,D3DTSS_TEXTURETRANSFORMFLAGS,D3DTTFF_DISABLE);dev->SetTextureStageState(1,D3DTSS_COLOROP,D3DTOP_DISABLE);dev->SetTextureStageState(1,D3DTSS_ALPHAOP,D3DTOP_DISABLE);
 dev->SetSamplerState(0,D3DSAMP_MINFILTER,D3DTEXF_LINEAR);dev->SetSamplerState(0,D3DSAMP_MAGFILTER,D3DTEXF_LINEAR);dev->SetSamplerState(0,D3DSAMP_ADDRESSU,D3DTADDRESS_CLAMP);dev->SetSamplerState(0,D3DSAMP_ADDRESSV,D3DTADDRESS_CLAMP);dev->SetSamplerState(0,D3DSAMP_SRGBTEXTURE,FALSE);
 if(cfg.demo){auto banner=image(dev,"L4D2DangoMusic 0.6.0 DEMO - "+std::string(path?"SwapChain":"Device"));draw(dev,banner,16,16,std::min(1.f,desc.Width*.9f/std::max(banner.width,1)),1.f);}
 auto camera=std::atomic_load(&source_camera::snapshot);const auto& gameFrame=*camera;CameraPose pose;
 bool cameraGood=usableCamera(gameFrame,now())&&cameraPose(gameFrame,pose);bool cameraValid=cfg.worldLyrics&&cameraGood;
 if(cfg.worldLyrics&&cfg.debugOverlay){auto status=image(dev,cameraValid?std::string("GAME CAMERA: ACTIVE (experimental)"):std::string("GAME CAMERA: ")+source_camera::status()+" - screen fallback");draw(dev,status,16,cfg.demo?64.f:16.f,std::min(.65f,desc.Width*.9f/std::max(status.width,1)),.8f,cameraValid?0x99ffbb:0xffcc88);}
 if(!cameraValid)anchors.reset();else anchors.update(gameFrame,pose,state->key,at);
 if(cameraValid&&cfg.worldTestAnchor){Placement test;if(projectAnchor(gameFrame,anchors.test,float(desc.Width),float(desc.Height),cfg.worldHeight,cfg.size,test)){auto im=image(dev,"WORLD ANCHOR +");styled(dev,im,test.x-im.width*test.scale*.5f,test.y,test.scale,.9f,0x99ffbb);}}
 drawSongInfo(dev,*state,gameFrame,cameraGood,pose,float(desc.Width),float(desc.Height));
 peakFrame(dev,*state,at,position<0?0:lyricPosition(position,cfg.demo?0:cfg.offset),cameraValid,gameFrame,pose,float(desc.Width),float(desc.Height));
 if(at<0&&cfg.debugOverlay){auto im=image(dev,text);styled(dev,im,16,96,.65f,.7f,cfg.textColor);}
 dev->EndScene();guard.scene=false;}
 }

 }catch(...){if(logger)logger(LDB_HOST_LOG_ERROR,"Lyric render failed");}
}
bool load(const std::filesystem::path& pluginDirectory){try{directory=pluginDirectory/L"L4D2DangoMusic";auto path=directory/L"config.json";
 if(std::filesystem::exists(path)){std::ifstream f(path);cfg=decodeConfig(nlohmann::json::parse(f));}
 std::atomic_store(&settings,std::make_shared<const Config>(cfg));

 note("L4D2DangoMusic 0.6.0 config="+path.u8string()+" mode="+(cfg.demo?"DEMO":"BLF/LX")+" port="+std::to_string(cfg.port)+" offsetMs="+std::to_string(cfg.offset)+" displayMode="+cfg.mode);
 stopping=false;worker=std::thread(run);return TRUE;}catch(const std::exception& e){if(logger)logger(LDB_HOST_LOG_ERROR,e.what());return FALSE;}catch(...){return FALSE;}}
void WINAPI created(IDirect3DDevice9*){++deviceGeneration;frameGate.reset();anchors.reset();peakEntries.clear();peakIndex=-1;clear();reportedErrors=0;drewPixels=false;note("L4D2DangoMusic device lifecycle: created/destroyed; textures cleared");}
void WINAPI beforeReset(IDirect3DDevice9*,const D3DPRESENT_PARAMETERS*,BOOL){frameGate.reset();anchors.reset();peakEntries.clear();peakIndex=-1;clear();}
void WINAPI afterReset(IDirect3DDevice9*,HRESULT hr,BOOL){++deviceGeneration;reportedErrors=0;checked(hr,5,"Reset");note("L4D2DangoMusic Reset completed; textures will rebuild");}
void WINAPI shutdown(){stopping=true;wake.notify_all();if(worker.joinable())worker.join();clear();}
}
namespace {
struct Patch {void** slot;void* original;void* replacement;};
std::array<Patch,64> patches{};std::atomic<size_t> patchCount{0};SRWLOCK patchLock=SRWLOCK_INIT;std::filesystem::path pluginDirectory;
void fileLog(int,const char* message){std::ofstream file(pluginDirectory/L"L4D2DangoMusic.log",std::ios::app);file<<"L4D2DangoMusic 0.6.0 "<<message<<'\n';}
bool patch(void** table,size_t slot,void* replacement){
 if(!TryAcquireSRWLockExclusive(&patchLock))return false;auto count=patchCount.load(std::memory_order_acquire);
 for(size_t i=0;i<count;++i)if(patches[i].slot==table+slot){bool same=patches[i].replacement==replacement;ReleaseSRWLockExclusive(&patchLock);return same;}
 if(count>=patches.size()){ReleaseSRWLockExclusive(&patchLock);return false;}
 DWORD old=0;if(!VirtualProtect(table+slot,sizeof(void*),PAGE_READWRITE,&old)){ReleaseSRWLockExclusive(&patchLock);return false;}
 void* before=table[slot];patches[count]={table+slot,before,replacement};patchCount.store(count+1,std::memory_order_release);
 void* actual=InterlockedCompareExchangePointer(table+slot,replacement,before);DWORD ignored=0;VirtualProtect(table+slot,sizeof(void*),old,&ignored);
 if(actual!=before)patchCount.store(count,std::memory_order_release);ReleaseSRWLockExclusive(&patchLock);return actual==before;
}
void* original(void* object,size_t slot){auto table=*reinterpret_cast<void***>(object);auto count=patchCount.load(std::memory_order_acquire);for(size_t i=0;i<count;++i)if(patches[i].slot==table+slot)return patches[i].original;return nullptr;}
thread_local unsigned presentDepth=0;
HRESULT WINAPI devicePresent(IDirect3DDevice9* device,const RECT* source,const RECT* target,HWND window,const RGNDATA* dirty){
 auto fn=reinterpret_cast<HRESULT(WINAPI*)(IDirect3DDevice9*,const RECT*,const RECT*,HWND,const RGNDATA*)>(original(device,17));
 ++presentDepth;if(presentDepth==1){PresentInfo info;render(device,&info);}auto hr=fn(device,source,target,window,dirty);--presentDepth;return hr;
}
HRESULT WINAPI chainPresent(IDirect3DSwapChain9* chain,const RECT* source,const RECT* target,HWND window,const RGNDATA* dirty,DWORD flags){
 auto fn=reinterpret_cast<HRESULT(WINAPI*)(IDirect3DSwapChain9*,const RECT*,const RECT*,HWND,const RGNDATA*,DWORD)>(original(chain,3));
 ++presentDepth;if(presentDepth==1){IDirect3DDevice9* device=nullptr;if(SUCCEEDED(chain->GetDevice(&device))&&device){PresentInfo info{true,chain};render(device,&info);device->Release();}}
 auto hr=fn(chain,source,target,window,dirty,flags);--presentDepth;return hr;
}
void hookChain(IDirect3DSwapChain9* chain){if(chain&&(*reinterpret_cast<void***>(chain))[3]!=reinterpret_cast<void*>(&chainPresent)&&patch(*reinterpret_cast<void***>(chain),3,reinterpret_cast<void*>(&chainPresent)))note("SwapChain Present attached");}
HRESULT WINAPI getChain(IDirect3DDevice9* device,UINT index,IDirect3DSwapChain9** chain){auto fn=reinterpret_cast<HRESULT(WINAPI*)(IDirect3DDevice9*,UINT,IDirect3DSwapChain9**)>(original(device,14));auto hr=fn(device,index,chain);if(SUCCEEDED(hr)&&chain&&*chain)hookChain(*chain);return hr;}
HRESULT WINAPI addChain(IDirect3DDevice9* device,D3DPRESENT_PARAMETERS* params,IDirect3DSwapChain9** chain){auto fn=reinterpret_cast<HRESULT(WINAPI*)(IDirect3DDevice9*,D3DPRESENT_PARAMETERS*,IDirect3DSwapChain9**)>(original(device,13));auto hr=fn(device,params,chain);if(SUCCEEDED(hr)&&chain&&*chain)hookChain(*chain);return hr;}
HRESULT WINAPI resetDevice(IDirect3DDevice9* device,D3DPRESENT_PARAMETERS* params){auto fn=reinterpret_cast<HRESULT(WINAPI*)(IDirect3DDevice9*,D3DPRESENT_PARAMETERS*)>(original(device,16));beforeReset(device,params,FALSE);auto hr=fn(device,params);afterReset(device,hr,FALSE);if(SUCCEEDED(hr)){IDirect3DSwapChain9* chain=nullptr;if(SUCCEEDED(device->GetSwapChain(0,&chain))){hookChain(chain);chain->Release();}}return hr;}
HRESULT WINAPI presentEx(IDirect3DDevice9Ex* device,const RECT* source,const RECT* target,HWND window,const RGNDATA* dirty,DWORD flags){auto fn=reinterpret_cast<HRESULT(WINAPI*)(IDirect3DDevice9Ex*,const RECT*,const RECT*,HWND,const RGNDATA*,DWORD)>(original(device,121));++presentDepth;if(presentDepth==1){PresentInfo info;render(device,&info);}auto hr=fn(device,source,target,window,dirty,flags);--presentDepth;return hr;}
HRESULT WINAPI resetEx(IDirect3DDevice9Ex* device,D3DPRESENT_PARAMETERS* params,D3DDISPLAYMODEEX* mode){auto fn=reinterpret_cast<HRESULT(WINAPI*)(IDirect3DDevice9Ex*,D3DPRESENT_PARAMETERS*,D3DDISPLAYMODEEX*)>(original(device,132));beforeReset(device,params,TRUE);auto hr=fn(device,params,mode);afterReset(device,hr,TRUE);return hr;}
HRESULT WINAPI endScene(IDirect3DDevice9* device){auto fn=reinterpret_cast<HRESULT(WINAPI*)(IDirect3DDevice9*)>(original(device,42));auto hr=fn(device);if(SUCCEEDED(hr)&&!overlayDrawing)sceneSerial.fetch_add(1,std::memory_order_release);return hr;}
HRESULT WINAPI clearScene(IDirect3DDevice9* device,DWORD count,const D3DRECT* rects,DWORD flags,D3DCOLOR color,float z,DWORD stencil){auto fn=reinterpret_cast<HRESULT(WINAPI*)(IDirect3DDevice9*,DWORD,const D3DRECT*,DWORD,D3DCOLOR,float,DWORD)>(original(device,43));auto hr=fn(device,count,rects,flags,color,z,stencil);if(SUCCEEDED(hr)&&!overlayDrawing&&(flags&D3DCLEAR_TARGET))sceneSerial.fetch_add(1,std::memory_order_release);return hr;}
std::string menuText;
const char* menuAction(void*);
std::string escaped(std::string s){std::string out;for(char c:s){if(c=='"'||c=='\\')out+='\\';if(c!='\r'&&c!='\n')out+=c;}return out;}
std::string item(const std::string& label,uintptr_t id){char callback[32],data[32];std::snprintf(callback,sizeof(callback),"0x%lx",static_cast<unsigned long>(reinterpret_cast<uintptr_t>(&menuAction)));std::snprintf(data,sizeof(data),"0x%lx",static_cast<unsigned long>(id));return "\""+escaped(label)+"\" { \"callback\" \""+callback+"\" \"user_data\" \""+data+"\" }\n";}
const char* page(const std::string& title,const std::string& rows){menuText="\""+escaped(title)+"\" {\n"+rows+"}\n";return menuText.c_str();}
void changeSettings(const Config& next){std::atomic_store(&settings,std::make_shared<const Config>(next));}
std::string on(bool v){return v?u8"开":u8"关";}
const char* channelMenu(const Config& c,bool accent,unsigned channel){uint32_t color=accent?c.accent:c.textColor;unsigned value=(color>>((2-channel)*8))&255;std::string rows;
 const char* names[]={"R","G","B"};const char* labels[]={"-16","-1","+1","+16","0","64","128","192","255"};
 for(unsigned i=0;i<9;++i)rows+=item(labels[i],10000+(accent?1000:0)+channel*100+i);
 return page(std::string(accent?u8"光晕 ":u8"正文 ")+names[channel]+" = "+std::to_string(value)+" "+colorText(color),rows);}
const char* menuAction(void* argument){uintptr_t id=reinterpret_cast<uintptr_t>(argument);std::lock_guard<std::mutex> lock(settingsMutex);auto c=*std::atomic_load(&settings);
 if(id==20)return page(u8"显示与歌曲信息",item(u8"翻译："+on(c.translation),2)+item(u8"世界歌词："+on(c.worldLyrics),7)+item(u8"靠近时后退："+on(c.keepAway),9)+item(u8"远处歌名："+on(c.showTitle),5)+item(u8"远处作者："+on(c.showArtist),6)+item(u8"长句回到视野",19));
 if(id==21)return page(u8"颜色与柔光",item(u8"正文颜色 "+colorText(c.textColor),11)+item(u8"光晕颜色 "+colorText(c.accent),12)+item(u8"柔光："+on(c.glow),8));
 if(id==22)return page(u8"动画与停留",item(u8"显字动画速度",13)+item(u8"退场耗时",14)+item(u8"换句后额外停留",15)+item(u8"抖动："+on(c.wiggle),3)+item(u8"逐字弹出："+on(c.pop),4));
 if(id==19)return page(u8"长句回到当前视野",item(u8"关闭",580)+item(u8"每 3 秒",581)+item(u8"每 5 秒",582)+item(u8"每 10 秒",583));
 if(id==10){std::string rows;for(int i=0;i<6;++i)rows+=item(presetName(i),100+i);return page(u8"歌词预设",rows);}
 if(id==11||id==12){bool accent=id==12;std::string rows;const char* names[]={u8"暖白",u8"纯白",u8"樱花粉",u8"冰蓝",u8"金色",u8"薄荷绿",u8"淡紫",u8"橘红"};for(int i=0;i<8;++i)rows+=item(names[i],(accent?310:300)+i);for(int i=0;i<3;++i)rows+=item(std::string(u8"自定义 ")+"RGB"[i],(accent?410:400)+i);return page(std::string(accent?u8"光晕颜色 ":u8"正文颜色 ")+colorText(accent?c.accent:c.textColor),rows);}
 if(id>=400&&id<=402)return channelMenu(c,false,unsigned(id-400));if(id>=410&&id<=412)return channelMenu(c,true,unsigned(id-410));
 if(id==13)return page(u8"显字动画速度（不改变歌曲进度）",item("0.5x",500)+item("0.75x",501)+item("1x",502)+item("1.5x",503)+item("2x",504));
 if(id==14)return page(u8"退场耗时",item("200 ms",520)+item("400 ms",521)+item("700 ms",522)+item("1000 ms",523)+item("1500 ms",524));
 if(id==15)return page(u8"换句后额外停留",item("0 ms",540)+item("500 ms",541)+item("1000 ms",542)+item("1500 ms",543)+item("2000 ms",544));
 if(id==16)return page(u8"同步偏移（正值提前）",item("-100 ms",560)+item("-50 ms",561)+item("+50 ms",562)+item("+100 ms",563)+item(u8"重设为 500ms",564));
 if(id==18){std::atomic_store(&pendingSave,std::make_shared<const Config>(c));saveState=1;return page(u8"保存设置",item(u8"已提交后台保存，返回主菜单查看结果",0));}
 if(id>=100&&id<=105)c=applyPreset(c,int(id-100));
 else if(id>=300&&id<=317&&(id<=307||id>=310)){uint32_t colors[]={0xf7e1d7,0xffffff,0xff99cc,0x70d6ff,0xffcc66,0x99ffcc,0xc5a3ff,0xff8855};if(id>=310)c.accent=colors[id-310];else c.textColor=colors[id-300];c.presetId=-1;}
 else if(id>=10000&&id<11209){bool accent=id>=11000;unsigned encoded=unsigned(id-(accent?11000:10000)),channel=encoded/100,choice=encoded%100;if(channel>2||choice>8)return nullptr;auto& color=accent?c.accent:c.textColor;unsigned shift=(2-channel)*8;int value=int((color>>shift)&255);const int deltas[]={-16,-1,1,16};const int fixed[]={0,64,128,192,255};value=choice<4?std::clamp(value+deltas[choice],0,255):fixed[choice-4];color=(color&~(255u<<shift))|(uint32_t(value)<<shift);c.presetId=-1;changeSettings(c);return channelMenu(c,accent,channel);}
 else if(id>=500&&id<=504){float values[]={.5f,.75f,1,1.5f,2};c.animationSpeed=values[id-500];c.presetId=-1;}
 else if(id>=520&&id<=524){int values[]={200,400,700,1000,1500};c.transition=values[id-520];c.presetId=-1;}
 else if(id>=540&&id<=544){int values[]={0,500,1000,1500,2000};c.lingerMs=values[id-540];}
 else if(id>=580&&id<=583){int values[]={0,3000,5000,10000};c.reanchorMs=values[id-580];}
 else if(id>=560&&id<=564){int values[]={-100,-50,50,100};c.offset=id==564?500:std::clamp(c.offset+values[id-560],-10000,10000);}
 else switch(id){case 1:c.enabled=!c.enabled;break;case 2:c.translation=!c.translation;break;case 3:c.wiggle=!c.wiggle;c.presetId=-1;break;case 4:c.pop=!c.pop;c.presetId=-1;break;case 5:c.showTitle=!c.showTitle;break;case 6:c.showArtist=!c.showArtist;break;case 7:c.worldLyrics=!c.worldLyrics;break;case 8:c.glow=!c.glow;c.presetId=-1;break;case 9:c.keepAway=!c.keepAway;break;default:return nullptr;}
 changeSettings(c);return page(u8"设置已应用",item(u8"实时生效；如需下次保留，请返回主菜单保存",0));
}
const char* rootMenu(bool title){if(title)return "L4D2DangoMusic";auto c=*std::atomic_load(&settings);std::string rows;
 rows+=item(u8"歌词显示："+on(c.enabled),1)+item(u8"显示与歌曲信息",20)+item(std::string(u8"预设：")+presetName(c.presetId),10)+item(u8"颜色与柔光",21)+item(u8"动画、速度与停留",22)+item(u8"同步偏移 "+std::to_string(c.offset)+"ms",16);
 int saved=saveState.load();rows+=item(saved==1?u8"正在保存…":saved==2?u8"保存设置（上次成功）":saved<0?u8"保存设置（上次失败，见日志）":u8"保存设置",18);return page("L4D2DangoMusic",rows);}
class LyricsPlugin final:public IL4NPlugin {
 bool started=false;
public:
 const char* GetName()override{return "L4D2DangoMusic";}
 const char* GetVersion()override{return "0.6.0";}
 const char* RequestHudMenu(bool title)override{start();return rootMenu(title);}
 void start(){if(started)return;HMODULE self=nullptr;GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,reinterpret_cast<const wchar_t*>(&fileLog),&self);wchar_t path[32768]{};if(!GetModuleFileNameW(self,path,32768))return;pluginDirectory=std::filesystem::path(path).parent_path();logger=fileLog;started=load(pluginDirectory);source_camera::logger=note;}
 void OnModuleLoaded(const char* name,std::uintptr_t handle)override{start();if(!started||!name)return;std::string n(name);if(n=="client"||n=="client.dll")source_camera::clientModule=reinterpret_cast<HMODULE>(handle);if(n=="engine"||n=="engine.dll")source_camera::engineModule=reinterpret_cast<HMODULE>(handle);source_camera::connect();}
 void OnGameLaunch()override{start();if(!started)return;if(!source_camera::clientModule)source_camera::clientModule=GetModuleHandleW(L"client.dll");if(!source_camera::engineModule)source_camera::engineModule=GetModuleHandleW(L"engine.dll");source_camera::connect();}
 void OnD3DDeviceCreated(void* pointer,bool)override{start();if(!started||!pointer)return;auto device=static_cast<IDirect3DDevice9*>(pointer);auto table=*reinterpret_cast<void***>(device);
 cfg=*std::atomic_load(&settings);created(device);patch(table,42,reinterpret_cast<void*>(&endScene));patch(table,43,reinterpret_cast<void*>(&clearScene));patch(table,17,reinterpret_cast<void*>(&devicePresent));patch(table,16,reinterpret_cast<void*>(&resetDevice));patch(table,14,reinterpret_cast<void*>(&getChain));patch(table,13,reinterpret_cast<void*>(&addChain));IDirect3DSwapChain9* chain=nullptr;if(SUCCEEDED(device->GetSwapChain(0,&chain))&&chain){hookChain(chain);chain->Release();}IDirect3DDevice9Ex* ex=nullptr;if(SUCCEEDED(device->QueryInterface(__uuidof(IDirect3DDevice9Ex),reinterpret_cast<void**>(&ex)))&&ex){auto et=*reinterpret_cast<void***>(ex);patch(et,121,reinterpret_cast<void*>(&presentEx));patch(et,132,reinterpret_cast<void*>(&resetEx));ex->Release();}note("x86 Client D3D9 callbacks installed; no Host plugin required");}
 void stop(){source_camera::stop();if(started){shutdown();started=false;}for(size_t i=patchCount.load();i>0;--i){auto& p=patches[i-1];if(!source_camera::readable(p.slot,sizeof(void*)))continue;DWORD old=0;if(VirtualProtect(p.slot,sizeof(void*),PAGE_READWRITE,&old)){InterlockedCompareExchangePointer(p.slot,p.original,p.replacement);DWORD ignored=0;VirtualProtect(p.slot,sizeof(void*),old,&ignored);}}patchCount.store(0);}
 ~LyricsPlugin()override{stop();}
};
}
LyricsPlugin& instance(){static LyricsPlugin plugin;return plugin;}
#if defined(__GNUC__) && !defined(_MSC_VER)
// L4N is an MSVC C++ interface. GNU's two destructor vtable entries are NOT
// compatible. Export an explicit MSVC x86 table; delegate into the native object.
namespace {
void* __fastcall abiDestructor(void* self,void*,unsigned){instance().stop();return self;}
unsigned __fastcall abiVersion(void*,void*){return 2;}
const char* __fastcall abiName(void*,void*){return instance().GetName();}
const char* __fastcall abiRelease(void*,void*){return instance().GetVersion();}
void __fastcall abiModule(void*,void*,const char* name,uintptr_t handle){instance().OnModuleLoaded(name,handle);}
void __fastcall abiLaunch(void*,void*){instance().OnGameLaunch();}
void __fastcall abiD3D(void*,void*,void*){}
void __fastcall abiDevice(void*,void*,void* device,bool dxvk){instance().OnD3DDeviceCreated(device,dxvk);}
const char* __fastcall abiMenu(void*,void*,bool title){return instance().RequestHudMenu(title);}
void* msvcVtable[]={reinterpret_cast<void*>(&abiDestructor),reinterpret_cast<void*>(&abiVersion),reinterpret_cast<void*>(&abiName),reinterpret_cast<void*>(&abiRelease),reinterpret_cast<void*>(&abiModule),reinterpret_cast<void*>(&abiLaunch),reinterpret_cast<void*>(&abiD3D),reinterpret_cast<void*>(&abiDevice),reinterpret_cast<void*>(&abiMenu)};
struct MsvcObject{void** table;};MsvcObject msvcObject{msvcVtable};
}
extern "C" __declspec(dllexport) IL4NPlugin* GetL4NPluginInstance(){return reinterpret_cast<IL4NPlugin*>(&msvcObject);}
#else
extern "C" __declspec(dllexport) IL4NPlugin* GetL4NPluginInstance(){return &instance();}
#endif
extern "C" __declspec(dllexport) void L4D2DangoMusicShutdown(){instance().stop();}
