#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <winhttp.h>
#include <tlhelp32.h>
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
#include "lyric_lifecycle.hpp"
#include "player_session.hpp"
#include "camera_diagnostics.hpp"
#include "source_camera.hpp"
#include "source_walls.hpp"
#include "source_entities.hpp"
#include "hud_menu_policy.hpp"
#include "native_hud.hpp"
#include "menu_pages.hpp"
static_assert(sizeof(void*)==4,"Pure L4N plugin must be x86");
struct PresentInfo {bool is_swapchain_present=false;IDirect3DSwapChain9* swap_chain=nullptr;};
namespace {
using namespace dango;
void captureSourceFeatures(const GameCameraFrame& frame){source_walls::onFrame(frame);source_entities::onFrame(frame);}
hud::NativeHud nativeHud;
std::string buildNativeMenu(uintptr_t);
void activateNativeMenu(uintptr_t);
void closeNativeMenu();
void updateHudAppearance(const hud::Appearance&);
std::string nativeMenuNotice();
const char* nativeEntryMenu();
int64_t now(){return static_cast<int64_t>(GetTickCount64());}
thread_local Config cfg;
struct MenuNotice {uint64_t tick=0;std::string text;};
std::shared_ptr<const MenuNotice> menuNotice=std::make_shared<MenuNotice>();
std::shared_ptr<const Config> settings=std::make_shared<Config>(),pendingSave,savedSettings=std::make_shared<Config>();
std::mutex settingsMutex;std::atomic<int> saveState{0};thread_local std::shared_ptr<const Config> renderedSettings;
HMODULE ownModule=nullptr;std::filesystem::path ownPath;
std::filesystem::path directory;using LogFn=void(*)(int,const char*);LogFn logger=nullptr;
constexpr int LDB_HOST_LOG_INFO=0,LDB_HOST_LOG_WARNING=1,LDB_HOST_LOG_ERROR=2;
std::atomic<bool> positionPreview{false},positionPreviewMotion{false};bool lastPositionPreview=false,lastWallMode=false,escapeHeld=false;uint64_t layoutGeneration=0;
PlaybackDisplay playbackDisplay;
PlaybackCursor playbackCursor;LyricLifecycle lyricLifecycle;std::atomic<int> visibleLyricIndex{-1};std::atomic<uint64_t> presentationCount{0},restoreErrors{0};
std::atomic<bool> stopping{false};std::thread worker;std::mutex sleepMutex;std::condition_variable wake;
std::shared_ptr<const State> snapshot=std::make_shared<State>();
bool presentLogged[2]={false,false};uint64_t reportedErrors=0;bool drewPixels=false;
void note(const std::string& message){if(logger)logger(LDB_HOST_LOG_INFO,message.c_str());}
struct WorldAnchors {
 std::string key;uint32_t epoch=0;bool ready=false;int currentIndex=-1,previousIndex=-1;Vec3 current{},previous{},test{},lastEye{};LyricAnchorSlots slots;
 void reset(){slots.reset();ready=false;currentIndex=previousIndex=-1;key.clear();}
 void update(const GameCameraFrame& frame,const CameraPose& pose,const std::string& song,int at){
 if(!ready||epoch!=frame.epoch||key!=song||length(pose.eye-lastEye)>1024){reset();ready=true;epoch=frame.epoch;key=song;test=anchorAt(pose,cfg.worldDistance,0,18);}
 lastEye=pose.eye;
 if(at>=0&&currentIndex!=at){previous=current;previousIndex=currentIndex;int slot=at%cfg.layout.lyricCount;Vec3 fallback{};if(componentAnchor(frame,pose,cfg.layout.lyrics[size_t(slot)],fallback))current=slots.select(slot,cfg.reuseVisibleAnchor,frame,fallback,pose.eye,cfg.layout.lyrics[size_t(slot)].distance,cfg.reuseDistanceFactor);currentIndex=at;}
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
 CameraSample s;while(cameraQueue.pop(s)){if(disabled)continue;auto json=cameraJson(s);json["dropped_total"]=cameraQueue.dropped.load();json["lyric_index"]=visibleLyricIndex.load();json["presentations"]=presentationCount.load();json["restore_errors"]=restoreErrors.load();auto music=std::atomic_load(&snapshot);json["player"]={{"connected",music->connected},{"playing",music->playing},{"generation",music->generation},{"position_ms",music->position},{"sampled_ms",music->sampled},{"timeline_lines",music->timeline.size()},{"message",music->message}};json["position_preview"]=positionPreview.load();auto camera=std::atomic_load(&source_camera::snapshot);json["source_camera"]={{"capture_state",source_camera::captureState.load()},{"captured_frames",source_camera::capturedFrames.load()},{"active",camera->active},{"tick_ms",camera->tick},{"epoch",camera->epoch},{"world_to_view",camera->worldToView},{"world_to_clip",camera->worldToClip},{"width",camera->width},{"height",camera->height}};auto line=json.dump()+"\n";if(bytes+line.size()>2*1024*1024||records>=2400){disabled=true;note("L4D2DangoMusic camera diagnostic reached limit; restart to capture again");continue;}file<<line;if(!file){disabled=true;note("L4D2DangoMusic camera diagnostic write failed");continue;}bytes+=line.size();++records;}if(file)file.flush();
 }
};
bool checked(HRESULT hr,unsigned stage,const char* operation){if(SUCCEEDED(hr))return true;auto bit=uint64_t(1)<<stage;if(!(reportedErrors&bit)){reportedErrors|=bit;char msg[256];std::snprintf(msg,sizeof(msg),"L4D2DangoMusic render-error operation=%s hr=0x%08lx",operation,static_cast<unsigned long>(hr));if(logger)logger(LDB_HOST_LOG_WARNING,msg);}return false;}
void publish(State s){auto old=std::atomic_load(&snapshot);if(old->connected!=s.connected||old->playing!=s.playing||old->generation!=s.generation||old->message!=s.message)note("Player state connected="+std::to_string(s.connected)+" playing="+std::to_string(s.playing)+" generation="+std::to_string(s.generation)+" position_ms="+std::to_string(s.position)+" timeline_lines="+std::to_string(s.timeline.size())+" message="+s.message);std::atomic_store(&snapshot,std::make_shared<const State>(std::move(s)));}
struct Internet {HINTERNET h; explicit Internet(HINTERNET v):h(v){if(!h)throw std::runtime_error("WinHTTP handle failed");}~Internet(){WinHttpCloseHandle(h);}Internet(const Internet&)=delete;};
nlohmann::json get(const wchar_t* path,size_t limit){
 Internet session(WinHttpOpen(L"L4D2DangoMusic/0.9.3",WINHTTP_ACCESS_TYPE_NO_PROXY,WINHTTP_NO_PROXY_NAME,WINHTTP_NO_PROXY_BYPASS,0));
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
 try {cfg=*std::atomic_load(&settings);CameraWriter cameraWriter;PlayerSession player;auto demoStart=now();
 while(!stopping||std::atomic_load(&pendingSave)){
 if(auto requested=std::atomic_exchange(&pendingSave,std::shared_ptr<const Config>{})){saveState=1;try{auto path=directory/L"config.json";nlohmann::json stored=nlohmann::json::object();if(std::filesystem::exists(path)){std::ifstream old(path);stored=nlohmann::json::parse(old);if(!stored.is_object())throw std::runtime_error("Existing config is not an object");}stored.update(encodeConfig(*requested));std::filesystem::create_directories(directory);auto temp=directory/L"config.json.tmp";{std::ofstream file(temp,std::ios::binary|std::ios::trunc);file<<stored.dump(2)<<'\n';file.flush();if(!file)throw std::runtime_error("Config write failed");}if(std::filesystem::exists(path))std::filesystem::copy_file(path,directory/L"config.json.bak",std::filesystem::copy_options::overwrite_existing);if(!MoveFileExW(temp.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))throw std::runtime_error("Config replace failed");std::atomic_store(&savedSettings,requested);saveState=2;note("Settings saved");}catch(const std::exception& e){saveState=-1;note(e.what());}}
 cameraWriter.drain();source_entities::drain(directory);try{State s;
 if(cfg.demo){s.connected=s.playing=true;s.title="Offline demonstration";s.key="demo";s.duration=16000;s.position=(now()-demoStart)%16000;s.sampled=now();s.timeline=parse("[00:01.00]<1000,500>你好 <1500,500>求生之路\n[00:05.00]Left and right floating lyrics\n[00:09.00]暂停、换歌与逐字时间轴\n[00:13.00]L4D2DangoMusic");s.message="Offline demo";}
 else {player.poll([](){return status(get(statusPath,65536),now());},[](){return peak::splitTimeline(lyrics(get(L"/lyric-all",2*1024*1024)));},[](){return now();},[](State state){publish(std::move(state));});}
 if(cfg.demo)publish(std::move(s));
 }catch(const std::exception& e){bool wasOnline=player.online;publish(player.disconnect(now()));if(wasOnline)note(std::string("Player disconnected; lyric cache invalidated: ")+e.what());}

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
 uint64_t leaseId=0;Line line;peak::Animation animation,translated;peak::VisibleAnimation motion,translationMotion;peak::Layout layout,translationLayout;int index,slot=0;double born,retired=-1,lastPlaced;Vec3 anchor;uint32_t epoch;
 PeakEntry(const Line& l,int64_t next,int i,double tick,Vec3 point,uint32_t e):line(l),animation(l,next),translated(peak::translationAnimation(animation,l.translation,next)),motion(animation.glyphs.size()),translationMotion(translated.glyphs.size()),index(i),born(tick),lastPlaced(tick),anchor(point),epoch(e){}
};
std::vector<std::unique_ptr<PeakEntry>> peakEntries;int peakIndex=-1;bool lastWorldMode=false;float lastWidth=0,lastHeight=0;
void peakFrame(IDirect3DDevice9* dev,const State& state,int at,int64_t song,bool world,const GameCameraFrame& camera,CameraPose pose,float width,float height){
 double clock=now()/1000.;
 bool contextChanged=lastWorldMode!=world||lastWidth!=width||lastHeight!=height||(!peakEntries.empty()&&world&&peakEntries.back()->epoch!=camera.epoch);
 lastWorldMode=world;lastWidth=width;lastHeight=height;
 if(contextChanged){lyricLifecycle.reset();peakEntries.clear();}
 std::string text=at>=0?state.timeline[at].text:std::string{};
 lyricLifecycle.update(state.key,at,text,clock,cfg.lingerMs,cfg.transition,false,cfg.layout.lyricCount);
 peakEntries.erase(std::remove_if(peakEntries.begin(),peakEntries.end(),[&](auto& entry){return std::none_of(lyricLifecycle.entries.begin(),lyricLifecycle.entries.end(),[&](auto& lease){return lease.id==entry->leaseId;});}),peakEntries.end());
 for(auto& lease:lyricLifecycle.entries){auto existing=std::find_if(peakEntries.begin(),peakEntries.end(),[&](auto& e){return e->leaseId==lease.id;});
 if(existing!=peakEntries.end()){(*existing)->retired=lease.retired;continue;}
 auto& line=state.timeline[lease.index];auto next=lease.index+1<int(state.timeline.size())?state.timeline[lease.index+1].start:line.start+5000;
 auto entry=std::make_unique<PeakEntry>(line,next,lease.index,clock,world?anchors.current:Vec3{},camera.epoch);entry->leaseId=lease.id;entry->retired=lease.retired;entry->slot=lease.slot;
 if(world&&cfg.keepAway){entry->anchor=peak::keepDistance(entry->anchor,pose,cfg.layout.lyrics[lease.slot].distance);if(lease.retired<0){anchors.slots.remember(lease.slot,entry->anchor);anchors.current=entry->anchor;}}
 const auto& position=cfg.layout.lyrics[lease.slot];auto initial=screenComponent(position,width,height,cfg.size);if(world)worldComponent(camera,entry->anchor,position,width,height,cfg.worldHeight,cfg.size,initial);float maxPixels=width*cfg.width*cfg.lineWidthScale/std::max(initial.scale,.02f);
 entry->layout=peak::layout(entry->animation.glyphs,[&](const std::string& glyph){return image(dev,glyph).advance;},maxPixels,cfg.size*1.35f);entry->translationLayout=peak::layout(entry->translated.glyphs,[&](const std::string& glyph){return image(dev,glyph).advance;},maxPixels/.72f,cfg.size*1.35f);peakEntries.push_back(std::move(entry));}
 visibleLyricIndex=at;
 for(size_t i=0;i<peakEntries.size();){auto& entry=*peakEntries[i];ExitFrame life;if(entry.retired>=0)life=exitFrame((clock-entry.retired)*1000,cfg.lingerMs,cfg.transition);
 if(life.done){peakEntries.erase(peakEntries.begin()+i);continue;}++i;
 const auto& position=cfg.layout.lyrics[entry.slot];Placement place=screenComponent(position,width,height,cfg.size);
 if(world){if(entry.retired<0&&needsReanchor(clock,entry.lastPlaced,cfg.reanchorMs)){componentAnchor(camera,pose,position,entry.anchor);anchors.slots.remember(entry.slot,entry.anchor);anchors.current=entry.anchor;entry.lastPlaced=clock;}
 if(cfg.keepAway){entry.anchor=peak::keepDistance(entry.anchor,pose,position.distance);if(entry.retired<0){anchors.slots.remember(entry.slot,entry.anchor);anchors.current=entry.anchor;}}if(!worldComponent(camera,entry.anchor,position,width,height,cfg.worldHeight,cfg.size,place))continue;}
 float scale=place.scale,center=place.x,top=place.y-entry.layout.height*scale*.5f;
 top-=life.lift*(world?20.f*scale*cfg.size/cfg.worldHeight:height*.025f);
 for(auto& glyph:entry.layout.glyphs){auto frame=peak::revealFrame(entry.motion,entry.animation,glyph.index,song,clock,cfg.animationSpeed,cfg.progressiveReveal,cfg.pop,cfg.wiggle,cfg.wiggleFrequency,cfg.wiggleAmount,cfg.fallbackReveal);if(frame.alpha<=0)continue;
 auto im=image(dev,entry.animation.glyphs[glyph.index].text);float size=scale*frame.scale;float x=center+(glyph.x+glyph.width*.5f)*scale+frame.x*height/1080.f-im.width*size*.5f;float y=top+(glyph.y+cfg.size*.5f)*scale+frame.y*height/1080.f-im.height*size*.5f;
 styled(dev,im,x,y,size,life.alpha*frame.alpha*cfg.opacity,cfg.textColor,frame.rotation);}
 if(cfg.translation&&!entry.line.translation.empty()){float ts=scale*.72f;
 for(auto& glyph:entry.translationLayout.glyphs){auto frame=peak::revealFrame(entry.translationMotion,entry.translated,glyph.index,song,clock,cfg.animationSpeed,cfg.progressiveReveal,false,false,8,0,cfg.fallbackReveal);if(frame.alpha<=0)continue;
 auto tr=image(dev,entry.translated.glyphs[glyph.index].text);styled(dev,tr,center+(glyph.x+glyph.width*.5f)*ts-tr.width*ts*.5f,top+entry.layout.height*scale+8+glyph.y*ts,ts,life.alpha*frame.alpha*cfg.opacity,cfg.textColor);}}
 }
}
void drawPositionedText(IDirect3DDevice9* dev,const std::string& text,const ComponentPosition& position,const GameCameraFrame& camera,bool world,const CameraPose& pose,float width,float height,DWORD color,float maxWidth,const Vec3* fixed=nullptr){
 if(text.empty())return;auto place=screenComponent(position,width,height,cfg.size);
 if(world){Vec3 anchor;if(fixed)anchor=*fixed;else if(!componentAnchor(camera,pose,position,anchor))return;if(!worldComponent(camera,anchor,position,width,height,cfg.worldHeight,cfg.size,place))return;}
 auto im=image(dev,text);float scale=std::min(place.scale,width*maxWidth/std::max(im.width,1));styled(dev,im,place.x-im.width*scale*.5f,place.y-im.height*scale*.5f,scale,cfg.opacity,color);
}
void drawPositionPreview(IDirect3DDevice9* dev,const GameCameraFrame& camera,bool world,const CameraPose& pose,float width,float height){
 static double started=0;static uint64_t generation=0;static std::shared_ptr<const Config> previous;static bool wasMoving=false;static std::array<peak::VisibleAnimation,MaxLyricSlots> motions;
 double clock=now()/1000.;bool moving=positionPreviewMotion.load();if(generation!=layoutGeneration||previous!=renderedSettings||moving!=wasMoving||clock-started>=4){started=clock;generation=layoutGeneration;previous=renderedSettings;wasMoving=moving;for(auto& motion:motions)motion=peak::VisibleAnimation{};}
 int64_t song=int64_t((clock-started)*1000);
 drawPositionedText(dev,u8"歌名 · Song Title",cfg.layout.title,camera,world,pose,width,height,cfg.textColor,.7f);
 drawPositionedText(dev,u8"作者 · Artist",cfg.layout.artist,camera,world,pose,width,height,cfg.accent,.7f);
 for(int i=0;i<cfg.layout.lyricCount;++i){const auto& position=cfg.layout.lyrics[i];auto place=screenComponent(position,width,height,cfg.size);
 if(world){Vec3 anchor;if(!componentAnchor(camera,pose,position,anchor)||!worldComponent(camera,anchor,position,width,height,cfg.worldHeight,cfg.size,place))continue;}
 Line line;line.text=u8"歌词 "+std::to_string(i+1)+u8" · 这是一句预览歌词";peak::Animation animation(line,3000);auto& glyphs=animation.glyphs;auto layout=peak::layout(glyphs,[&](const std::string& glyph){return image(dev,glyph).advance;},width*cfg.width*cfg.lineWidthScale/std::max(place.scale,.02f),cfg.size*1.35f);
 float top=place.y-layout.height*place.scale*.5f;for(auto& glyph:layout.glyphs){auto frame=moving?peak::revealFrame(motions[i],animation,glyph.index,song,clock,cfg.animationSpeed,cfg.progressiveReveal,cfg.pop,cfg.wiggle,cfg.wiggleFrequency,cfg.wiggleAmount,cfg.fallbackReveal):peak::GlyphFrame{1,1,0,0,0};if(frame.alpha<=0)continue;auto im=image(dev,glyphs[glyph.index].text);float size=place.scale*frame.scale;styled(dev,im,place.x+(glyph.x+glyph.width*.5f)*place.scale-im.width*size*.5f+frame.x*height/1080.f,top+(glyph.y+cfg.size*.5f)*place.scale-im.height*size*.5f+frame.y*height/1080.f,size,cfg.opacity*frame.alpha,cfg.textColor,frame.rotation);}
 }
}
void drawSongInfo(IDirect3DDevice9* dev,const State& state,const GameCameraFrame& camera,bool valid,const CameraPose& pose,float width,float height){
 static std::string song;static Vec3 titleAnchor,artistAnchor;static uint32_t epoch=0;static int64_t placed=0;static std::shared_ptr<const Config> lastSettings;static uint64_t generation=0;
 if(!cfg.showTitle&&!cfg.showArtist){song.clear();return;}if(state.key.empty())return;
 if(song!=state.key||epoch!=camera.epoch||!placed||lastSettings!=renderedSettings||generation!=layoutGeneration||(cfg.reanchorMs>0&&now()-placed>=cfg.reanchorMs)){song=state.key;epoch=camera.epoch;componentAnchor(camera,pose,cfg.layout.title,titleAnchor);componentAnchor(camera,pose,cfg.layout.artist,artistAnchor);placed=now();lastSettings=renderedSettings;generation=layoutGeneration;}
 if(valid&&cfg.keepAway){titleAnchor=peak::keepDistance(titleAnchor,pose,cfg.layout.title.distance);artistAnchor=peak::keepDistance(artistAnchor,pose,cfg.layout.artist.distance);}
 auto limited=[](const std::string& text){std::string out;auto chars=graphemes(text);for(size_t i=0;i<std::min<size_t>(80,chars.size());++i)out+=chars[i].text;return out;};
 if(cfg.showTitle)drawPositionedText(dev,limited(state.title),cfg.layout.title,camera,valid,pose,width,height,cfg.textColor,.7f,&titleAnchor);
 if(cfg.showArtist)drawPositionedText(dev,limited(state.artist),cfg.layout.artist,camera,valid,pose,width,height,cfg.accent,.7f,&artistAnchor);
 if(!valid){song.clear();placed=0;}
}
void wallQuad(IDirect3DDevice9* dev,const wall::Panel& panel,const GameCameraFrame& camera,float width,float height,Image im,DWORD color){
 std::array<wall::Vertex,4> corners;if(!wall::projectPanel(panel,camera,width,height,corners))return;Vertex vertices[4];for(unsigned i=0;i<4;++i){auto& p=corners[i];vertices[i]={p.x-.5f,p.y-.5f,p.z,p.rhw,color,p.u,p.v};}
 dev->SetTexture(0,im.texture);if(!im.texture){dev->SetTextureStageState(0,D3DTSS_COLOROP,D3DTOP_SELECTARG2);dev->SetTextureStageState(0,D3DTSS_ALPHAOP,D3DTOP_SELECTARG2);}
 checked(dev->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP,2,vertices,sizeof(Vertex)),16,"Wall perspective quad");
 if(!im.texture){dev->SetTextureStageState(0,D3DTSS_COLOROP,D3DTOP_MODULATE);dev->SetTextureStageState(0,D3DTSS_ALPHAOP,D3DTOP_MODULATE);}
}
void wallText(IDirect3DDevice9* dev,const wall::Panel& panel,const GameCameraFrame& camera,float width,float height,const std::string& text,float vertical,float maxHeight,DWORD color){
 auto im=image(dev,text);if(!im.texture)return;auto face=panel;face.center=panel.center+panel.normal*.05f+panel.up*(panel.height*vertical);face.width=panel.width*.9f;face.height=face.width*im.height/std::max(im.width,1);if(face.height>panel.height*maxHeight){face.width*=panel.height*maxHeight/face.height;face.height=panel.height*maxHeight;}wallQuad(dev,face,camera,width,height,im,color);
}
struct WallSentence {
 peak::Animation original,translated;peak::VisibleAnimation originalMotion,translatedMotion;std::string title,artist;
 WallSentence(const Line& line,int64_t next,const State& state):original(line,next),translated(peak::translationAnimation(original,line.translation,next)),title(state.title),artist(state.artist){}
};
struct WallTextState {
 std::string key;uint64_t generation=0,session=1,serial=0,current=0,started=0;uint32_t epoch=0;int index=-1;wallscene::SentenceRoute route;
 std::map<uint64_t,std::unique_ptr<WallSentence>> texts;
 void reset(){route={};key.clear();index=-1;current=0;texts.clear();++session;source_walls::submit({session,0,uint64_t(now()),epoch,false});}
 void select(const State& music,const PlaybackDisplayFrame& display,int at,const GameCameraFrame& camera){
 bool active=display.lyrics&&at>=0&&at<int(music.timeline.size())&&cfg.wallLiveLyrics;
 if(!active){if(index>=0)reset();source_walls::submit({session,0,uint64_t(now()),camera.epoch,false});return;}
 if(key!=music.key||generation!=music.generation||epoch!=camera.epoch){reset();key=music.key;generation=music.generation;epoch=camera.epoch;}
 if(index!=at){route={};index=at;current=++serial;started=uint64_t(now());const auto& line=music.timeline[at];auto next=at+1<int(music.timeline.size())?music.timeline[at+1].start:line.start+5000;texts[current]=std::make_unique<WallSentence>(line,next,music);}
 source_walls::submit({session,current,uint64_t(now()),camera.epoch,true,!route.fallback});
 }
} wallTextState;
void wallGlyphs(IDirect3DDevice9* dev,const wall::Panel& panel,const GameCameraFrame& camera,float width,float height,const peak::Animation& animation,peak::VisibleAnimation& motion,const peak::Layout& layout,float unit,float top,int64_t song,double clock,bool translated,float opacity=1){
 for(const auto& glyph:layout.glyphs){auto frame=peak::revealFrame(motion,animation,glyph.index,song,clock,cfg.animationSpeed,cfg.progressiveReveal,!translated&&cfg.pop,!translated&&cfg.wiggle,cfg.wiggleFrequency,cfg.wiggleAmount,cfg.fallbackReveal);if(frame.alpha<=0)continue;
 auto im=image(dev,animation.glyphs[glyph.index].text);if(!im.texture)continue;auto face=panel;
 face.center=panel.center+panel.normal*.1f+panel.right*((glyph.x+glyph.width*.5f+frame.x)*unit)+panel.up*(top-(glyph.y+cfg.size*.5f+frame.y)*unit);
 face.width=im.width*unit*frame.scale;face.height=im.height*unit*frame.scale;
 float angle=frame.rotation;face.right=panel.right*std::cos(angle)+panel.up*std::sin(angle);face.up=panel.up*std::cos(angle)-panel.right*std::sin(angle);
 DWORD color=(DWORD(std::clamp(cfg.opacity*frame.alpha*opacity,0.f,1.f)*255)<<24)|(cfg.textColor&0xffffff);wallQuad(dev,face,camera,width,height,im,color);
 }
}
bool drawWallExperiment(IDirect3DDevice9* dev,const GameCameraFrame& camera,float width,float height,const State& music,const PlaybackDisplayFrame& display,int at){
 wallTextState.select(music,display,at,camera);auto state=std::atomic_load(&source_walls::snapshot);auto options=std::atomic_load(&source_walls::options);
 if(cfg.wallDebug){auto message=state->tick?state->message:std::string("Waiting for Source callback: ")+source_camera::status();auto banner=image(dev,"WALL | F8: place | "+message+" | panels="+std::to_string(state->panels.size()));draw(dev,banner,16,16,std::min(.6f,width*.95f/std::max(banner.width,1)),1.f,0xffcc88);}
 bool live=cfg.wallLiveLyrics,currentVisible=false;uint64_t tick=uint64_t(now());
 bool assigned=state->epoch==camera.epoch&&tick>=state->tick&&tick-state->tick<=350&&std::any_of(state->panels.begin(),state->panels.end(),[&](const auto& item){return item.token==wallTextState.current&&item.session==wallTextState.session;});
 wallTextState.route.choose(assigned,tick,wallTextState.started,cfg.wallFallback);
 if(wallTextState.route.fallback&&display.lyrics&&live)source_walls::submit({wallTextState.session,wallTextState.current,tick,camera.epoch,true,false});
 if(!usableCamera(camera,tick)||state->epoch!=camera.epoch||tick<state->tick||tick-state->tick>350||source_walls::clearRequested.load())return false;
 for(auto& item:state->panels){if(!item.visible||tick<item.visibleTick||tick-item.visibleTick>350)continue;const auto& panel=item.panel;
 WallSentence* sentence=nullptr;float opacity=1;
 if(live){if(wallTextState.route.fallback&&item.token==wallTextState.current)continue;auto text=wallTextState.texts.find(item.token);if(!display.lyrics||item.session!=wallTextState.session||text==wallTextState.texts.end())continue;sentence=text->second.get();opacity=wallscene::alpha(item,tick,*options);if(opacity<=0)continue;}
 if(!wallscene::onScreen(panel,camera,width,height))continue;
 if(live&&item.token==wallTextState.current)currentVisible=true;
 if(cfg.wallBackdrop)wallQuad(dev,panel,camera,width,height,Image{},(DWORD(176*opacity)<<24)|0x18202c);
 if(cfg.wallDebug)wallText(dev,panel,camera,width,height,"WALL PANEL "+std::to_string(item.id),.4f,.12f,(DWORD(255*opacity)<<24)|0x99ccff);
 if(!live){wallText(dev,panel,camera,width,height,u8"固定在墙上的歌词挂画",0,.54f,0xff000000u|(cfg.textColor&0xffffff));continue;}
 auto limited=[](const std::string& text){std::string result;auto chars=graphemes(text);for(size_t i=0;i<std::min<size_t>(chars.size(),80);++i)result+=chars[i].text;return result;};
 if(cfg.showTitle)wallText(dev,panel,camera,width,height,limited(sentence->title),.37f,.12f,(DWORD(255*opacity)<<24)|(cfg.textColor&0xffffff));
 if(cfg.showArtist)wallText(dev,panel,camera,width,height,limited(sentence->artist),.24f,.09f,(DWORD(255*opacity)<<24)|(cfg.accent&0xffffff));
 auto measure=[&](const std::string& text){return image(dev,text).advance;};float maxPixels=cfg.size*18.f*cfg.lineWidthScale;
 auto original=peak::layout(sentence->original.glyphs,measure,maxPixels,cfg.size*1.35f);auto translation=peak::layout(sentence->translated.glyphs,measure,maxPixels/.72f,cfg.size*1.35f);
 bool showTranslation=cfg.translation&&!translation.glyphs.empty();float widest=1;for(auto row:original.rows)widest=std::max(widest,row);if(showTranslation)for(auto row:translation.rows)widest=std::max(widest,row*.72f);
 float contentHeight=original.height+(showTranslation?cfg.size*.25f+translation.height*.72f:0);float unit=std::min(panel.width*.9f/widest,panel.height*.62f/std::max(contentHeight,1.f));unit=std::min(unit,panel.height*.27f/std::max(cfg.size,1));
 float top=contentHeight*unit*.5f-panel.height*.08f;auto song=lyricPosition(display.position,cfg.demo?0:cfg.offset);double clock=now()/1000.;
 wallGlyphs(dev,panel,camera,width,height,sentence->original,sentence->originalMotion,original,unit,top,song,clock,false,opacity);
 if(showTranslation)wallGlyphs(dev,panel,camera,width,height,sentence->translated,sentence->translatedMotion,translation,unit*.72f,top-(original.height+cfg.size*.25f)*unit,song,clock,true,opacity);
 }
 // Keep payloads only for retained panels and the current pending sentence.
 for(auto it=wallTextState.texts.begin();it!=wallTextState.texts.end();){bool used=it->first==wallTextState.current||std::any_of(state->panels.begin(),state->panels.end(),[&](const auto& item){return item.session==wallTextState.session&&item.token==it->first;});if(!used)it=wallTextState.texts.erase(it);else ++it;}
 return currentVisible;
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
 bool preview=positionPreview.load();bool escapeDown=(GetAsyncKeyState(VK_ESCAPE)&0x8000)!=0;if(preview&&!nativeHud.isOpen()&&escapeDown&&!escapeHeld){positionPreview=false;preview=false;}escapeHeld=escapeDown;
 auto currentSettings=std::atomic_load(&settings);cfg=*currentSettings;bool wallMode=source_walls::enabled.load();auto notice=std::atomic_load(&menuNotice);bool showNotice=!notice->text.empty()&&uint64_t(now())>=notice->tick&&uint64_t(now())-notice->tick<2000;if(!cfg.enabled&&!preview&&!showNotice&&!nativeHud.isOpen())return;
 if(lastWallMode!=wallMode){wallTextState.reset();lastWallMode=wallMode;peakEntries.clear();lyricLifecycle.reset();anchors.reset();++layoutGeneration;}
 if(lastPositionPreview!=preview){wallTextState.reset();++layoutGeneration;peakEntries.clear();lyricLifecycle.reset();anchors.reset();playbackCursor={};lastPositionPreview=preview;}
 if(renderedSettings!=currentSettings){wallTextState.reset();clear();peakEntries.clear();peakIndex=-1;lyricLifecycle.reset();anchors.reset();renderedSettings=currentSettings;}
 captureCamera(dev,present);
 const unsigned path=present->is_swapchain_present?1:0;
 if(!presentLogged[path]){presentLogged[path]=true;note(std::string("L4D2DangoMusic first Present path=")+(path?"SwapChain":"Device")+(present->swap_chain?" current-chain=yes":" current-chain=no"));}
 try {if(cache.size()>256)clear();auto state=std::atomic_load(&snapshot);auto display=playbackDisplay.select(*state,now());
 if(display.reset){wallTextState.reset();visibleLyricIndex=-1;++layoutGeneration;peakEntries.clear();lyricLifecycle.reset();anchors.reset();playbackCursor={};}
 auto cursor=preview?ClockSelection{}:playbackCursor.select(state->key,display.position,state->playing);auto position=cursor.position;if(cursor.seek){wallTextState.reset();peakEntries.clear();lyricLifecycle.reset();anchors.reset();}auto at=position<0?-1:indexAt(state->timeline,lyricPosition(position,cfg.demo?0:cfg.offset));
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
 if(cfg.enabled&&cfg.demo&&!preview&&!wallMode){auto banner=image(dev,"L4D2DangoMusic 0.9.3 DEMO - "+std::string(path?"SwapChain":"Device"));draw(dev,banner,16,16,std::min(1.f,desc.Width*.9f/std::max(banner.width,1)),1.f);}
 auto camera=std::atomic_load(&source_camera::snapshot);const auto& gameFrame=*camera;CameraPose pose;
 bool cameraGood=usableCamera(gameFrame,now())&&cameraPose(gameFrame,pose);bool cameraValid=cfg.worldLyrics&&cameraGood;
 if(cfg.enabled&&cfg.worldLyrics&&cfg.debugOverlay&&!preview&&!wallMode){auto status=image(dev,cameraValid?std::string("GAME CAMERA: ACTIVE (experimental)"):std::string("GAME CAMERA: ")+source_camera::status()+" - screen fallback");draw(dev,status,16,cfg.demo?64.f:16.f,std::min(.65f,desc.Width*.9f/std::max(status.width,1)),.8f,cameraValid?0x99ffbb:0xffcc88);}
 if(!cfg.enabled&&!preview){}
 else if(preview)drawPositionPreview(dev,gameFrame,cameraValid,pose,float(desc.Width),float(desc.Height));
 else if(wallMode){drawWallExperiment(dev,gameFrame,float(desc.Width),float(desc.Height),*state,display,at);
 if(cfg.wallLiveLyrics&&display.lyrics&&wallTextState.route.fallback){if(cameraGood)anchors.update(gameFrame,pose,state->key,at);else anchors.reset();peakFrame(dev,*state,at,lyricPosition(position,cfg.demo?0:cfg.offset),cameraGood,gameFrame,pose,float(desc.Width),float(desc.Height));}
 else{peakEntries.clear();lyricLifecycle.reset();}}

 else {
 if(!cameraValid)anchors.reset();else anchors.update(gameFrame,pose,state->key,at);
 if(cameraValid&&cfg.worldTestAnchor){Placement test;if(projectAnchor(gameFrame,anchors.test,float(desc.Width),float(desc.Height),cfg.worldHeight,cfg.size,test)){auto im=image(dev,"WORLD ANCHOR +");styled(dev,im,test.x-im.width*test.scale*.5f,test.y,test.scale,.9f,0x99ffbb);}}
 if(display.metadata)drawSongInfo(dev,*state,gameFrame,cameraValid,pose,float(desc.Width),float(desc.Height));
 if(display.lyrics)peakFrame(dev,*state,at,lyricPosition(position,cfg.demo?0:cfg.offset),cameraValid,gameFrame,pose,float(desc.Width),float(desc.Height));
 if(at<0&&cfg.debugOverlay){auto im=image(dev,text);styled(dev,im,16,96,.65f,.7f,cfg.textColor);}
 }
 if(showNotice&&!nativeHud.isOpen()){auto im=image(dev,notice->text);float scale=std::min(.65f,desc.Width*.8f/std::max(im.width,1));draw(dev,im,(desc.Width-im.width*scale)*.5f,desc.Height*.88f,scale,1,0x99ffcc);}
 nativeHud.frame(dev,float(desc.Width),float(desc.Height));
 dev->EndScene();guard.scene=false;}
 }

 }catch(...){if(logger)logger(LDB_HOST_LOG_ERROR,"Lyric render failed");}
}
bool load(const std::filesystem::path& pluginDirectory){try{directory=pluginDirectory/L"L4D2DangoMusic";auto path=directory/L"config.json";
 if(std::filesystem::exists(path)){std::ifstream f(path);cfg=decodeConfig(nlohmann::json::parse(f));}
 std::atomic_store(&settings,std::make_shared<const Config>(cfg));std::atomic_store(&savedSettings,std::filesystem::exists(path)?std::make_shared<const Config>(cfg):std::shared_ptr<const Config>{});source_walls::configure(cfg);nativeHud.loadAppearance(cfg.hud);

 note("L4D2DangoMusic 0.9.3 config="+path.u8string()+" mode="+(cfg.demo?"DEMO":"BLF/LX")+" port="+std::to_string(cfg.port)+" offsetMs="+std::to_string(cfg.offset)+" displayMode="+cfg.mode);
 stopping=false;worker=std::thread(run);return TRUE;}catch(const std::exception& e){if(logger)logger(LDB_HOST_LOG_ERROR,e.what());return FALSE;}catch(...){return FALSE;}}
