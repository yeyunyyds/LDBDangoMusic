#include "wall_panel.hpp"
#include <iostream>
#include <stdexcept>
#include <limits>
using namespace dango;
void check(bool ok){if(!ok)throw std::runtime_error("Wall panel geometry regression");}
bool near(float a,float b){return std::abs(a-b)<.001f;}
GameCameraFrame camera(Vec3 eye={},float yaw=0){GameCameraFrame f;f.active=1;f.epoch=7;f.width=1280;f.height=720;
 Vec3 forward{std::cos(yaw),std::sin(yaw),0},right{std::sin(yaw),-std::cos(yaw),0},up{0,0,1};float r=-wall::dot(right,eye),u=-wall::dot(up,eye),d=-wall::dot(forward,eye);
 f.worldToView={right.x,right.y,right.z,r,up.x,up.y,up.z,u,forward.x,forward.y,forward.z,d,0,0,0,1};
 f.worldToClip={right.x,right.y,right.z,r,up.x,up.y,up.z,u,forward.x*1.0001f,forward.y*1.0001f,0,d*1.0001f-1.0001f,forward.x,forward.y,0,d};return f;}
int main(){
 wall::Panel p;check(wall::makePanel({100,0,0},{-1,0,0},40,20,7,p));check(near(p.center.x,99.25f)&&near(p.right.y,-1)&&near(p.up.z,1));
 auto saved=p;auto corners=std::array<Vec3,4>{wall::point(p,0,0),wall::point(p,1,0),wall::point(p,0,1),wall::point(p,1,1)};
 std::array<wall::Vertex,4> front,moved,turned;
 check(wall::projectPanel(p,camera(),1280,720,front));check(front[0].x<front[1].x&&front[0].y<front[2].y&&near(front[0].rhw,front[1].rhw));
 check(wall::projectPanel(p,camera({0,10,0}),1280,720,moved));check(moved[0].x>front[0].x);
 check(wall::projectPanel(p,camera({},.4f),1280,720,turned));check(!near(turned[0].rhw,turned[1].rhw));check(std::abs((turned[2].y-turned[0].y)-(turned[3].y-turned[1].y))>1); // perspective foreshortening, not a billboard
 for(int i=0;i<4;++i){auto q=wall::point(p,i%2,i/2);check(near(q.x,corners[i].x)&&near(q.y,corners[i].y)&&near(q.z,corners[i].z));}
 check(near(p.normal.x,saved.normal.x)&&near(p.right.y,saved.right.y));
 wall::Panel aroundCorner;check(wall::makePanel({120,100,0},{0,-1,0},40,20,7,aroundCorner));check(wall::projectPanel(aroundCorner,camera({120,0,0},1.5707963f),1280,720,front));check(!wall::projectPanel(p,camera({120,0,0}),1280,720,front));
 auto degenerate=p;degenerate.width=0;check(!wall::projectPanel(degenerate,camera(),1280,720,front));
 auto changedMap=camera();changedMap.epoch=8;check(!wall::projectPanel(p,changedMap,1280,720,front));
 check(!wall::makePanel({0,0,0},{0,0,1},40,20,7,p));check(!wall::makePanel({0,0,0},{0,0,0},40,20,7,p));check(!wall::makePanel({0,0,0},{-1,0,0},0,20,7,p));check(!wall::makePanel({std::numeric_limits<float>::quiet_NaN(),0,0},{-1,0,0},40,20,7,p));
 wall::Vertex vertex;check(!wall::projectVertex(camera().worldToClip,{-10,0,0},1280,720,0,0,vertex));
 std::cout<<"Passed: fixed wall axes, camera translation/yaw perspective, unequal corner rhw, corridor turn, backface/map rejection and invalid hits\n";
}
