#pragma once
#include <algorithm>
#include <cmath>

namespace dragon {
struct PanelPoint { int x,y; };
struct PanelBox { int x,y,w,h; };
// Native bounds are supplied after rotation; no circular safe area is applied.
inline PanelBox rectangularRadarField(int width,int height){return {16,98,width-32,height-208};}
inline PanelPoint rectangularRadarPoint(float radius,float ux,float uy,int width,int height){
  const auto b=rectangularRadarField(width,height);
  const float largest=std::max(std::abs(ux),std::abs(uy));
  const float r=std::clamp(radius/170.f,0.f,1.f);
  return {b.x+b.w/2+(largest?int(std::lround(ux/largest*r*(b.w/2-12))):0),
          b.y+b.h/2+(largest?int(std::lround(uy/largest*r*(b.h/2-12))):0)};
}
inline PanelBox rectangularRadarLabel(unsigned slot,int width,int height){
  const auto b=rectangularRadarField(width,height);
  const int labelWidth=std::min(150,(b.w-20)/2),labelHeight=40;
  return {slot%2?b.x+b.w-labelWidth:b.x,b.y+int(slot/2)*(b.h-labelHeight)/2,labelWidth,labelHeight};
}
// The clock chapter follows a rectangle while preserving true hand angles.
inline PanelPoint rectangularRay(float turns,int cx,int cy,int halfWidth,int halfHeight){
  const float angle=turns*6.28318530718f,dx=std::sin(angle),dy=-std::cos(angle);
  const float scale=1.f/std::max(std::abs(dx)/halfWidth,std::abs(dy)/halfHeight);
  return {cx+int(std::lround(dx*scale)),cy+int(std::lround(dy*scale))};
}
}