void releasePresentationBackup();
void WINAPI created(IDirect3DDevice9*){nativeHud.release();wallTextState.reset();++deviceGeneration;releasePresentationBackup();anchors.reset();peakEntries.clear();peakIndex=-1;lyricLifecycle.reset();clear();reportedErrors=0;drewPixels=false;note("L4D2DangoMusic device lifecycle: created/destroyed; textures cleared");}
void WINAPI beforeReset(IDirect3DDevice9*,const D3DPRESENT_PARAMETERS*,BOOL){nativeHud.beforeReset();wallTextState.reset();releasePresentationBackup();anchors.reset();peakEntries.clear();peakIndex=-1;lyricLifecycle.reset();clear();}
void WINAPI afterReset(IDirect3DDevice9*,HRESULT hr,BOOL){nativeHud.afterReset(hr);++deviceGeneration;reportedErrors=0;checked(hr,5,"Reset");note("L4D2DangoMusic Reset completed; textures will rebuild");}
void WINAPI shutdown(){nativeHud.shutdown();source_camera::frameListener=nullptr;source_walls::stop();source_entities::enabled=false;stopping=true;wake.notify_all();if(worker.joinable())worker.join();releasePresentationBackup();clear();}
}
namespace {
struct PresentationBackup {
 IDirect3DSurface9* clean=nullptr;IDirect3DDevice9* owner=nullptr;D3DSURFACE_DESC description{};
 void release(){if(clean)clean->Release();clean=nullptr;owner=nullptr;}
 bool prepare(IDirect3DDevice9* device,const D3DSURFACE_DESC& desc){
 if(clean&&(owner!=device||description.Width!=desc.Width||description.Height!=desc.Height||description.Format!=desc.Format||description.MultiSampleType!=desc.MultiSampleType||description.MultiSampleQuality!=desc.MultiSampleQuality))release();
 if(!clean){if(!checked(device->CreateRenderTarget(desc.Width,desc.Height,desc.Format,desc.MultiSampleType,desc.MultiSampleQuality,FALSE,&clean,nullptr),11,"Create pristine backbuffer backup"))return false;owner=device;description=desc;}return true;
 }
};
PresentationBackup presentationBackup;
void releasePresentationBackup(){presentationBackup.release();}
template<class Present> HRESULT paintAndPresent(IDirect3DDevice9* device,IDirect3DSwapChain9* chain,Present present){
 nativeHud.pollHotkey(device);
 IDirect3DSurface9* back=nullptr;D3DSURFACE_DESC desc{};PresentInfo info{chain!=nullptr,chain};
 auto capture=[&](){auto config=std::atomic_load(&settings);if(!config->enabled&&!positionPreview.load()&&!nativeHud.isOpen())return false;
 HRESULT result=chain?chain->GetBackBuffer(0,D3DBACKBUFFER_TYPE_MONO,&back):device->GetBackBuffer(0,0,D3DBACKBUFFER_TYPE_MONO,&back);
 if(!checked(result,12,"Acquire presented backbuffer"))return false;
 if(!checked(back->GetDesc(&desc),13,"Describe presented backbuffer")||!presentationBackup.prepare(device,desc))return false;
 return checked(device->StretchRect(back,nullptr,presentationBackup.clean,nullptr,D3DTEXF_NONE),14,"Capture pristine backbuffer");};
 auto restore=[&](){if(!checked(device->StretchRect(presentationBackup.clean,nullptr,back,nullptr,D3DTEXF_NONE),15,"Restore pristine backbuffer"))++restoreErrors;};
 HRESULT result=E_FAIL;try{result=overlayTransaction(capture,[&](){render(device,&info);++presentationCount;},present,restore);}catch(...){note("Presentation transaction failed");if(back)back->Release();throw;}
 if(back)back->Release();return result;
}
struct Patch {void** slot;void* original;void* replacement;};
std::array<Patch,64> patches{};std::atomic<size_t> patchCount{0};SRWLOCK patchLock=SRWLOCK_INIT;std::filesystem::path pluginDirectory;
void fileLog(int,const char* message){std::error_code error;auto dataDirectory=pluginDirectory/L"L4D2DangoMusic";std::filesystem::create_directories(dataDirectory,error);if(error)return;std::ofstream file(dataDirectory/L"L4D2DangoMusic.log",std::ios::app);file<<"L4D2DangoMusic 0.9.3 "<<message<<'\n';}
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
 ++presentDepth;auto hr=presentDepth==1?paintAndPresent(device,nullptr,[&](){return fn(device,source,target,window,dirty);}):fn(device,source,target,window,dirty);--presentDepth;return hr;
}
HRESULT WINAPI chainPresent(IDirect3DSwapChain9* chain,const RECT* source,const RECT* target,HWND window,const RGNDATA* dirty,DWORD flags){
 auto fn=reinterpret_cast<HRESULT(WINAPI*)(IDirect3DSwapChain9*,const RECT*,const RECT*,HWND,const RGNDATA*,DWORD)>(original(chain,3));
 ++presentDepth;HRESULT hr;
 IDirect3DDevice9* device=nullptr;if(presentDepth==1&&SUCCEEDED(chain->GetDevice(&device))&&device){hr=paintAndPresent(device,chain,[&](){return fn(chain,source,target,window,dirty,flags);});device->Release();}
 else hr=fn(chain,source,target,window,dirty,flags);--presentDepth;return hr;
}
void hookChain(IDirect3DSwapChain9* chain){if(chain&&(*reinterpret_cast<void***>(chain))[3]!=reinterpret_cast<void*>(&chainPresent)&&patch(*reinterpret_cast<void***>(chain),3,reinterpret_cast<void*>(&chainPresent)))note("SwapChain Present attached");}
HRESULT WINAPI getChain(IDirect3DDevice9* device,UINT index,IDirect3DSwapChain9** chain){auto fn=reinterpret_cast<HRESULT(WINAPI*)(IDirect3DDevice9*,UINT,IDirect3DSwapChain9**)>(original(device,14));auto hr=fn(device,index,chain);if(SUCCEEDED(hr)&&chain&&*chain)hookChain(*chain);return hr;}
HRESULT WINAPI addChain(IDirect3DDevice9* device,D3DPRESENT_PARAMETERS* params,IDirect3DSwapChain9** chain){auto fn=reinterpret_cast<HRESULT(WINAPI*)(IDirect3DDevice9*,D3DPRESENT_PARAMETERS*,IDirect3DSwapChain9**)>(original(device,13));auto hr=fn(device,params,chain);if(SUCCEEDED(hr)&&chain&&*chain)hookChain(*chain);return hr;}
HRESULT WINAPI resetDevice(IDirect3DDevice9* device,D3DPRESENT_PARAMETERS* params){auto fn=reinterpret_cast<HRESULT(WINAPI*)(IDirect3DDevice9*,D3DPRESENT_PARAMETERS*)>(original(device,16));beforeReset(device,params,FALSE);auto hr=fn(device,params);afterReset(device,hr,FALSE);if(SUCCEEDED(hr)){IDirect3DSwapChain9* chain=nullptr;if(SUCCEEDED(device->GetSwapChain(0,&chain))){hookChain(chain);chain->Release();}}return hr;}
HRESULT WINAPI presentEx(IDirect3DDevice9Ex* device,const RECT* source,const RECT* target,HWND window,const RGNDATA* dirty,DWORD flags){auto fn=reinterpret_cast<HRESULT(WINAPI*)(IDirect3DDevice9Ex*,const RECT*,const RECT*,HWND,const RGNDATA*,DWORD)>(original(device,121));++presentDepth;auto hr=presentDepth==1?paintAndPresent(device,nullptr,[&](){return fn(device,source,target,window,dirty,flags);}):fn(device,source,target,window,dirty,flags);--presentDepth;return hr;}
HRESULT WINAPI resetEx(IDirect3DDevice9Ex* device,D3DPRESENT_PARAMETERS* params,D3DDISPLAYMODEEX* mode){auto fn=reinterpret_cast<HRESULT(WINAPI*)(IDirect3DDevice9Ex*,D3DPRESENT_PARAMETERS*,D3DDISPLAYMODEEX*)>(original(device,132));beforeReset(device,params,TRUE);auto hr=fn(device,params,mode);afterReset(device,hr,TRUE);return hr;}
std::string menuText;
const char* menuAction(void*);
std::string escaped(std::string s){std::string out;for(char c:s){if(c=='"'||c=='\\')out+='\\';if(c!='\r'&&c!='\n')out+=c;}return out;}
std::string item(const std::string& label,uintptr_t id){if(label.rfind(u8"返回",0)==0)return {};char callback[32],data[32];std::snprintf(callback,sizeof(callback),"0x%lx",static_cast<unsigned long>(reinterpret_cast<uintptr_t>(&menuAction)));std::snprintf(data,sizeof(data),"0x%lx",static_cast<unsigned long>(id));return "\""+escaped(label)+"\" { \"callback\" \""+callback+"\" \"user_data\" \""+data+"\" }\n";}
const char* page(const std::string& title,const std::string& rows){menuText="\""+escaped(title)+"\" {\n"+rows+"}\n";return menuText.c_str();}
const char* menuPage(const hud::Document& doc){std::string rows;for(auto& row:doc.rows)rows+=item(row.label,row.action);return page(doc.title,rows);}
std::vector<std::string> relatedModules(){std::vector<std::string> paths;HANDLE snapshot=CreateToolhelp32Snapshot(TH32CS_SNAPMODULE|TH32CS_SNAPMODULE32,GetCurrentProcessId());if(snapshot==INVALID_HANDLE_VALUE)return paths;MODULEENTRY32W entry{};entry.dwSize=sizeof(entry);
 if(Module32FirstW(snapshot,&entry))do{if(entry.hModule==ownModule)continue;std::wstring name=entry.szModule;std::transform(name.begin(),name.end(),name.begin(),[](wchar_t c){return c>=L'A'&&c<=L'Z'?c+32:c;});
 if(name.find(L"dango")!=std::wstring::npos||GetProcAddress(entry.hModule,"L4D2DangoMusicShutdown")||GetProcAddress(entry.hModule,"LDBDangoMusicShutdown"))paths.push_back(std::filesystem::path(entry.szExePath).u8string());}while(Module32NextW(snapshot,&entry));CloseHandle(snapshot);return paths;}
