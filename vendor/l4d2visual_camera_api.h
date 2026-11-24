#ifndef L4D2VISUAL_CAMERA_API_H
#define L4D2VISUAL_CAMERA_API_H

/* Public x86 C ABI for optional camera-control clients such as DangoMusic.
 * This header contains no Source/L4N internals and can be included from C or C++.
 * Resolve exports with GetModuleHandleW/GetProcAddress; never LoadLibrary the
 * L4N plugin just to acquire a camera.
 */
#include <stdint.h>
#ifdef _WIN32
#define L4D2VISUAL_CAMERA_CALL __cdecl
#else
#define L4D2VISUAL_CAMERA_CALL
#endif

#define L4D2VISUAL_CAMERA_ABI_VERSION 4u
#define L4D2VISUAL_CAMERA_OWNER_NATIVE 0u
#define L4D2VISUAL_CAMERA_OWNER_USER 1u
#define L4D2VISUAL_CAMERA_OWNER_EXTERNAL 2u

typedef struct L4D2VisualCameraPoseV1 {
    uint32_t size;             /* sizeof(L4D2VisualCameraPoseV1) */
    float origin[3];           /* absolute Source world coordinates */
    float angles[3];           /* Source pitch, yaw, roll in degrees */
} L4D2VisualCameraPoseV1;

typedef struct L4D2VisualCameraStatusV1 {
    uint32_t size;             /* sizeof(L4D2VisualCameraStatusV1) */
    uint32_t abi_version;
    uint32_t external_allowed; /* user consent in L4D2Visual's menu */
    uint32_t external_requested; /* controller has asked for ownership */
    uint32_t owner;            /* NATIVE, USER, EXTERNAL */
    uint32_t idle_seconds;     /* user-configured 1-30s, default 3 */
    uint32_t idle_remaining_ms;/* zero if eligible for external ownership */
    uint32_t hook_ready;       /* verified ClientMode hooks installed */
    uint32_t camera_active;    /* camera is actually overriding the view */
} L4D2VisualCameraStatusV1;

/* Version 4: Request(1) arms AFK; foreground polling after the initial idle interval turns on the shared camera, including spectator mode.
 * User OFF returns to native view and restarts the initial idle interval; it does not cancel the provider session. SetPose while USER owns the camera renews
 * the lease but discards coordinates. On handoff, GetPose returns the actual
 * detached pose, not the last submitted target. It fails if no active view.
 * SetExternalActive(0) releases an existing session; a stale release cannot
 * close a separate manually enabled camera. All calls return 1/0 except version.
 */
typedef uint32_t (L4D2VISUAL_CAMERA_CALL *L4D2VisualCameraGetVersionFn)(void);
typedef int (L4D2VISUAL_CAMERA_CALL *L4D2VisualCameraSetExternalActiveFn)(int active);
typedef int (L4D2VISUAL_CAMERA_CALL *L4D2VisualCameraSetInitialIdleFn)(uint32_t seconds);
/* Optional v4 extension: default native view on genuine user takeover. */
typedef int (L4D2VISUAL_CAMERA_CALL *L4D2VisualCameraSetNativeOnInputFn)(int enabled);
typedef int (L4D2VISUAL_CAMERA_CALL *L4D2VisualCameraSetPoseFn)(const L4D2VisualCameraPoseV1* pose);
typedef int (L4D2VISUAL_CAMERA_CALL *L4D2VisualCameraGetPoseFn)(L4D2VisualCameraPoseV1* out);
typedef int (L4D2VISUAL_CAMERA_CALL *L4D2VisualCameraGetStatusFn)(L4D2VisualCameraStatusV1* out);

#ifdef __cplusplus
extern "C" {
#endif
uint32_t L4D2VISUAL_CAMERA_CALL L4D2VisualCameraGetVersion(void);
int L4D2VISUAL_CAMERA_CALL L4D2VisualCameraSetExternalActive(int active);
int L4D2VISUAL_CAMERA_CALL L4D2VisualCameraSetInitialIdle(uint32_t seconds);
int L4D2VISUAL_CAMERA_CALL L4D2VisualCameraSetNativeOnInput(int enabled);
int L4D2VISUAL_CAMERA_CALL L4D2VisualCameraSetPose(const L4D2VisualCameraPoseV1* pose);
int L4D2VISUAL_CAMERA_CALL L4D2VisualCameraGetPose(L4D2VisualCameraPoseV1* out);
int L4D2VISUAL_CAMERA_CALL L4D2VisualCameraGetStatus(L4D2VisualCameraStatusV1* out);
#ifdef __cplusplus
}
#endif
#endif /* L4D2VISUAL_CAMERA_API_H */
