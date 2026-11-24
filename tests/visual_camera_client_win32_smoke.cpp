#include "../src/visual_camera_client.hpp"
int main(){
 L4D2VisualCameraStatusV1 s{};
 (void)dango::visualcamera::status(s);
 L4D2VisualCameraPoseV1 p{};(void)dango::visualcamera::getPose(p);
 (void)dango::visualcamera::externallyControlling();
 // Without L4N/Visual present, all API calls are safe and do nothing.
 (void)dango::visualcamera::request(false);
 (void)dango::visualcamera::pose(0.f,0.f,0.f,0.f,0.f);
 return 0;
}