const char* diagnosticMenu(bool audit=true){std::string rows=item(u8"版本：0.9.3；进程："+std::to_string(GetCurrentProcessId()),0)+item(u8"当前 DLL："+ownPath.u8string(),0);auto others=relatedModules();
 if(audit)note("Runtime audit pid="+std::to_string(GetCurrentProcessId())+" dll="+ownPath.u8string()+" related_modules="+std::to_string(others.size()));
 for(auto& path:others){rows+=item(u8"其他歌词模块："+path,0);if(audit)note("Other lyric module: "+path);}
 if(others.empty())rows+=item(u8"当前进程未发现其他相关模块（Host 需另查）",0);
 auto music=std::atomic_load(&snapshot);rows+=item(std::string(u8"播放器：")+(music->connected?(music->playing?u8"播放中":u8"暂停/停止"):u8"未连接")+u8"；会话 "+std::to_string(music->generation)+u8"；歌词 "+std::to_string(music->timeline.size()),0);
 return page(u8"L4D2DangoMusic 0.9.3 · 诊断",rows+item(u8"返回主菜单",0));}
void changeSettings(const Config& next){source_walls::configure(next);std::atomic_store(&settings,std::make_shared<const Config>(next));note("Settings applied enabled="+std::to_string(next.enabled)+" preset="+std::to_string(next.presetId)+" preview="+std::to_string(positionPreview.load()));}
std::string on(bool v){return v?u8"开":u8"关";}
const char* channelMenu(const Config& c,bool accent,unsigned channel){uint32_t color=accent?c.accent:c.textColor;unsigned value=(color>>((2-channel)*8))&255;std::string rows;
 const char* names[]={"R","G","B"};const char* labels[]={"-16","-1","+1","+16","0","64","128","192","255"};
 for(unsigned i=0;i<9;++i)rows+=item(labels[i],10000+(accent?1000:0)+channel*100+i);
 return page(std::string(accent?u8"光晕 ":u8"正文 ")+names[channel]+" = "+std::to_string(value)+" "+colorText(color),rows);}
