#pragma once
// Optional VisualEffects FreeCamera v3 client; no new Source/ClientMode hooks.
// GPL-3.0: shared ABI contract from yeyunyyds/L4D2-Visual-Effects.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "../vendor/l4d2visual_camera_api.h"
#include <cstdint>

namespace dango::visualcamera {

struct Api {
 L4D2VisualCameraSetExternalActiveFn setActive=nullptr;
 L4D2VisualCameraSetPoseFn setPose=nullptr;
 L4D2VisualCameraGetStatusFn getStatus=nullptr;
 L4D2VisualCameraGetPoseFn getPose=nullptr;
 L4D2VisualCameraSetInitialIdleFn setInitialIdle=nullptr;

 explicit operator bool() const noexcept {return setActive&&setPose&&getStatus&&getPose&&setInitialIdle;}
};

// Resolve each time; do not retain unowned pointers across plugin reloads.
// Do not call LoadLibrary here: L4N must own L4D2Visual's module lifecycle.
inline Api resolve() noexcept {
 HMODULE module=GetModuleHandleW(L"L4D2Visual.dll");
 if(!module)return {};
 const auto version=reinterpret_cast<L4D2VisualCameraGetVersionFn>(
  GetProcAddress(module,"L4D2VisualCameraGetVersion"));
 if(!version||version()!=L4D2VISUAL_CAMERA_ABI_VERSION)return {};
 Api api;
 api.setActive=reinterpret_cast<L4D2VisualCameraSetExternalActiveFn>(
  GetProcAddress(module,"L4D2VisualCameraSetExternalActive"));
 api.setPose=reinterpret_cast<L4D2VisualCameraSetPoseFn>(
  GetProcAddress(module,"L4D2VisualCameraSetPose"));
 api.getStatus=reinterpret_cast<L4D2VisualCameraGetStatusFn>(
  GetProcAddress(module,"L4D2VisualCameraGetStatus"));
 api.getPose=reinterpret_cast<L4D2VisualCameraGetPoseFn>(
  GetProcAddress(module,"L4D2VisualCameraGetPose"));
 api.setInitialIdle=reinterpret_cast<L4D2VisualCameraSetInitialIdleFn>(GetProcAddress(module,"L4D2VisualCameraSetInitialIdle"));
 return api;
}
inline bool status(L4D2VisualCameraStatusV1& result) noexcept {
 const auto api=resolve();
 if(!api)return false;
 result={};result.size=sizeof(result);
 return api.getStatus(&result)!=0&&result.abi_version==L4D2VISUAL_CAMERA_ABI_VERSION;
}
inline bool getPose(L4D2VisualCameraPoseV1& result) noexcept {
 const auto api=resolve();if(!api)return false;
 result={};result.size=sizeof(result);return api.getPose(&result)!=0;
}
inline bool request(bool on) noexcept {
 const auto api=resolve();
 return api&&api.setActive(on?1:0)!=0;
}
inline bool pose(float x,float y,float z,float pitch,float yaw,float roll=0.f) noexcept {
 const auto api=resolve();
 if(!api)return false;
 L4D2VisualCameraPoseV1 target{};
 target.size=sizeof(target);
 target.origin[0]=x;target.origin[1]=y;target.origin[2]=z;
 target.angles[0]=pitch;target.angles[1]=yaw;target.angles[2]=roll;
 return api.setPose(&target)!=0;
}
// Do not infer ownership from request()/pose() success: the user can always
// override the camera; status.owner is authoritative.
inline bool externallyControlling() noexcept {
 L4D2VisualCameraStatusV1 s{};
 return status(s)&&s.owner==L4D2VISUAL_CAMERA_OWNER_EXTERNAL&&s.camera_active;
}
} // namespace dango::visualcamera
