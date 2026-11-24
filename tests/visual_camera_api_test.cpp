#include "../vendor/l4d2visual_camera_api.h"
#include <cassert>
#include <cstdint>
#include <type_traits>
static_assert(sizeof(L4D2VisualCameraPoseV1)==28,"ABI pose layout changed");
static_assert(sizeof(L4D2VisualCameraStatusV1)==36,"ABI status layout changed");
static_assert(std::is_standard_layout<L4D2VisualCameraPoseV1>::value,"C ABI required");
static_assert(std::is_standard_layout<L4D2VisualCameraStatusV1>::value,"C ABI required");
static_assert(L4D2VISUAL_CAMERA_ABI_VERSION==2u,"Must coordinate version");
int main(){
 L4D2VisualCameraPoseV1 p{};
 p.size=sizeof(p);
 assert(p.size==28);
 L4D2VisualCameraStatusV1 s{};
 s.size=sizeof(s);
 assert(s.size==36&&L4D2VISUAL_CAMERA_OWNER_EXTERNAL==2);
}