std::string positionValue(float value,bool percent=false){char text[40];std::snprintf(text,sizeof(text),percent?"%.0f%%":"%.2f",percent?value*100:value);return text;}
const char* rootMenu(bool title);
const char* displayMenu(const Config& c){return menuPage(menus::display(c));}
const char* colorMenu(const Config& c){return menuPage(menus::colors(c));}
const char* animationMenu(const Config& c){return menuPage(menus::animation(c));}
const char* wallMenu(){auto c=*std::atomic_load(&settings);auto state=std::atomic_load(&source_walls::snapshot);return menuPage(menus::wall(c,state->message));}
const char* wallOptionsMenu(){auto c=*std::atomic_load(&settings);const char* sizes[]={u8"小80×28",u8"中120×40",u8"大180×60"};return page(u8"场景挂画参数",item(std::string(u8"新挂画尺寸：")+sizes[c.wallSize]+u8"（循环切换）",712)+item(u8"最多挂画："+std::to_string(c.wallCapacity)+u8"（2/4/6/8）",713)+item(u8"搜索距离："+std::to_string(int(c.wallRange))+u8"（300/600/900）",714)+item(u8"最小间距："+std::to_string(int(c.wallSpacing))+u8"（80/140/220）",715)+item(u8"旧句额外停留："+std::to_string(c.wallLingerMs)+u8"ms（0/2000/4000/8000）",716)+item(u8"淡出时间："+std::to_string(c.wallFadeMs)+u8"ms（500/1000/2000）",717)+item(u8"返回场景挂画",30));}
std::string componentName(int index){return index==0?u8"歌名":index==1?u8"作者":u8"歌词 "+std::to_string(index-1);}
std::string previewExitRows(){return item(u8"动画预览："+on(positionPreviewMotion.load()),2006)+item(u8"保存设置并结束预览",2098)+item(u8"结束预览，暂不保存",2099);}
const char* positionHome(const Config& c){return page(u8"位置调节 · 固定预览",item(u8"歌词位置与数量（"+std::to_string(c.layout.lyricCount)+u8" 个位置）",2001)+item(u8"歌名：高度、远近与大小",2100)+item(u8"作者：高度、远近与大小",2101)+item(u8"全部组件位置重置",2004)+previewExitRows());}
const char* positionGroup(const Config& c,bool lyrics){std::string rows;
 if(lyrics){rows+=item(u8"全部歌词高度",3201)+item(u8"全部歌词远近",3202)+item(u8"歌词位置数量："+std::to_string(c.layout.lyricCount),2003);for(int i=0;i<c.layout.lyricCount;++i)rows+=item(componentName(i+2),2102+i);rows+=item(u8"全部歌词位置重置",2005);}
 else rows=item(u8"歌名",2100)+item(u8"作者",2101);
 return page(lyrics?u8"歌词 · 按编号依次轮换":u8"歌名与作者",rows+item(u8"返回位置调节",2000)+previewExitRows());}
