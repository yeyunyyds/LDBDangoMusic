#pragma once
#include "world_camera.hpp"
namespace dango::wall {
inline float dot(Vec3 a,Vec3 b){return a.x*b.x+a.y*b.y+a.z*b.z;}
inline Vec3 cross(Vec3 a,Vec3 b){return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
inline bool finite(Vec3 p){return std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z);}
struct Panel {Vec3 center,normal,right,up;float width=0,height=0;uint32_t epoch=0;};
// Source uses Z-up. Plane axes are fixed at placement, never rebuilt from the
// current camera. Trace hits/normals must be supplied by a verified backend.
inline bool makePanel(Vec3 hit,Vec3 normal,float width,float height,uint32_t epoch,Panel& out,float offset=.75f){
 if(!finite(hit)||!finite(normal)||!std::isfinite(width)||!std::isfinite(height)||!std::isfinite(offset)||width<=0||height<=0||offset<0||length(normal)<.9f)return false;
 normal=normalize(normal);Vec3 worldUp{0,0,1};if(std::abs(dot(normal,worldUp))>.5f)return false;
 auto right=normalize(cross(worldUp,normal));auto up=normalize(cross(normal,right));
 out={hit+normal*offset,normal,right,up,width,height,epoch};return true;
}
inline Vec3 point(const Panel& p,float u,float v){return p.center+p.right*((u-.5f)*p.width)+p.up*((.5f-v)*p.height);}
struct Vertex {float x=0,y=0,z=0,rhw=1,u=0,v=0;};
inline bool projectVertex(const Matrix4& m,Vec3 p,float width,float height,float u,float v,Vertex& out){
 if(!finite(p)||!std::isfinite(width)||!std::isfinite(height)||width<=0||height<=0)return false;
 double x=m[0]*p.x+m[1]*p.y+m[2]*p.z+m[3],y=m[4]*p.x+m[5]*p.y+m[6]*p.z+m[7];
 double z=m[8]*p.x+m[9]*p.y+m[10]*p.z+m[11],w=m[12]*p.x+m[13]*p.y+m[14]*p.z+m[15];
 if(!std::isfinite(x)||!std::isfinite(y)||!std::isfinite(z)||!std::isfinite(w)||w<=.001||z<0||z>w)return false;
 out={float((x/w+1)*width*.5),float((1-y/w)*height*.5),float(z/w),float(1/w),u,v};
 return std::isfinite(out.x)&&std::isfinite(out.y)&&std::isfinite(out.rhw);
}
// Projection-only prototype: near-plane crossings are rejected as a whole.
// No scene occlusion or wall-extent validation is implied by this function.
inline bool projectPanel(const Panel& p,const GameCameraFrame& camera,float width,float height,std::array<Vertex,4>& vertices){
 if(!camera.active||p.epoch!=camera.epoch||!std::isfinite(p.width)||!std::isfinite(p.height)||p.width<=0||p.height<=0)return false;CameraPose pose;if(!cameraPose(camera,pose)||dot(p.normal,pose.eye-p.center)<=.001f)return false;
 const float uv[][2]={{0,0},{1,0},{0,1},{1,1}};
 for(size_t i=0;i<4;++i)if(!projectVertex(camera.worldToClip,point(p,uv[i][0],uv[i][1]),width,height,uv[i][0],uv[i][1],vertices[i]))return false;
 return true;
}
}
