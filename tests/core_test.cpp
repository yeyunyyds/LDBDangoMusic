#include "core.hpp"
#include "present_target.hpp"
#include "presentation.hpp"
#include "camera_diagnostics.hpp"
#include <iostream>
#include <stdexcept>
void check(bool b){if(!b)throw std::runtime_error("check failed");}
struct Surface {int id;};
struct FakeDevice {Surface primary{1};int calls=0;int GetBackBuffer(int,int,int,Surface** out){++calls;*out=&primary;return 0;}};
struct FakeChain {Surface back;int calls=0;int GetBackBuffer(int,int,Surface** out){++calls;*out=&back;return 0;}};
int main(){using namespace dango;
 check(lyricPosition(1000,500)==1500&&lyricPosition(1000,-500)==500);
 check(motion(-1).alpha==0&&motion(180).alpha==1&&motion(0).scale>1);
 check(outgoingAlpha(0,380)==1&&outgoingAlpha(380,380)==0);
 check(graphemes(u8"e\u0301👩‍💻🇨🇳").size()==3);
 Line timed{1000,"你好","",{{1000,400,"你好"}}};auto chars=reveal(timed,2000,true);check(chars.size()==2&&chars[0].start==1000&&chars[1].start==1200);
 auto words=reveal(timed,2000,false);check(words.size()==1&&words[0].text=="你好");
 Line plain{1000,"Hello world 你好","",{}};auto estimated=reveal(plain,5000,false);check(estimated[0].text=="Hello"&&estimated[1].text==" "&&estimated.back().text=="好");
 check(reveal(Line{0,"","",{}},0,true).empty());
 CameraQueue<3> queue;CameraSample camera;camera.tick=123;camera.view[0]=camera.view[5]=camera.view[10]=camera.view[15]=1;
 check(identityMatrix(camera.view));check(queue.push(camera)&&queue.push(camera)&&!queue.push(camera)&&queue.dropped==1);CameraSample output;check(queue.pop(output)&&output.tick==123&&queue.pop(output)&&!queue.pop(output));
 check(queue.push(camera)&&queue.pop(output)&&!queue.pop(output));
 auto record=cameraJson(camera);check(record["camera_confirmed"]==false&&record["view_identity"]==true&&record["view"].size()==16);
 camera.view[0]=std::numeric_limits<float>::quiet_NaN();check(!finiteMatrix(camera.view)&&!identityMatrix(camera.view));
 FakeDevice dev;FakeChain primary{{2}},additional{{3}};Surface* target=nullptr;
 check(currentBackBuffer(&dev,&additional,true,0,&target,-1)==0&&target->id==3&&dev.calls==0);
 check(currentBackBuffer(&dev,&primary,false,0,&target,-1)==0&&target->id==2&&dev.calls==0);
 check(currentBackBuffer(&dev,static_cast<FakeChain*>(nullptr),false,0,&target,-1)==0&&target->id==1&&dev.calls==1);
 check(currentBackBuffer(&dev,static_cast<FakeChain*>(nullptr),true,0,&target,-1)==-1&&dev.calls==1);

 auto t=parse("[offset:-100]\n[00:02.50]later\n[00:01.000][00:03.000]repeat\nmetadata\n");check(t.size()==3&&t[0].start==900&&t[1].start==2400);check(indexAt(t,899)==-1&&indexAt(t,900)==0&&indexAt(t,9999)==2);
 auto l=lyrics(nlohmann::json{{"lxlyric","[00:01.00]<1000,250>你<1250,250>好"},{"tlyric","[00:01.00]hello"}});check(l.size()==1&&l[0].text=="你好"&&l[0].words.size()==2&&l[0].translation=="hello");
 check(parse("[00:01.5]x",true)[0].start==1005);check(parse("[00:01.5]x")[0].start==1500);
 check(parse("[00:00]"+std::string(32768,'x')).size()==1);
 auto fallback=lyrics(nlohmann::json{{"lxlyric","invalid"},{"lyric","[00:00]fallback"}});check(fallback.size()==1);
 auto s=status(nlohmann::json{{"status","playing"},{"name","song"},{"singer","artist"},{"duration",20},{"progress",2},{"playbackRate",2}},100);check(s.at(600)==3000&&s.at(3200)==-1);s.playing=false;check(s.at(600)==2000);s.position=18000;s.playing=true;check(s.at(1600)==20000);
 bool rejected=false;try{status(nlohmann::json{{"status","playing"},{"name","song"},{"duration",20},{"progress",-1}},100);}catch(...){rejected=true;}check(rejected);
 std::cout<<"Passed: animation/Unicode/reveal/500ms offset, Present target routing (additional/primary/fallback/missing), LRC ordering/offset/repeated tags, LX words/translation/fallback, clock pause/rate/staleness/clamp, invalid status\n";
}