const char* componentMenu(const Config& c,int index){auto p=component(c.layout,index);return page(componentName(index)+u8" · 位置",item(u8"左右："+positionValue(p.x,true),3000+index*10)+item(u8"高度（距上边缘）："+positionValue(p.y,true),3001+index*10)+item(u8"距离："+positionValue(p.distance),3002+index*10)+item(u8"大小："+positionValue(p.size)+"x",3003+index*10)+item(u8"重置此组件位置",2300+index)+item(u8"返回组件列表",index<2?2002:2001)+previewExitRows());}
const char* positionAxisMenu(const Config& c,int index,int axis){bool group=index==2+MaxLyricSlots;auto p=group?c.layout.lyrics[0]:component(c.layout,index);const char* names[]={u8"左右",u8"高度（距上边缘）",u8"距离",u8"大小"};
 const char* choices[][5]={{u8"向左 5%",u8"向左 1%",u8"向右 1%",u8"向右 5%",u8"恢复默认左右"},{u8"向上 5%",u8"向上 1%",u8"向下 1%",u8"向下 5%",u8"恢复默认高度"},{u8"拉近 50",u8"拉近 10",u8"推远 10",u8"推远 50",u8"恢复默认距离"},{"-0.25x","-0.05x","+0.05x","+0.25x",u8"恢复默认大小"}};
 std::string rows;for(int choice=0;choice<5;++choice)rows+=item(choices[axis][choice],20000+index*1000+axis*100+choice);
 std::string value=positionValue(positionAxis(p,axis),axis<2);if(group){float lo=positionAxis(p,axis),hi=lo;for(int i=1;i<c.layout.lyricCount;++i){auto other=c.layout.lyrics[i];lo=std::min(lo,positionAxis(other,axis));hi=std::max(hi,positionAxis(other,axis));}value=positionValue(lo,axis<2)+" ~ "+positionValue(hi,axis<2);}
 return page((group?std::string(u8"全部歌词"):componentName(index))+" · "+names[axis]+": ["+value+"]",rows+item(group?u8"返回歌词列表":u8"返回此组件",group?2001:2100+index)+previewExitRows());}
