#include "wall_scene.hpp"
#include <stdexcept>
#include <iostream>
#include <functional>
using namespace dango;
void check(bool v,const char* m){if(!v)throw std::runtime_error(m);}
GameCameraFrame camera(Vec3 eye={},float yaw=0){GameCameraFrame f;f.active=1;f.epoch=7;f.width=1280;f.height=720;
 Vec3 forward{std::cos(yaw),std::sin(yaw),0},right{std::sin(yaw),-std::cos(yaw),0},up{0,0,1};float r=-wall::dot(right,eye),u=-wall::dot(up,eye),d=-wall::dot(forward,eye);
 f.worldToView={right.x,right.y,right.z,r,up.x,up.y,up.z,u,forward.x,forward.y,forward.z,d,0,0,0,1};
 f.worldToClip={right.x,right.y,right.z,r,up.x,up.y,up.z,u,forward.x*1.0001f,forward.y*1.0001f,0,d*1.0001f-1.0001f,forward.x,forward.y,0,d};return f;}
struct Corridor {
 mutable unsigned calls=0;bool hole=false,failure=false;
 bool operator()(Vec3 start,Vec3 end,trace::Result& r)const{
 ++calls;if(failure)return false;r={};r.start=start;r.end=end;auto delta=end-start;
 const Vec3 normals[]={{-1,0,0},{0,-1,0},{0,1,0}};const float ds[]={-600,-150,-150};
 for(int i=0;i<3;++i){float denom=wall::dot(normals[i],delta);if(std::abs(denom)<.0001)continue;float t=(ds[i]-wall::dot(normals[i],start))/denom;
 if(t<0||t>=r.fraction)continue;auto point=start+delta*t;if(hole&&i>0&&point.x>180&&point.x<260)continue;
 r.fraction=t;r.end=point;r.plane.normal=normals[i];r.plane.distance=ds[i];r.contents=1;}
 return true;
 }
};
int main(){
 wallscene::Options options;options.size=0;options.spacing=80;auto frame=camera();CameraPose pose;check(cameraPose(frame,pose),"camera");
 Corridor world;wall::Panel left,right;
 check(wallscene::candidate(frame,pose,0,1,options,{},std::ref(world),left),"left search");check(world.calls<=15,"bounded search");
 world.calls=0;check(wallscene::candidate(frame,pose,0,2,options,{},std::ref(world),right),"right search");check(left.center.y*right.center.y<0,"alternate corridor sides");
 check(std::abs(left.normal.y)>.99f&&std::abs(right.normal.y)>.99f,"wall normals");
 wallscene::Scene scene;wallscene::Ticket ticket{1,1,1000,7,true};scene.observe(ticket,1000,7,options);check(scene.pending(ticket),"first pending");
 scene.add(left,true,ticket,1000,options);check(scene.items.size()==1&&scene.items[0].token==1&&!scene.pending(ticket),"assign exactly once");
 check(!wallscene::separated(left,scene.items,80),"duplicate rectangle");check(wallscene::separated(right,scene.items,80),"other wall");
 ticket.serial=2;ticket.tick=2000;scene.observe(ticket,2000,7,options);check(scene.items[0].retired==2000,"retire at next sentence");scene.add(right,true,ticket,2000,options);
 check(scene.items[0].token==1&&scene.items[1].token==2,"distinct lyric payloads");check(wallscene::alpha(scene.items[0],6000,options)==1,"extra hold");check(std::abs(wallscene::alpha(scene.items[0],6500,options)-.5f)<.001,"fade");
 ticket.tick=7000;scene.observe(ticket,7000,7,options);check(scene.items.size()==1&&scene.items[0].token==2,"expiry no resurrection");
 // Pause and stale requests clear history; old tickets cannot cross a map epoch.
 ticket.active=false;scene.observe(ticket,7001,7,options);check(scene.items.empty(),"pause clears");ticket.active=true;ticket.tick=8000;scene.observe(ticket,8000,7,options);scene.add(left,true,ticket,8000,options);scene.observe(ticket,9001,7,options);check(scene.items.empty(),"stale producer clears");
 scene.observe(ticket,8000,8,options);check(!scene.pending(ticket),"map epoch");
 // Manual anchors survive playback resets; content is rotated and cleared.
 scene.reset();ticket={2,10,10000,7,true};scene.observe(ticket,10000,7,options);scene.add(left,false,ticket,10000,options);scene.add(right,false,ticket,10000,options);
 ticket.serial=11;ticket.tick=10050;scene.observe(ticket,10050,7,options);check(scene.assignManual(ticket,10050)&&scene.items[1].token==11&&scene.items[0].token==10,"manual rotate onto next panel");check(!scene.assignManual(ticket,10050),"no duplicate assign");
 ticket.session=3;ticket.tick=10100;scene.observe(ticket,10100,7,options);check(scene.items.size()==2&&scene.items[0].token==0&&scene.items[1].token==0,"session clears manual payloads");
 scene.clear(ticket);scene.observe(ticket,10100,7,options);check(!scene.pending(ticket),"clear waits next sentence");
 options.capacity=2;for(int i=0;i<10;++i){ticket.serial++;scene.observe(ticket,10100,7,options);scene.add(left,true,ticket,10100,options);check(scene.items.size()<=2,"bounded panel count");}
 // Sentence display routes are sticky: leaving view never changes an
 // already assigned wall sentence, even well past the fallback timeout.
 wallscene::SentenceRoute route;route.choose(true,1500,1000,true);route.choose(false,9000,1000,true);check(route.attached&&!route.fallback,"assigned sentence stays wall-bound");
 wallscene::SentenceRoute nextRoute;nextRoute.choose(false,1999,1000,true);check(!nextRoute.fallback,"wait for wall search");nextRoute.choose(false,2000,1000,true);check(nextRoute.fallback,"new sentence chooses world fallback");nextRoute.choose(true,2100,1000,true);check(nextRoute.fallback&&!nextRoute.attached,"late wall does not move current sentence");
 scene.reset();ticket={4,30,20000,7,true};scene.observe(ticket,20000,7,options);scene.add(left,true,ticket,20000,options);
 ticket.serial=31;ticket.tick=21000;scene.observe(ticket,21000,7,options);scene.add(right,true,ticket,21000,options); // late placement raced fallback
 ticket.place=false;scene.observe(ticket,21000,7,options);check(scene.items.size()==1&&scene.items[0].token==30&&scene.items[0].retired==21000,"fallback retains outgoing wall");check(!scene.pending(ticket),"fallback cancels search for current sentence");check(wallscene::alpha(scene.items[0],22000,options)==1,"outgoing wall holds naturally");
 // Turning a corner changes the selected wall normal, not a placed panel's axes.
 auto turnedFrame=camera({600,300,0},1.57079632679f);CameraPose turnedPose;check(cameraPose(turnedFrame,turnedPose),"turned camera");
 auto rotatedWorld=[&](Vec3 start,Vec3 end,trace::Result& hit){
 auto local=[](Vec3 p){auto d=p-Vec3{600,300,0};return Vec3{d.y,-d.x,d.z};};
 trace::Result result;if(!world(local(start),local(end),result))return false;hit=result;hit.start=start;
 hit.end=Vec3{600,300,0}+Vec3{-result.end.y,result.end.x,result.end.z};hit.plane.normal={-result.plane.normal.y,result.plane.normal.x,result.plane.normal.z};hit.plane.distance=wall::dot(hit.plane.normal,hit.end);return true;};
 wall::Panel afterTurn;check(wallscene::candidate(turnedFrame,turnedPose,0,3,options,{},rotatedWorld,afterTurn),"search around corridor turn");
 check(std::abs(afterTurn.normal.x)>.99f&&std::abs(left.normal.y)>.99f,"independent wall orientation after turn");
 // Search rejects a failed world, and a supported wall is still fixed after moving.
 Corridor failed;failed.failure=true;wall::Panel p;check(!wallscene::candidate(frame,pose,0,1,options,{},std::ref(failed),p),"failed trace");
 auto corners=wall::point(left,0,0);auto moved=camera({80,0,0});check(wallscene::onScreen(left,moved,1280,720),"parallax visible");check(length(wall::point(left,0,0)-corners)<.001,"fixed plane");
 auto away=camera({},3.14159265f);check(!wallscene::onScreen(left,away,1280,720),"offscreen rejection");
 // Reuse is opt-in, replaces content in creation order and never changes geometry.
 wallscene::Scene reuse;auto ro=options;ro.capacity=6;ro.reuse=false;ticket={9,1,30000,7,true};reuse.observe(ticket,30000,7,ro);reuse.add(left,true,ticket,30000,ro);
 ticket.serial=2;ticket.tick=30010;reuse.observe(ticket,30010,7,ro);reuse.add(right,true,ticket,30010,ro);
 ticket.serial=3;ticket.tick=30020;reuse.observe(ticket,30020,7,ro);for(auto& i:reuse.items)i.visibleTick=30020;
 auto firstPlane=reuse.items[0].panel;check(!reuse.reuseExisting(ticket,30020,frame,pose,ro),"reuse disabled by default");ro.reuse=true;
 check(reuse.reuseExisting(ticket,30020,frame,pose,ro)&&reuse.items[0].token==3&&reuse.items[1].token==2,"oldest panel first");
 check(length(reuse.items[0].panel.center-firstPlane.center)<.001f&&reuse.items[0].id==1,"reuse preserves plane and identity");
 check(!reuse.reuseExisting(ticket,30020,frame,pose,ro),"no duplicate assignment");
 ticket.serial=4;ticket.tick=30030;reuse.observe(ticket,30030,7,ro);check(reuse.reuseExisting(ticket,30030,frame,pose,ro)&&reuse.items[1].token==4,"reuse second panel");
 ticket.serial=5;ticket.tick=30040;reuse.observe(ticket,30040,7,ro);check(reuse.reuseExisting(ticket,30040,frame,pose,ro)&&reuse.items[0].token==5,"reuse wraps without old text resurrection");
 ticket.serial=6;ticket.tick=30050;reuse.observe(ticket,30050,7,ro);for(auto& i:reuse.items)i.visible=false;check(!reuse.reuseExisting(ticket,30050,frame,pose,ro),"occluded panels not reused");
 for(auto& i:reuse.items){i.visible=true;i.visibleTick=29000;}check(!reuse.reuseExisting(ticket,30050,frame,pose,ro),"stale visibility not reused");
 for(auto& i:reuse.items)i.visibleTick=30050;ro.range=1;check(!reuse.reuseExisting(ticket,30050,frame,pose,ro),"distant panels not reused");ro.range=600;
 check(!reuse.reuseExisting(ticket,30050,away,pose,ro),"offscreen panels not reused");ro.surface=1;check(!reuse.reuseExisting(ticket,30050,frame,pose,ro),"wall not reused as floor");ro.surface=0;
 ticket.place=false;reuse.observe(ticket,30050,7,ro);check(!reuse.reuseExisting(ticket,30050,frame,pose,ro),"sticky fallback blocks late reuse");
 // A complete five-direction search fails on occupied wall patches; reuse
 // must settle the route before the renderer's one-second fallback deadline.
 wallscene::Scene occupied;auto crowded=ro;crowded.spacing=10000;ticket={10,1,40000,7,true};occupied.observe(ticket,40000,7,crowded);occupied.add(left,true,ticket,40000,crowded);occupied.add(right,true,ticket,40000,crowded);
 ticket.serial=2;wallscene::SentenceRoute reusedRoute;
 for(unsigned attempt=0;attempt<5;++attempt){uint64_t tick=40010+attempt*200;ticket.tick=tick;occupied.observe(ticket,tick,7,crowded);for(auto& i:occupied.items)i.visibleTick=tick;wall::Panel unused;check(!wallscene::candidate(frame,pose,attempt,ticket.serial,crowded,occupied.items,std::ref(world),unused),"no unoccupied new patch");if(attempt==4)check(occupied.reuseExisting(ticket,tick,frame,pose,crowded),"reuse before timeout");}
 reusedRoute.choose(!occupied.pending(ticket),40810,40010,true);check(reusedRoute.attached&&!reusedRoute.fallback,"reused wall wins before fallback");
 // A delayed backend may still see a place=true ticket after the renderer's
 // fallback deadline; it must not overwrite retained old content in that race.
 ticket.serial=3;ticket.tick=42000;ticket.started=41000;occupied.observe(ticket,42000,7,crowded);for(auto& i:occupied.items)i.visibleTick=42000;
 auto keptToken=occupied.items.front().token;check(!occupied.reuseExisting(ticket,42000,frame,pose,crowded)&&occupied.items.front().token==keptToken,"deadline preserves old panel content");
 crowded.fallback=false;check(occupied.reuseExisting(ticket,42000,frame,pose,crowded),"no deadline when fallback disabled");
 // Floor search is explicit; the default wall search still rejects floor hits.
 auto ground=[](Vec3 start,Vec3 end,trace::Result& hit){hit={};hit.start=start;hit.end=end;auto d=end-start;if(std::abs(d.z)<.0001f)return true;float t=-start.z/d.z;if(t>=0&&t<hit.fraction){hit.fraction=t;hit.end=start+d*t;hit.plane.normal={0,0,1};hit.contents=1;}return true;};
 auto above=camera({0,0,80});CameraPose groundPose;check(cameraPose(above,groundPose),"floor camera");wall::Panel groundPanel;auto groundOptions=wallscene::Options{};
 check(!wallscene::candidate(above,groundPose,0,1,groundOptions,{},ground,groundPanel),"floor not enabled implicitly");groundOptions.surface=1;groundOptions.size=0;
 check(wallscene::candidate(above,groundPose,0,1,groundOptions,{},ground,groundPanel)&&groundPanel.normal.z>.99f,"automatic ground placement");
 std::cout<<"Passed: corridor side search/budget, distinct lyric assignment, hold/fade/expiry, pause/stale/map/session clear, manual rotation, capacity/spacing and fixed wall parallax\n";
}
