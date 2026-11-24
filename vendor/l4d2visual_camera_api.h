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

#define L4D2VISUAL_CAMERA_ABI_VERSION 1u
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

/* Returns 1 for this ABI, 0 otherwise. All other calls return 1 on
 * acceptance and 0 on refusal (permission off, invalid input, shutdown).
 *
 * SetExternalActive(1) requests control, but NEVER overrides active user
 * input. It has no effect until the user allows External API in the menu
 * and SetPose has supplied a valid pose. Keep sending poses while active:
 * a controller silent for 15s expires safely to native view.
 * SetExternalActive(0) explicitly stops the external session.
 */
typedef uint32_t (L4D2VISUAL_CAMERA_CALL *L4D2VisualCameraGetVersionFn)(void);
typedef int (L4D2VISUAL_CAMERA_CALL *L4D2VisualCameraSetExternalActiveFn)(int active);
typedef int (L4D2VISUAL_CAMERA_CALL *L4D2VisualCameraSetPoseFn)(const L4D2VisualCameraPoseV1* pose);
typedef int (L4D2VISUAL_CAMERA_CALL *L4D2VisualCameraGetStatusFn)(L4D2VisualCameraStatusV1* out);

#ifdef __cplusplus
extern "C" {
#endif
uint32_t L4D2VISUAL_CAMERA_CALL L4D2VisualCameraGetVersion(void);
int L4D2VISUAL_CAMERA_CALL L4D2VisualCameraSetExternalActive(int active);
int L4D2VISUAL_CAMERA_CALL L4D2VisualCameraSetPose(const L4D2VisualCameraPoseV1* pose);
int L4D2VISUAL_CAMERA_CALL L4D2VisualCameraGetStatus(L4D2VisualCameraStatusV1* out);
#ifdef __cplusplus
}
#endif
#endif /* L4D2VISUAL_CAMERA_API_H */