const char* menuActionImpl(void* argument){uintptr_t id=reinterpret_cast<uintptr_t>(argument);std::lock_guard<std::mutex> lock(settingsMutex);auto c=*std::atomic_load(&settings);
 if(id==0||id==2099){positionPreview=false;return rootMenu(false);}
 if(id==2098){saveState=1;std::atomic_store(&pendingSave,std::make_shared<const Config>(c));positionPreview=false;return page(u8"保存设置 · 已恢复当前歌词",item(u8"已提交后台保存，返回查看结果",0));}
 if(id==27){positionPreview=true;positionPreviewMotion=true;return positionHome(c);}
 if(id==23||id==2000){positionPreview=true;return positionHome(c);}
 if(id==2006){positionPreview=true;positionPreviewMotion=!positionPreviewMotion.load();return positionHome(c);}
 if(id>=3200&&id<=3203){positionPreview=true;return positionAxisMenu(c,2+MaxLyricSlots,int(id-3200));}
 if(id==2001||id==2002){positionPreview=true;return positionGroup(c,id==2001);}
 if(id==2003){positionPreview=true;std::string rows;for(int i=1;i<=MaxLyricSlots;++i)rows+=item(std::to_string(i)+u8" 个歌词位置",2200+i);return page(u8"歌词位置数量（依次轮换）",rows+item(u8"返回歌词位置",2001)+previewExitRows());}
 if(id==2004||id==2005){positionPreview=true;auto defaults=ComponentLayout{};if(id==2004){defaults.lyricCount=c.layout.lyricCount;c.layout=defaults;}else c.layout.lyrics=defaults.lyrics;changeSettings(c);return positionHome(c);}
 if(id>=2201&&id<=2209){positionPreview=true;c.layout.lyricCount=int(id-2200);changeSettings(c);return positionGroup(c,true);}
 if(id>=2100&&id<2100+2+MaxLyricSlots){positionPreview=true;return componentMenu(c,int(id-2100));}
 if(id>=2300&&id<2300+2+MaxLyricSlots){positionPreview=true;int index=int(id-2300);component(c.layout,index)=component(ComponentLayout{},index);changeSettings(c);return componentMenu(c,index);}
 if(id>=3000&&id<=3103){int index=int(id-3000)/10,axis=int(id-3000)%10;if(index>=2+MaxLyricSlots||axis>3)return nullptr;positionPreview=true;return positionAxisMenu(c,index,axis);}
 if(id>=20000&&id<=31304){int code=int(id-20000),index=code/1000,axis=code%1000/100,choice=code%100;if(index>2+MaxLyricSlots||axis>3||choice>4)return nullptr;positionPreview=true;if(index==2+MaxLyricSlots)adjustLyricGroup(c.layout,axis,choice);else adjustPosition(c.layout,index,axis,choice);changeSettings(c);return positionAxisMenu(c,index,axis);}
 positionPreview=false;
 if(id==800){auto frame=std::atomic_load(&source_entities::snapshot);return page(u8"实体接口实验 · 只读诊断",item(menus::value(u8"定时采集",on(source_entities::enabled.load())),801)+item(u8"立即导出当前快照",802)+item(menus::value(u8"已识别实体",std::to_string(frame->records.size())),0)+item(menus::value(u8"扫描条目",std::to_string(frame->occupied)),0)+item(menus::value(u8"已写帧",std::to_string(source_entities::fileFrames.load())),0)+item(menus::value(u8"状态",frame->status),0)+item(u8"输出目录："+directory.u8string(),0)+item(u8"当前为网络坐标，不是骨骼或渲染姿态",0));}
 if(id==801){source_entities::enabled=!source_entities::enabled.load();if(source_entities::enabled.load())source_entities::resetRequested=true;return nullptr;}
 if(id==802){source_entities::dumpRequested=true;wake.notify_all();return nullptr;}
 if(id==30)return wallMenu();
 if(id==711)return wallOptionsMenu();
 if(id>=700&&id<=717){
 if(id==700)c.wallEnabled=!c.wallEnabled;
 else if(id==701){c.wallEnabled=true;source_walls::placeAt=GetTickCount64()+3000;}
 else if(id==702)c.wallLiveLyrics=!c.wallLiveLyrics;
 else if(id==703)c.wallBackdrop=!c.wallBackdrop;
 else if(id==704){source_walls::clearRequested=true;return wallMenu();}
 else if(id==705)c.wallDebug=!c.wallDebug;
 else if(id>=706&&id<=708)c.wallSize=int(id-706);
 else if(id==709)c.wallAutomatic=!c.wallAutomatic;
 else if(id==710)c.wallFallback=!c.wallFallback;
 else if(id==712)c.wallSize=(c.wallSize+1)%3;
 else if(id==713)c.wallCapacity=c.wallCapacity>=8?2:(c.wallCapacity/2+1)*2;
 else if(id==714)c.wallRange=c.wallRange<600?600:c.wallRange<900?900:300;
 else if(id==715)c.wallSpacing=c.wallSpacing<140?140:c.wallSpacing<220?220:80;
 else if(id==716)c.wallLingerMs=c.wallLingerMs<2000?2000:c.wallLingerMs<4000?4000:c.wallLingerMs<8000?8000:0;
 else if(id==717)c.wallFadeMs=c.wallFadeMs<1000?1000:c.wallFadeMs<2000?2000:500;
 changeSettings(c);return id>=712?wallOptionsMenu():wallMenu();}
 if(id==24)return diagnosticMenu();
 if(id==20)return displayMenu(c);
 if(id==21)return colorMenu(c);
 if(id==22)return animationMenu(c);
 if(id==35)return menuPage(menus::more());
 if(id==36)return menuPage(menus::world(c));
 if(id==37)return menuPage(menus::metadata(c));
 if(id==38)return menuPage(menus::timing(c));
 if(id==39)return menuPage(menus::wallAppearance(c));
 if(id==33)return page(u8"复用距离上限 · "+positionValue(c.reuseDistanceFactor)+u8" × 各歌词配置距离",item("1x",620)+item("1.5x",621)+item("2x",622)+item(u8"3x（默认）",623)+item("4x",624)+item("5x",625)+item("10x",626));
 if(id==31)return page(u8"单行长度（不改变字号和距离） · "+positionValue(c.lineWidthScale)+"x",item("0.75x",600)+item(u8"1x（默认）",601)+item("1.5x",602)+item("2x",603)+item("3x",604)+item("4x",605)+item(u8"返回主菜单",0));
 if(id==19)return page(u8"长句回到当前视野 · "+reanchorSeconds(c.reanchorMs)+u8" 秒",item(u8"关闭",580)+item(u8"每 3 秒",581)+item(u8"每 5 秒",582)+item(u8"每 10 秒",583)+item(u8"自定义秒数（当前 "+reanchorSeconds(c.reanchorMs)+u8" 秒）",34));
 if(id==34)return page(u8"自定义回归间隔 · "+reanchorSeconds(c.reanchorMs)+u8" 秒",item(u8"-10 秒",584)+item(u8"-1 秒",585)+item(u8"-0.1 秒",586)+item(u8"+0.1 秒",587)+item(u8"+1 秒",588)+item(u8"+10 秒",589)+item(u8"重设为 3 秒",590)+item(u8"关闭回归",591)+item(u8"范围 0.1～300 秒；0 为关闭",0));
 if(id==10){std::string rows;for(int i=0;i<6;++i)rows+=item(presetName(i),100+i);return page(std::string(u8"歌词预设 · ")+presetName(colorPresetId(c)),rows);}
 if(id==11||id==12)return menuPage(menus::palette(c,id==12));
 if(id==40||id==41)return menuPage(menus::channels(c,id==41));
 if(id>=400&&id<=402)return channelMenu(c,false,unsigned(id-400));if(id>=410&&id<=412)return channelMenu(c,true,unsigned(id-410));
 if(id==13)return page(u8"显字动画速度 · "+positionValue(c.animationSpeed)+u8"x（不改变歌曲进度）",item("0.5x",500)+item("0.75x",501)+item("1x",502)+item("1.5x",503)+item("2x",504));
 if(id==14)return page(u8"退场耗时 · "+std::to_string(c.transition)+" ms",item("200 ms",520)+item("400 ms",521)+item("700 ms",522)+item("1000 ms",523)+item("1500 ms",524));
 if(id==15)return page(u8"换句后额外停留 · "+std::to_string(c.lingerMs)+" ms",item("0 ms",540)+item("500 ms",541)+item("1000 ms",542)+item("1500 ms",543)+item("2000 ms",544));
 if(id==16)return page(menus::value(u8"同步偏移",std::to_string(c.offset)+"ms")+u8"（正值提前）",item("-100 ms",560)+item("-50 ms",561)+item("+50 ms",562)+item("+100 ms",563)+item(u8"重设为 500ms",564));
 if(id==18){saveState=1;std::atomic_store(&pendingSave,std::make_shared<const Config>(c));return page(u8"保存设置",item(u8"已提交后台保存，返回主菜单查看结果",0));}
 if(id>=100&&id<=105)c=applyColorPreset(c,int(id-100));
 else if(id>=300&&id<=317&&(id<=307||id>=310)){uint32_t colors[]={0xf7e1d7,0xffffff,0xff99cc,0x70d6ff,0xffcc66,0x99ffcc,0xc5a3ff,0xff8855};if(id>=310)c.accent=colors[id-310];else c.textColor=colors[id-300];c.presetId=-1;}
 else if(id>=10000&&id<11209){bool accent=id>=11000;unsigned encoded=unsigned(id-(accent?11000:10000)),channel=encoded/100,choice=encoded%100;if(channel>2||choice>8)return nullptr;auto& color=accent?c.accent:c.textColor;unsigned shift=(2-channel)*8;int value=int((color>>shift)&255);const int deltas[]={-16,-1,1,16};const int fixed[]={0,64,128,192,255};value=choice<4?std::clamp(value+deltas[choice],0,255):fixed[choice-4];color=(color&~(255u<<shift))|(uint32_t(value)<<shift);c.presetId=-1;changeSettings(c);return channelMenu(c,accent,channel);}
 else if(id>=500&&id<=504){float values[]={.5f,.75f,1,1.5f,2};c.animationSpeed=values[id-500];}
 else if(id>=520&&id<=524){int values[]={200,400,700,1000,1500};c.transition=values[id-520];}
 else if(id>=540&&id<=544){int values[]={0,500,1000,1500,2000};c.lingerMs=values[id-540];}
 else if(id>=620&&id<=626){const float values[]={1,1.5f,2,3,4,5,10};c.reuseDistanceFactor=values[id-620];}
 else if(id>=600&&id<=605){float values[]={.75f,1,1.5f,2,3,4};c.lineWidthScale=values[id-600];}
 else if(id>=584&&id<=589)c.reanchorMs=adjustReanchor(c.reanchorMs,int(id-584));
 else if(id==590)c.reanchorMs=3000;
 else if(id==591)c.reanchorMs=0;
 else if(id>=580&&id<=583){int values[]={0,3000,5000,10000};c.reanchorMs=values[id-580];}
 else if(id>=560&&id<=564){int values[]={-100,-50,50,100};c.offset=id==564?500:std::clamp(c.offset+values[id-560],-10000,10000);}
 else switch(id){case 32:c.fallbackReveal=!c.fallbackReveal;break;case 28:c.progressiveReveal=!c.progressiveReveal;break;case 29:c.reuseVisibleAnchor=!c.reuseVisibleAnchor;break;case 26:c.pop=c.wiggle=true;c.wiggleFrequency=8;c.wiggleAmount=.8f;c.animationSpeed=1;break;case 1:c.enabled=!c.enabled;break;case 2:c.translation=!c.translation;break;case 3:c.wiggle=!c.wiggle;break;case 4:c.pop=!c.pop;break;case 5:c.showTitle=!c.showTitle;break;case 6:c.showArtist=!c.showArtist;break;case 7:c.worldLyrics=!c.worldLyrics;break;case 8:c.glow=!c.glow;c.presetId=-1;break;case 9:c.keepAway=!c.keepAway;break;default:return nullptr;}
 changeSettings(c);
 if(id==2||id==5||id==6||id==7||id==9||id==29|| (id>=580&&id<=583))return displayMenu(c);
 if(id==3||id==4||id==26||id==28||id==32||(id>=500&&id<=544))return animationMenu(c);
 if(id==8||(id>=300&&id<=317))return colorMenu(c);
 return rootMenu(false);
}
const char* menuAction(void* argument){
 auto id=reinterpret_cast<uintptr_t>(argument);auto result=menuActionImpl(argument);
 if(!hud::opensChild(id)&&id){auto c=*std::atomic_load(&settings);std::string text=u8"设置已实时应用";
 auto status=[&](const char* label,bool value){text=std::string(label)+(value?u8"：已开启":u8"：已关闭");};
 switch(id){case 801:status(u8"实体诊断采集",source_entities::enabled.load());break;case 802:text=u8"已请求后台导出当前实体快照";break;case 1:status(u8"歌词显示",c.enabled);break;case 2:status(u8"翻译",c.translation);break;case 3:status(u8"抖动",c.wiggle);break;case 4:status(u8"弹跳",c.pop);break;case 5:status(u8"歌名",c.showTitle);break;case 6:status(u8"作者",c.showArtist);break;case 7:status(u8"世界歌词",c.worldLyrics);break;case 8:status(u8"柔光",c.glow);break;case 9:status(u8"靠近后退（悬浮世界歌词）",c.keepAway);break;case 28:status(u8"按演唱进度显字",c.progressiveReveal);break;case 29:status(u8"沿用可见锚点",c.reuseVisibleAnchor);break;case 32:text=c.fallbackReveal?u8"无逐字时间轴：估算渐显":u8"无逐字时间轴：整句显示";break;case 18:case 2098:text=u8"已提交保存设置";break;case 2099:text=u8"已结束位置预览";break;case 700:status(u8"场景挂画",c.wallEnabled);break;case 702:text=c.wallLiveLyrics?u8"挂画内容：歌词轮换":u8"挂画内容：固定示例";break;case 703:status(u8"挂画背景",c.wallBackdrop);break;case 705:status(u8"挂画调试标识",c.wallDebug);break;case 709:text=c.wallAutomatic?u8"自动寻找墙面":u8"手动挂画轮换";break;case 710:status(u8"新句无墙面时固定世界歌词",c.wallFallback);break;case 704:text=u8"已请求清空挂画，下一句重新寻找墙面";break;default:break;}
 if(id>=100&&id<=105)text=std::string(u8"预设：")+presetName(c.presetId);
 if(id>=10000&&id<11209)text=std::string(id>=11000?u8"光晕颜色：":u8"正文颜色：")+colorText(id>=11000?c.accent:c.textColor);
 if(id>=2201&&id<=2209)text=u8"歌词位置数量："+std::to_string(c.layout.lyricCount);
 if(id>=20000&&id<=31304){int code=int(id-20000),index=code/1000,axis=code%1000/100;const char* labels[]={u8"左右",u8"高度",u8"距离",u8"大小"};if(index<=2+MaxLyricSlots&&axis<4){auto p=index==2+MaxLyricSlots?c.layout.lyrics[0]:component(c.layout,index);text=(index==2+MaxLyricSlots?std::string(u8"全部歌词"):componentName(index))+labels[axis]+u8"："+positionValue(positionAxis(p,axis),axis<2);}}
 if(id>=500&&id<=504)text=u8"动画速度："+positionValue(c.animationSpeed)+"x";
 if(id>=520&&id<=524)text=u8"退场耗时："+std::to_string(c.transition)+"ms";
 if(id>=540&&id<=544)text=u8"换句额外停留："+std::to_string(c.lingerMs)+"ms";
 if(id==712){const char* sizes[]={u8"小80×28",u8"中120×40",u8"大180×60"};text=std::string(u8"新挂画尺寸：")+sizes[c.wallSize];}
 if(id>=620&&id<=626)text=u8"复用距离上限："+positionValue(c.reuseDistanceFactor)+u8" × 各歌词配置距离";
 if(id>=600&&id<=605)text=u8"单行长度："+positionValue(c.lineWidthScale)+"x";
 if(id>=580&&id<=591)text=c.reanchorMs>0?u8"长句回归间隔："+reanchorSeconds(c.reanchorMs)+u8" 秒":u8"长句回归：已关闭";
 if(id>=560&&id<=564)text=u8"同步偏移："+std::to_string(c.offset)+"ms";
 if(id==713)text=u8"最多挂画："+std::to_string(c.wallCapacity);
 if(id==714)text=u8"搜索距离："+std::to_string(int(c.wallRange));
 if(id==715)text=u8"挂画间距："+std::to_string(int(c.wallSpacing));
 if(id==716)text=u8"旧句停留："+std::to_string(c.wallLingerMs)+"ms";
 if(id==717)text=u8"淡出时间："+std::to_string(c.wallFadeMs)+"ms";
 std::atomic_store(&menuNotice,std::make_shared<const MenuNotice>(MenuNotice{uint64_t(now()),text}));}
 return hud::callbackResult(id,result);
}
std::string buildNativeMenu(uintptr_t id){
 if(!id)return rootMenu(false);
 if(id==24)return diagnosticMenu(false);
 auto result=menuActionImpl(reinterpret_cast<void*>(id));return result?result:std::string{};
}
void activateNativeMenu(uintptr_t id){menuAction(reinterpret_cast<void*>(id));}
void closeNativeMenu(){positionPreview=false;}
void updateHudAppearance(const hud::Appearance& appearance){std::lock_guard<std::mutex> lock(settingsMutex);auto c=*std::atomic_load(&settings);c.hud=appearance;changeSettings(c);}
std::string nativeMenuNotice(){auto notice=std::atomic_load(&menuNotice);auto tick=uint64_t(now());return tick>=notice->tick&&tick-notice->tick<2000?notice->text:std::string{};}
const char* openNativeMenu(void*){nativeHud.open();return nullptr;}
const char* nativeEntryMenu(){static std::string entry;char callback[32];std::snprintf(callback,sizeof(callback),"0x%lx",static_cast<unsigned long>(reinterpret_cast<uintptr_t>(&openNativeMenu)));entry="\"L4D2DangoMusic\" { \"打开歌词控制面板\" { \"callback\" \""+std::string(callback)+"\" \"user_data\" \"0x0\" } }";return entry.c_str();}
const char* rootMenu(bool title){if(title)return "L4D2DangoMusic 0.9.3";positionPreview=false;auto c=*std::atomic_load(&settings);auto saved=std::atomic_load(&savedSettings);auto state=saveState.load();bool saving=state==1||bool(std::atomic_load(&pendingSave));return menuPage(menus::root(c,menus::saveLabel(c,saved?*saved:Config{},saving,state<0,bool(saved))));}

