#pragma once
#include <windows.h>
#include <commctrl.h>
#include <d3d9.h>
#include <atomic>
#include <cstdio>
#include <string>
#include "native_menu_model.hpp"
#include "hud_menu_policy.hpp"
#include "imgui/imgui.h"
#include "imgui/backends/imgui_impl_dx9.h"

namespace dango::hud {
// The bridge's native HUD design: L4N entry only, keyboard controls and a
// private ImGui/D3D9 context. Reuse the lyric plugin's existing Present/Reset
// chain and pristine backbuffer transaction rather than installing more hooks.
class NativeHud {
 using Builder=std::string(*)(uintptr_t);
 using Action=void(*)(uintptr_t);
 using Close=void(*)();
 using Notice=std::string(*)();
 using Log=void(*)(const std::string&);
 Builder build=nullptr;Action act=nullptr;Close closed=nullptr;Notice notice=nullptr;Log log=nullptr;
 std::atomic<bool> opened{false},restart{false},f10{false};
 ImGuiContext* context=nullptr;IDirect3DDevice9* device=nullptr;HWND window=nullptr;bool backend=false,subclass=false,hotkeyHeld=false;
 KeyGate gate;MenuStack stack;enum class Page {Settings,Appearance,Position};std::atomic<Page> page{Page::Settings};
 unsigned preset=0,selected=0,step=8;float opacity=.48f,fontPixels=22,x=8,y=14;std::array<bool,4> arrows{};uint64_t retryAfter=0;bool submitted=false,fontReady=false;
 ImVec4 background{.015f,.018f,.020f,1};
 struct ContextScope {ImGuiContext* previous;explicit ContextScope(ImGuiContext* c):previous(ImGui::GetCurrentContext()){ImGui::SetCurrentContext(c);}~ContextScope(){ImGui::SetCurrentContext(previous);}};
 static constexpr UINT_PTR SubclassId=0x44414e47;
 static bool required(WPARAM key){return (key>='0'&&key<='9')||(key>=VK_NUMPAD0&&key<=VK_NUMPAD9)||key==VK_ESCAPE||key==VK_OEM_2||key==VK_DIVIDE||key==VK_OEM_MINUS||key==VK_OEM_PLUS||key==VK_SUBTRACT||key==VK_ADD;}
 static LRESULT CALLBACK proc(HWND h,UINT m,WPARAM w,LPARAM l,UINT_PTR,DWORD_PTR ref){
 auto self=reinterpret_cast<NativeHud*>(ref);
 bool keyboard=m==WM_KEYDOWN||m==WM_KEYUP||m==WM_SYSKEYDOWN||m==WM_SYSKEYUP;
 if(self->isOpen()&&keyboard&&(required(w)||(self->page==Page::Position&&(w>=VK_LEFT&&w<=VK_DOWN))))return 0;
 if(self->isOpen()&&m==WM_CHAR&&((w>='0'&&w<='9')||w=='/'||w=='-'||w=='='||w=='+'||w==27))return 0;
 return DefSubclassProc(h,m,w,l);
 }
 static bool down(int key){return (GetAsyncKeyState(key)&0x8000)!=0;}
 static bool localWindow(HWND h){DWORD pid=0;return h&&IsWindow(h)&&GetWindowThreadProcessId(h,&pid)&&pid==GetCurrentProcessId();}
 struct WindowSearch {HWND window=nullptr;LONG64 area=0;};
 static BOOL CALLBACK searchWindow(HWND h,LPARAM data){auto& s=*reinterpret_cast<WindowSearch*>(data);if(!localWindow(h)||!IsWindowVisible(h)||GetWindow(h,GW_OWNER))return TRUE;RECT r{};if(!GetClientRect(h,&r))return TRUE;auto area=LONG64(r.right-r.left)*(r.bottom-r.top);if(area>s.area){s.window=h;s.area=area;}return TRUE;}
 static HWND findWindow(IDirect3DDevice9* d){
 D3DDEVICE_CREATION_PARAMETERS creation{};if(SUCCEEDED(d->GetCreationParameters(&creation))&&localWindow(creation.hFocusWindow))return creation.hFocusWindow;
 IDirect3DSwapChain9* chain=nullptr;if(SUCCEEDED(d->GetSwapChain(0,&chain))&&chain){D3DPRESENT_PARAMETERS p{};auto hr=chain->GetPresentParameters(&p);chain->Release();if(SUCCEEDED(hr)&&localWindow(p.hDeviceWindow))return p.hDeviceWindow;}
 auto foreground=GetForegroundWindow();if(localWindow(foreground))return foreground;
 WindowSearch search;EnumWindows(searchWindow,reinterpret_cast<LPARAM>(&search));return search.window;
 }
 KeyGate::Keys keys(){KeyGate::Keys k{};for(int i=0;i<10;++i)k[i]=down('0'+i)||down(VK_NUMPAD0+i);k[10]=down(VK_ESCAPE);k[11]=down(VK_OEM_2)||down(VK_DIVIDE);k[12]=down(VK_OEM_MINUS)||down(VK_SUBTRACT);k[13]=down(VK_OEM_PLUS)||down(VK_ADD);return k;}
 void close(){opened=false;gate.reset();if(closed)closed();}
 void applyPreset(){if(preset==0){background={.015f,.018f,.020f,1};opacity=.48f;}if(preset==1){background={.10f,.105f,.11f,1};opacity=.42f;}if(preset==2){background={.055f,.065f,.075f,1};opacity=.66f;}if(preset==3){background={.01f,.012f,.014f,1};opacity=.28f;}}
 static void text(const std::string& s,ImVec4 color={.94f,.94f,.91f,1}){ImGui::PushStyleColor(ImGuiCol_Text,color);ImGui::TextWrapped("%s",s.c_str());ImGui::PopStyleColor();}
 static void row(unsigned n,const std::string& s,bool enabled=true){text((enabled?"["+std::to_string(n)+"]  ":"     ")+s,enabled?ImVec4(.94f,.94f,.91f,1):ImVec4(.58f,.60f,.60f,1));ImGui::Dummy({1,3});}
 void font(){auto& io=ImGui::GetIO();char windows[MAX_PATH]{};if(GetWindowsDirectoryA(windows,MAX_PATH))for(auto name:{"msyh.ttc","msyhbd.ttc","simhei.ttf"}){
 auto path=std::string(windows)+"\\Fonts\\"+name;if(GetFileAttributesA(path.c_str())!=INVALID_FILE_ATTRIBUTES&&io.Fonts->AddFontFromFileTTF(path.c_str(),24,nullptr,io.Fonts->GetGlyphRangesChineseSimplifiedCommon()))return;}
 io.Fonts->AddFontDefault();if(log)log("Native HUD Chinese font unavailable; using fallback");}
 bool attach(IDirect3DDevice9* d){
 if(device==d&&context)return true;release();HWND candidate=findWindow(d);
 auto previous=ImGui::GetCurrentContext();IMGUI_CHECKVERSION();context=ImGui::CreateContext();ImGui::SetCurrentContext(previous);
 {ContextScope scope(context);auto& io=ImGui::GetIO();io.ConfigFlags|=ImGuiConfigFlags_NoMouse|ImGuiConfigFlags_NoMouseCursorChange;io.IniFilename=nullptr;io.LogFilename=nullptr;font();
 auto& s=ImGui::GetStyle();s.WindowRounding=0;s.WindowBorderSize=1;s.WindowPadding={9,7};s.ItemSpacing={7,3};s.Colors[ImGuiCol_Border]={.72f,.74f,.74f,.72f};backend=ImGui_ImplDX9_Init(d);}
 if(!backend){if(log)log("Native HUD D3D9 backend initialization failed");release();return false;}device=d;window=candidate;
 // SetWindowSubclass cannot attach across threads. Input filtering is optional:
 // keep the panel and focused keyboard polling alive if this filter is unavailable.
 DWORD owner=window?GetWindowThreadProcessId(window,nullptr):0;
 if(owner==GetCurrentThreadId())subclass=SetWindowSubclass(window,proc,SubclassId,reinterpret_cast<DWORD_PTR>(this))!=FALSE;
 if(log){char status[256];std::snprintf(status,sizeof(status),"Native HUD attached window=%p windowThread=%lu renderThread=%lu subclass=%d; %s",static_cast<void*>(window),static_cast<unsigned long>(owner),static_cast<unsigned long>(GetCurrentThreadId()),int(subclass),subclass?"window input filter ready":"focused polling fallback");log(status);}return true;
 }
 void move(bool focused){const int vk[]={VK_LEFT,VK_UP,VK_RIGHT,VK_DOWN};for(size_t i=0;i<4;++i){bool held=focused&&gate.armed&&down(vk[i]);if(held&&!arrows[i]){if(i==0)x-=step;if(i==1)y-=step;if(i==2)x+=step;if(i==3)y+=step;}arrows[i]=held;}}
 void appearance(const KeyGate::Keys& edge){
 static const char* names[]={u8"深黑",u8"烟灰",u8"石墨",u8"高透明"};
 if(edge[1]){preset=(preset+1)%4;applyPreset();selected=0;}if(edge[2])selected=1;if(edge[3])selected=2;
 if(selected==1){if(edge[12])opacity=std::max(.2f,opacity-.05f);if(edge[13])opacity=std::min(.95f,opacity+.05f);}
 if(selected==2){if(edge[12])fontPixels=std::max(16.f,fontPixels-1);if(edge[13])fontPixels=std::min(32.f,fontPixels+1);}
 if(edge[4]){f10=!f10.load();hotkeyHeld=down(VK_F10);}if(edge[5])page=Page::Position;
 row(1,std::string(u8"背景预设：")+names[preset]);row(2,u8"背景不透明度："+std::to_string(int(opacity*100))+"%"+(selected==1?"  [- / =]":""));
 row(3,u8"字体大小："+std::to_string(int(fontPixels))+" px"+(selected==2?"  [- / =]":""));row(4,std::string("F10: ")+(f10?u8"开":u8"关"));row(5,u8"调整菜单位置");text(u8"外观仅对本次游戏会话生效",{.58f,.60f,.60f,1});
 }
public:
 void configure(Builder b,Action a,Close c,Notice n,Log l){build=b;act=a;closed=c;notice=n;log=l;}
 bool isOpen()const{return opened.load();}
 void open(){if(!opened.exchange(true)){restart=true;if(log)log("Native HUD open requested");}}
 void pollHotkey(IDirect3DDevice9* d){if(!f10)return;bool held=down(VK_F10);HWND target=window;if(!target){D3DDEVICE_CREATION_PARAMETERS c{};if(SUCCEEDED(d->GetCreationParameters(&c)))target=c.hFocusWindow;}
 auto foreground=GetForegroundWindow();bool focused=target&&(foreground==target||foreground==GetAncestor(target,GA_ROOT));if(focused&&held&&!hotkeyHeld){if(isOpen())close();else open();}hotkeyHeld=held;}
 void frame(IDirect3DDevice9* d,float width,float height){
 if(!isOpen()||!build||!act)return;if(GetTickCount64()<retryAfter)return;if(!attach(d)){retryAfter=GetTickCount64()+1000;return;}retryAfter=0;ContextScope scope(context);
 if(restart.exchange(false)){stack.reset();page=Page::Settings;gate.reset();arrows.fill(false);}
 if(!localWindow(window))window=findWindow(d);auto foreground=GetForegroundWindow();bool focused=localWindow(foreground)&&(!window||foreground==window||foreground==GetAncestor(window,GA_ROOT));
 auto edge=gate.update(keys(),focused);if(edge[10]||edge[11]){close();return;}
 if(edge[0]){if(page==Page::Position)page=Page::Appearance;else if(page==Page::Appearance)page=Page::Settings;else if(!stack.back()){close();return;}}
 auto& io=ImGui::GetIO();io.DisplaySize={width,height};static thread_local LARGE_INTEGER last{};LARGE_INTEGER tick{},frequency{};QueryPerformanceCounter(&tick);QueryPerformanceFrequency(&frequency);io.DeltaTime=last.QuadPart?float(double(tick.QuadPart-last.QuadPart)/double(frequency.QuadPart)):1.f/60;io.DeltaTime=std::clamp(io.DeltaTime,.001f,.1f);last=tick;
 if(!fontReady){fontReady=ImGui_ImplDX9_CreateDeviceObjects();if(!fontReady){ImGui_ImplDX9_InvalidateDeviceObjects();if(log)log("Native HUD font texture creation failed; retrying");retryAfter=GetTickCount64()+1000;return;}}
 ImGui_ImplDX9_NewFrame();ImGui::NewFrame();float panelWidth=std::min(680.f,std::max(180.f,width-16));x=std::clamp(x,0.f,std::max(0.f,width-120));y=std::clamp(y,0.f,std::max(0.f,height-50));
 ImGui::SetNextWindowPos({x,y},ImGuiCond_Always);ImGui::SetNextWindowSizeConstraints({panelWidth,0},{panelWidth,std::max(50.f,height-y-8)});ImGui::SetNextWindowBgAlpha(opacity);ImGui::PushStyleColor(ImGuiCol_WindowBg,background);
 uintptr_t action=0;bool openAppearance=false;
 if(ImGui::Begin("L4D2DangoMusic Native HUD",nullptr,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_NoScrollbar|ImGuiWindowFlags_AlwaysAutoResize)){
 ImGui::SetWindowFontScale(fontPixels/24.f);
 if(page==Page::Settings){auto doc=document(build(stack.current().page));if(stack.current().page==0)doc.rows.push_back({u8"菜单外观",9000});stack.clamp(doc.rows.size());
 if(edge[12])stack.previous();if(edge[13])stack.next(doc.rows.size());
 text(doc.title,{1,.84f,.22f,1});ImGui::Separator();size_t first=stack.current().sheet*MenuStack::PageSize;
 for(size_t i=first;i<doc.rows.size()&&i<first+MenuStack::PageSize;++i){auto& item=doc.rows[i];unsigned n=unsigned(i-first+1);row(n,item.label,item.action!=0);if(edge[n]&&item.action)action=item.action;}
 size_t count=std::max<size_t>(1,(doc.rows.size()+MenuStack::PageSize-1)/MenuStack::PageSize);if(count>1)text(u8"[-] 上页    [=] 下页    "+std::to_string(stack.current().sheet+1)+" / "+std::to_string(count),{.58f,.60f,.60f,1});
 openAppearance=action==9000;
 }else if(page==Page::Appearance){text(u8"菜单外观",{1,.84f,.22f,1});ImGui::Separator();appearance(edge);
 }else{text(u8"调整菜单位置",{1,.84f,.22f,1});ImGui::Separator();if(edge[12]&&step>1)--step;if(edge[13]&&step<64)++step;if(edge[1]){x=8;y=14;}move(focused);text(u8"位置："+std::to_string(int(x))+", "+std::to_string(int(y)));text(u8"方向键移动菜单；[- / =] 步宽："+std::to_string(step)+" px",{1,.84f,.22f,1});row(1,u8"重置到左上角");}
 if(notice){auto message=notice();if(!message.empty()){ImGui::Separator();text(message,{1,.84f,.22f,1});}}
 ImGui::Separator();text(stack.frames.size()==1&&page==Page::Settings?u8"[0] 关闭    [/ / Esc] 关闭":u8"[0] 返回    [/ / Esc] 关闭",{1,.84f,.22f,1});
 }ImGui::End();ImGui::PopStyleColor();ImGui::Render();auto data=ImGui::GetDrawData();ImGui_ImplDX9_RenderDrawData(data);if(!submitted&&data&&data->TotalVtxCount){submitted=true;if(log)log("Native HUD first frame submitted vertices="+std::to_string(data->TotalVtxCount));}
 if(openAppearance){page=Page::Appearance;return;}if(action){act(action);if(action==2098||action==2099)stack.reset();else if(opensChild(action))stack.enter(action);}
 }
 void beforeReset(){fontReady=false;submitted=false;if(context&&backend){ContextScope scope(context);ImGui_ImplDX9_InvalidateDeviceObjects();}}
 void afterReset(HRESULT hr){if(context&&backend&&SUCCEEDED(hr)){ContextScope scope(context);fontReady=ImGui_ImplDX9_CreateDeviceObjects();}}
 void release(){if(window&&subclass)RemoveWindowSubclass(window,proc,SubclassId);subclass=false;
 if(context){auto previous=ImGui::GetCurrentContext();ImGui::SetCurrentContext(context);if(backend)ImGui_ImplDX9_Shutdown();ImGui::DestroyContext(context);ImGui::SetCurrentContext(previous==context?nullptr:previous);}context=nullptr;backend=false;fontReady=false;submitted=false;device=nullptr;window=nullptr;gate.reset();}
 void shutdown(){close();release();}
};
}
