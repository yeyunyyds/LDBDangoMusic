#include "scene_attachment.hpp"
#include <iostream>
#include <stdexcept>
using namespace dango;
namespace api=dango::scene;
void check(bool v,const char* message){if(!v)throw std::runtime_error(message);}
bool near(Vec3 a,Vec3 b){return length(a-b)<.001f;}
api::Transform pose(Vec3 p){auto t=api::identity();t[3]=p.x;t[7]=p.y;t[11]=p.z;return t;}
api::Object object(uint64_t handle,const char* name,Vec3 p={}){api::Object o;o.id={7,api::ObjectKind::NetworkEntity,handle};o.className=name;o.world=pose(p);return o;}
int main(){
 api::Frame f;f.epoch=7;f.tick=1000;f.capabilities=api::Entities|api::StaticProps|api::WorldSurfaces|api::Bones|api::Attachments|api::ViewModels;
 auto local=object(0x20001,"CTerrorPlayer");local.team=2;
 auto teammate=object(0x30002,"CTerrorPlayer",{100,20,0});teammate.team=2;teammate.sockets.push_back({api::SocketKind::Bone,"head",pose({100,20,70})});
 auto projectile=object(0x40003,"CPipeBombProjectile",{10,0,0});
 auto gnome=object(0x50004,"CPhysicsProp",{15,0,0});gnome.modelName="models/props_junk/gnome.mdl";gnome.links.push_back({api::Relation::Owner,local.id});
 auto viewmodel=object(0x60005,"CBaseViewModel");viewmodel.domain=api::Domain::ViewModel;viewmodel.sockets.push_back({api::SocketKind::Attachment,"grip",pose({5,2,1})});
 auto car=object(17,"static_prop",{1000,100,0});car.id.kind=api::ObjectKind::StaticProp;car.modelName="models/props_vehicles/car_example.mdl";
 local.links={{api::Relation::ActiveWeapon,gnome.id},{api::Relation::ViewModel,viewmodel.id}};
 f.localPlayer=local.id;f.objects={local,teammate,projectile,gnome,viewmodel,car};
 api::Exchange exchange;check(exchange.publish(f),"publish");api::View view(exchange.snapshot(),1000);
 api::Filter filter;filter.className="CPipeBombProjectile";auto ids=view.enumerate(filter);check(ids.size()==1&&ids[0]==projectile.id,"projectile enumeration");
 filter={};filter.modelContains="gnome.mdl";check(view.enumerate(filter).size()==1,"model-based carryable query");
 check(view.related(local.id,api::Relation::ActiveWeapon)==gnome.id,"active held item relation");check(view.related(local.id,api::Relation::ViewModel)==viewmodel.id,"first-person viewmodel relation");
 api::Anchor text;text.target=projectile.id;text.local=pose({0,0,8});check(near(api::point(view.resolve(text).world,{}),{10,0,8}),"projectile offset");
 api::Anchor head;head.target=teammate.id;head.kind=api::AnchorKind::Bone;head.socket="head";head.local=pose({0,0,12});check(near(api::point(view.resolve(head).world,{}),{100,20,82}),"teammate head bone");
 auto missing=head;missing.socket="not-present";check(view.resolve(missing).status==api::ResolveStatus::MissingSocket,"no silent bone fallback");
 api::Anchor held;held.target=viewmodel.id;held.kind=api::AnchorKind::Attachment;held.socket="grip";auto heldPose=view.resolve(held);check(heldPose.status==api::ResolveStatus::Ready&&heldPose.domain==api::Domain::ViewModel,"separate viewmodel projection domain");
 auto window=view.bindSurface(car.id,{1010,100,50},{0,-1,0});check(window.has_value(),"bind vehicle window local plane");check(near(api::point(view.resolve(*window).world,{}),{1010,100,50}),"initial surface position");
 // Vehicle moves and rotates 90 degrees: the entire local surface frame follows.
 auto moved=f;moved.tick=1016;moved.objects[5].world={0,-1,0,1200,1,0,0,200,0,0,1,0};moved.objects[2].world=pose({30,5,0});check(exchange.publish(moved),"moving snapshot");api::View moving(exchange.snapshot(),1016);
 check(near(api::point(moving.resolve(*window).world,{}),{1200,210,50}),"vehicle surface position follows rotation");
 auto plane=moving.resolve(*window).world;check(std::abs(plane[0])<.001&&std::abs(plane[4]-1)<.001,"vehicle plane axes follow rotation");
 check(near(api::point(moving.resolve(text).world,{}),{30,5,8}),"projectile moves each sample");check(near(api::point(view.resolve(text).world,{}),{10,0,8}),"old snapshot immutable");
 check(!view.bindSurface(car.id,{},{0,0,0}),"reject invalid surface normal");
 auto deleted=moved;deleted.tick=1032;deleted.objects.erase(deleted.objects.begin()+2);check(exchange.publish(deleted),"deletion");check(api::View(exchange.snapshot(),1032).resolve(text).status==api::ResolveStatus::TargetGone,"destroyed projectile detaches");
 auto recycled=projectile;recycled.id.identity=0x90003;deleted.objects.push_back(recycled);deleted.tick=1048;check(exchange.publish(deleted),"recycled index");check(api::View(exchange.snapshot(),1048).resolve(text).status==api::ResolveStatus::TargetGone,"full handle serial prevents retargeting");
 auto dormant=moved;dormant.objects[2].dormant=true;check(api::View(std::make_shared<const api::Frame>(dormant),1016).resolve(text).status==api::ResolveStatus::Dormant,"dormant targets not tracked using old pose");
 auto unsupported=f;unsupported.capabilities=api::Entities;auto limited=api::View(std::make_shared<const api::Frame>(unsupported),1000);check(limited.resolve(head).status==api::ResolveStatus::Unsupported&&limited.resolve(held).status==api::ResolveStatus::Unsupported,"capability negotiation");
 check(api::View(exchange.snapshot(),2000).resolve(text).status==api::ResolveStatus::Stale,"stale producer lease");
 auto nextMap=f;nextMap.epoch=8;nextMap.localPlayer.reset();for(auto& o:nextMap.objects){o.id.epoch=8;o.links.clear();}check(exchange.publish(nextMap),"map epoch");check(api::View(exchange.snapshot(),1000).resolve(text).status==api::ResolveStatus::WrongMap,"map identity guards all targets");
 auto invalid=nextMap;invalid.objects.push_back(invalid.objects[0]);check(!exchange.publish(invalid),"duplicate object IDs rejected");invalid=nextMap;invalid.objects[0].world={};check(!exchange.publish(invalid),"singular transforms rejected");check(!exchange.publish(f),"old map snapshots rejected");
 std::cout<<"Passed: projectile/head/held-item/gnome queries, vehicle local-plane motion+rotation, snapshot lifetime, deletion/serial/dormancy/epoch guards and capability failures\n";
}
