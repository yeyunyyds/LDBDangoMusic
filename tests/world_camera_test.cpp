#include "world_camera.hpp"
#include <iostream>
#include <stdexcept>
using namespace dango;
void check(bool value){if(!value)throw std::runtime_error("World camera regression");}
bool near(float a,float b){return std::abs(a-b)<.001f;}
int main(){
 GameCameraFrame f;f.active=1;f.width=1920;f.height=1080;f.tick=1000;
 // Camera at origin looking down +Z, native Source wire uses column vectors.
 f.worldToView={1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
 f.worldToClip={1,0,0,0,0,1,0,0,0,0,1.0001f,-1.0001f,0,0,1,0};
 check(usableCamera(f,1100)&&!usableCamera(f,1600)&&!usableCamera(f,999));
 CameraPose pose;check(cameraPose(f,pose)&&near(pose.forward.z,1)&&near(pose.right.x,1)&&near(pose.up.y,1));
 auto anchor=anchorAt(pose,100,0,0);Placement p;check(projectAnchor(f,anchor,1920,1080,8,32,p)&&near(p.x,960)&&near(p.y,540));float initialScale=p.scale;
 auto moved=f;moved.worldToView[3]=-10;moved.worldToClip[3]=-10;
 check(projectAnchor(moved,anchor,1920,1080,8,32,p)&&p.x<960); // Fixed world point shifts when camera translates.
 check(projectAnchor(f,{0,0,200},1920,1080,8,32,p)&&p.scale<initialScale);
 check(!projectAnchor(f,{0,0,-100},1920,1080,8,32,p));
 auto source=f;source.worldToView={0,-1,0,0,0,0,1,0,1,0,0,0,0,0,0,1};source.worldToClip={0,-1,0,0,0,0,1,0,1.0001f,0,0,-1.0001f,1,0,0,0};
 check(cameraPose(source,pose)&&near(pose.forward.x,1)&&near(pose.right.y,-1)&&near(pose.up.z,1));check(projectAnchor(source,{100,0,0},1920,1080,8,32,p)&&near(p.x,960));
 auto invalid=f;invalid.worldToClip=f.worldToView;check(!cameraPose(invalid,pose));invalid=f;invalid.active=0;check(!usableCamera(invalid,1100));invalid=f;invalid.worldToView={};check(!cameraPose(invalid,pose));
 std::cout<<"Passed: local camera snapshot, lease/state validation, inverse camera basis, anchored parallax, distance scale, behind-camera rejection\n";
}
