#pragma once
// Experimental value-only scene API. No Source pointers escape the producer.
// The Source entity/static-prop/bone acquisition backend is not implemented yet.
#include "world_camera.hpp"
#include <atomic>
#include <memory>
#include <optional>
#include <string>
#include <vector>
#include <set>
#include <tuple>
namespace dango::scene {
enum class ObjectKind {NetworkEntity,ClientOnly,StaticProp,World};
struct ObjectId {
 uint32_t epoch=0;ObjectKind kind=ObjectKind::NetworkEntity;uint64_t identity=0;
 // identity is the full network handle (entry AND serial), a map static-prop
 // index, or a producer-issued generation ID for a client-only object.
 bool operator==(const ObjectId& b)const{return epoch==b.epoch&&kind==b.kind&&identity==b.identity;}
};
using Transform=std::array<float,12>;
inline Transform identity(){return {1,0,0,0,0,1,0,0,0,0,1,0};}
inline bool finite(Vec3 v){return std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z);}
inline Vec3 point(const Transform& m,Vec3 p){return {m[0]*p.x+m[1]*p.y+m[2]*p.z+m[3],m[4]*p.x+m[5]*p.y+m[6]*p.z+m[7],m[8]*p.x+m[9]*p.y+m[10]*p.z+m[11]};}
inline Transform multiply(const Transform& a,const Transform& b){
 Transform out{};for(int r=0;r<3;++r){for(int c=0;c<3;++c)for(int k=0;k<3;++k)out[r*4+c]+=a[r*4+k]*b[k*4+c];out[r*4+3]=a[r*4+3];for(int k=0;k<3;++k)out[r*4+3]+=a[r*4+k]*b[k*4+3];}return out;
}
inline bool valid(const Transform& t){
 for(auto f:t)if(!std::isfinite(f))return false;
 double d=double(t[0])*(double(t[5])*t[10]-double(t[6])*t[9])-double(t[1])*(double(t[4])*t[10]-double(t[6])*t[8])+double(t[2])*(double(t[4])*t[9]-double(t[5])*t[8]);return std::isfinite(d)&&std::abs(d)>1e-8;
}
inline bool invert(const Transform& t,Transform& result){
 if(!valid(t))return false;Matrix4 m={t[0],t[1],t[2],t[3],t[4],t[5],t[6],t[7],t[8],t[9],t[10],t[11],0,0,0,1},out;
 if(!inverse(m,out))return false;std::copy_n(out.begin(),12,result.begin());return valid(result);
}
enum Capability : uint32_t {Entities=1,StaticProps=2,WorldSurfaces=4,Bones=8,Attachments=16,ViewModels=32};
enum class Domain {World,ViewModel};
enum class SocketKind {Bone,Attachment};
struct Socket {SocketKind kind;std::string name;Transform world=identity();};
enum class Relation {Owner,Parent,ActiveWeapon,ViewModel,CarriedObject};
struct Link {Relation relation;ObjectId target;};
struct Object {
 ObjectId id;std::string className,modelName;int team=0;bool dormant=false;
 Domain domain=Domain::World;Transform world=identity();std::vector<Socket> sockets;std::vector<Link> links;
};
struct Frame {
 uint32_t epoch=0,capabilities=0;uint64_t tick=0;std::vector<Object> objects;std::optional<ObjectId> localPlayer;
 std::string backendStatus="No Source scene backend connected";
};
struct Filter {
 std::optional<ObjectKind> kind;std::string className,modelContains;std::optional<int> team;
};
enum class AnchorKind {Origin,Bone,Attachment,Surface};
struct Anchor {ObjectId target;AnchorKind kind=AnchorKind::Origin;std::string socket;Transform local=identity();};
enum class ResolveStatus {Ready,Stale,WrongMap,TargetGone,Dormant,Unsupported,MissingSocket,InvalidTransform};
struct Resolved {ResolveStatus status=ResolveStatus::TargetGone;Transform world=identity();Domain domain=Domain::World;};
class View {
 std::shared_ptr<const Frame> frame;uint64_t now,maxAge;
 bool fresh()const{return frame&&frame->epoch&&now>=frame->tick&&now-frame->tick<=maxAge;}
 uint32_t capability(const Object& o)const{return o.id.kind==ObjectKind::StaticProp?StaticProps:o.id.kind==ObjectKind::World?WorldSurfaces:Entities;}
public:
 View(std::shared_ptr<const Frame> f,uint64_t tick,uint64_t age=250):frame(std::move(f)),now(tick),maxAge(age){}
 const Object* find(ObjectId id)const{if(!fresh()||id.epoch!=frame->epoch)return nullptr;for(const auto& o:frame->objects)if(o.id==id)return &o;return nullptr;}
 std::vector<ObjectId> enumerate(const Filter& f={})const{
 std::vector<ObjectId> result;if(!fresh())return result;for(const auto& o:frame->objects){if(o.dormant||!(frame->capabilities&capability(o)))continue;
 if(f.kind&&o.id.kind!=*f.kind)continue;if(f.team&&o.team!=*f.team)continue;if(!f.className.empty()&&o.className!=f.className)continue;if(!f.modelContains.empty()&&o.modelName.find(f.modelContains)==std::string::npos)continue;result.push_back(o.id);}return result;
 }
 std::optional<ObjectId> related(ObjectId id,Relation relation)const{
 auto o=find(id);if(!o||o->dormant||!(frame->capabilities&capability(*o)))return {};for(const auto& link:o->links)if(link.relation==relation){auto target=find(link.target);if(target&&!target->dormant&&(frame->capabilities&capability(*target)))return target->id;}return {};
 }
 Resolved resolve(const Anchor& a)const{
 Resolved result;if(!fresh()){result.status=ResolveStatus::Stale;return result;}if(a.target.epoch!=frame->epoch){result.status=ResolveStatus::WrongMap;return result;}
 auto o=find(a.target);if(!o)return result;result.domain=o->domain;
 if(o->dormant){result.status=ResolveStatus::Dormant;return result;}
 if(!(frame->capabilities&capability(*o))||(o->domain==Domain::ViewModel&&!(frame->capabilities&ViewModels))){result.status=ResolveStatus::Unsupported;return result;}
 Transform parent=o->world;
 if(a.kind==AnchorKind::Bone||a.kind==AnchorKind::Attachment){auto flag=a.kind==AnchorKind::Bone?Bones:Attachments;auto kind=a.kind==AnchorKind::Bone?SocketKind::Bone:SocketKind::Attachment;
 if(!(frame->capabilities&flag)){result.status=ResolveStatus::Unsupported;return result;}
 auto socket=std::find_if(o->sockets.begin(),o->sockets.end(),[&](const Socket& s){return s.kind==kind&&s.name==a.socket;});
 if(socket==o->sockets.end()){result.status=ResolveStatus::MissingSocket;return result;}parent=socket->world;}
 if(!valid(parent)||!valid(a.local)){result.status=ResolveStatus::InvalidTransform;return result;}
 result.world=multiply(parent,a.local);result.status=valid(result.world)?ResolveStatus::Ready:ResolveStatus::InvalidTransform;return result;
 }
 // A trace backend supplies the hit OWNER and world point/normal. Freeze that
 // surface in owner-local space so moving/rotating props carry it correctly.
 std::optional<Anchor> bindSurface(ObjectId owner,Vec3 hit,Vec3 normal)const{
 auto o=find(owner);if(!o||o->dormant||!(frame->capabilities&capability(*o))||!finite(hit)||!finite(normal)||length(normal)<.9f)return {};
 Transform inv;if(!invert(o->world,inv))return {};normal=normalize(normal);
 Vec3 reference=std::abs(normal.z)>.9f?Vec3{1,0,0}:Vec3{0,0,1};
 auto cross=[](Vec3 a,Vec3 b){return Vec3{a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};};
 auto right=normalize(cross(reference,normal)),up=cross(normal,right);
 Transform surface={right.x,up.x,normal.x,hit.x,right.y,up.y,normal.y,hit.y,right.z,up.z,normal.z,hit.z};
 Anchor a;a.target=owner;a.kind=AnchorKind::Surface;a.local=multiply(inv,surface);return valid(a.local)?std::optional<Anchor>(a):std::nullopt;
 }
};
// publish() belongs to a future validated Source game-thread producer. Readers
// retain immutable snapshots; object deletion and full-handle reuse are explicit.
class Exchange {
 std::shared_ptr<const Frame> frame=std::make_shared<Frame>();
public:
 std::shared_ptr<const Frame> snapshot()const{return std::atomic_load(&frame);}
 bool publish(Frame next){
 if(!next.epoch||next.objects.size()>8192)return false;
 std::set<std::tuple<uint32_t,int,uint64_t>> ids;
 for(size_t i=0;i<next.objects.size();++i){const auto& o=next.objects[i];if(o.id.epoch!=next.epoch||!valid(o.world))return false;
 if(!ids.emplace(o.id.epoch,int(o.id.kind),o.id.identity).second)return false;
 for(const auto& socket:o.sockets)if(socket.name.empty()||!valid(socket.world))return false;
 for(const auto& link:o.links)if(link.target.epoch!=next.epoch)return false;}
 if(next.localPlayer&&(next.localPlayer->epoch!=next.epoch||std::none_of(next.objects.begin(),next.objects.end(),[&](const Object& o){return o.id==*next.localPlayer;})))return false;
 auto previous=snapshot();if(previous->epoch&&(next.epoch<previous->epoch||(next.epoch==previous->epoch&&next.tick<previous->tick)))return false;
 std::atomic_store(&frame,std::make_shared<const Frame>(std::move(next)));return true;
 }
};
}
