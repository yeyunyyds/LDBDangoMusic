#pragma once
#include <array>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstddef>
namespace dango {
struct Vec3 {float x=0,y=0,z=0;};
inline Vec3 operator+(Vec3 a,Vec3 b){return {a.x+b.x,a.y+b.y,a.z+b.z};}
inline Vec3 operator-(Vec3 a,Vec3 b){return {a.x-b.x,a.y-b.y,a.z-b.z};}
inline Vec3 operator*(Vec3 a,float s){return {a.x*s,a.y*s,a.z*s};}
inline float length(Vec3 v){return std::sqrt(v.x*v.x+v.y*v.y+v.z*v.z);}
inline Vec3 normalize(Vec3 v){float n=length(v);return n>1e-6f?v*(1/n):Vec3{};}
using Matrix4=std::array<float,16>;
// Process-local Source engine snapshot (no shared memory or cross-process ABI).
struct GameCameraFrame {uint32_t epoch=0,active=0,width=0,height=0;uint64_t tick=0;Matrix4 worldToView{},worldToClip{};};
inline bool inverse(const Matrix4& input,Matrix4& output){
 double rows[4][8]{};for(int r=0;r<4;++r)for(int c=0;c<4;++c){if(!std::isfinite(input[r*4+c]))return false;rows[r][c]=input[r*4+c];rows[r][c+4]=r==c;}
 for(int c=0;c<4;++c){int pivot=c;for(int r=c+1;r<4;++r)if(std::abs(rows[r][c])>std::abs(rows[pivot][c]))pivot=r;if(std::abs(rows[pivot][c])<1e-10)return false;for(int k=0;k<8;++k)std::swap(rows[pivot][k],rows[c][k]);double d=rows[c][c];for(int k=0;k<8;++k)rows[c][k]/=d;for(int r=0;r<4;++r)if(r!=c){d=rows[r][c];for(int k=0;k<8;++k)rows[r][k]-=d*rows[c][k];}}
 for(int r=0;r<4;++r)for(int c=0;c<4;++c){output[r*4+c]=float(rows[r][c+4]);if(!std::isfinite(output[r*4+c]))return false;}return true;
}
inline bool transform(const Matrix4& m,Vec3 input,Vec3& out,bool positiveW=false){
 double w=m[12]*input.x+m[13]*input.y+m[14]*input.z+m[15];if(!std::isfinite(w)||std::abs(w)<1e-6||(positiveW&&w<=.001))return false;
 out={float((m[0]*input.x+m[1]*input.y+m[2]*input.z+m[3])/w),float((m[4]*input.x+m[5]*input.y+m[6]*input.z+m[7])/w),float((m[8]*input.x+m[9]*input.y+m[10]*input.z+m[11])/w)};
 return std::isfinite(out.x)&&std::isfinite(out.y)&&std::isfinite(out.z);
}
struct CameraPose {Vec3 eye,forward,right,up;};
inline bool cameraPose(const GameCameraFrame& frame,CameraPose& pose){
 Matrix4 viewInverse{},clipInverse{};
 if(!inverse(frame.worldToView,viewInverse)||!inverse(frame.worldToClip,clipInverse))return false;
 // WorldToView is affine in Source's column-vector VMatrix convention.
 auto& v=frame.worldToView;
 if(std::abs(v[12])+std::abs(v[13])+std::abs(v[14])+std::abs(v[15]-1)>.001)return false;
 Vec3 center,right,up;if(!transform(viewInverse,{},pose.eye)||!transform(clipInverse,{0,0,.5f},center)||!transform(clipInverse,{.1f,0,.5f},right)||!transform(clipInverse,{0,.1f,.5f},up))return false;
 pose.forward=normalize(center-pose.eye);pose.right=normalize(right-center);pose.up=normalize(up-center);
 if(length(pose.forward)<.9f||length(pose.right)<.9f||length(pose.up)<.9f)return false;
 // Reject identity/orthographic and mismatched view/view-projection pairs.
 float dot=pose.forward.x*pose.right.x+pose.forward.y*pose.right.y+pose.forward.z*pose.right.z;
 auto& m=frame.worldToClip;float plane=length({m[12],m[13],m[14]});float eyeW=m[12]*pose.eye.x+m[13]*pose.eye.y+m[14]*pose.eye.z+m[15];if(plane<1e-5f||!std::isfinite(eyeW)||std::abs(eyeW)/plane>.1f)return false;
 return std::abs(dot)<.05f;
}
inline bool usableCamera(const GameCameraFrame& f,uint64_t tick){
 if(f.active!=1||f.width<64||f.height<64||f.width>32768||f.height>32768||tick<f.tick||tick-f.tick>500)return false;
 CameraPose pose;return cameraPose(f,pose);
}
struct Placement {float x=0,y=0,scale=1;};
inline Vec3 anchorAt(const CameraPose& pose,float distance,float side,float height){return pose.eye+pose.forward*distance+pose.right*side+pose.up*height;}
inline bool projectAnchor(const GameCameraFrame& frame,Vec3 anchor,float width,float height,float worldHeight,int fontSize,Placement& placement){
 Vec3 ndc{},top{};CameraPose pose;if(!cameraPose(frame,pose)||!transform(frame.worldToClip,anchor,ndc,true)||ndc.x< -1.1f||ndc.x>1.1f||ndc.y< -1.1f||ndc.y>1.1f||ndc.z<0||ndc.z>1)return false;
 if(!transform(frame.worldToClip,anchor+pose.up*worldHeight,top,true))return false;
 placement={(ndc.x+1)*width*.5f,(1-ndc.y)*height*.5f,std::clamp(std::abs(top.y-ndc.y)*height*.5f/std::max(fontSize,1),.08f,3.f)};return true;
}
}