class LyricsPlugin final:public IL4NPlugin {
 bool started=false,attempted=false,duplicate=false;HANDLE renderOwner=nullptr;
public:
 const char* GetName()override{return "L4D2DangoMusic";}
 const char* GetVersion()override{return "0.9.3";}
 const char* RequestHudMenu(bool title)override{start();if(duplicate){if(title)return u8"L4D2DangoMusic 0.9.3 · 重复副本已阻止";return page(u8"重复副本未启用绘制",item(u8"查看实际 DLL 路径与其他模块",24));}if(title)return "L4D2DangoMusic 0.9.3";nativeHud.open();return nativeEntryMenu();}
 void start(){if(attempted)return;attempted=true;GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,reinterpret_cast<const wchar_t*>(&fileLog),&ownModule);wchar_t path[32768]{};if(!GetModuleFileNameW(ownModule,path,32768))return;ownPath=path;pluginDirectory=ownPath.parent_path();logger=fileLog;nativeHud.configure(buildNativeMenu,activateNativeMenu,closeNativeMenu,nativeMenuNotice,note,updateHudAppearance);
 std::wstring ownerName=L"Local\\L4D2DangoMusic-render-"+std::to_wstring(GetCurrentProcessId());SetLastError(ERROR_SUCCESS);renderOwner=CreateMutexW(nullptr,FALSE,ownerName.c_str());DWORD ownerError=GetLastError();
 if(!renderOwner||ownerError==ERROR_ALREADY_EXISTS){duplicate=true;if(renderOwner)CloseHandle(renderOwner);renderOwner=nullptr;note("Duplicate renderer blocked pid="+std::to_string(GetCurrentProcessId())+" dll="+ownPath.u8string());return;}
 note("Runtime pid="+std::to_string(GetCurrentProcessId())+" dll="+ownPath.u8string());for(auto& other:relatedModules())note("Other lyric module: "+other);
 started=load(pluginDirectory);if(!started){CloseHandle(renderOwner);renderOwner=nullptr;}source_camera::logger=note;if(started)source_camera::frameListener=captureSourceFeatures;}
 void OnModuleLoaded(const char* name,std::uintptr_t handle)override{start();if(!started||!name)return;std::string n(name);if(n=="client"||n=="client.dll")source_camera::clientModule=reinterpret_cast<HMODULE>(handle);if(n=="engine"||n=="engine.dll")source_camera::engineModule=reinterpret_cast<HMODULE>(handle);source_camera::connect();}
 void OnGameLaunch()override{start();if(!started)return;if(!source_camera::clientModule)source_camera::clientModule=GetModuleHandleW(L"client.dll");if(!source_camera::engineModule)source_camera::engineModule=GetModuleHandleW(L"engine.dll");source_camera::connect();}
 void OnD3DDeviceCreated(void* pointer,bool)override{start();if(!started||!pointer)return;auto device=static_cast<IDirect3DDevice9*>(pointer);auto table=*reinterpret_cast<void***>(device);
 cfg=*std::atomic_load(&settings);created(device);patch(table,17,reinterpret_cast<void*>(&devicePresent));patch(table,16,reinterpret_cast<void*>(&resetDevice));patch(table,14,reinterpret_cast<void*>(&getChain));patch(table,13,reinterpret_cast<void*>(&addChain));IDirect3DSwapChain9* chain=nullptr;if(SUCCEEDED(device->GetSwapChain(0,&chain))&&chain){hookChain(chain);chain->Release();}IDirect3DDevice9Ex* ex=nullptr;if(SUCCEEDED(device->QueryInterface(__uuidof(IDirect3DDevice9Ex),reinterpret_cast<void**>(&ex)))&&ex){auto et=*reinterpret_cast<void***>(ex);patch(et,121,reinterpret_cast<void*>(&presentEx));patch(et,132,reinterpret_cast<void*>(&resetEx));ex->Release();}note("x86 Client D3D9 callbacks installed; no Host plugin required");}
 void stop(){source_camera::stop();if(started){shutdown();started=false;}for(size_t i=patchCount.load();i>0;--i){auto& p=patches[i-1];if(!source_camera::readable(p.slot,sizeof(void*)))continue;DWORD old=0;if(VirtualProtect(p.slot,sizeof(void*),PAGE_READWRITE,&old)){InterlockedCompareExchangePointer(p.slot,p.original,p.replacement);DWORD ignored=0;VirtualProtect(p.slot,sizeof(void*),old,&ignored);}}patchCount.store(0);if(renderOwner){CloseHandle(renderOwner);renderOwner=nullptr;}}
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
