#pragma once
// Pure geometry and timing for radar visual effects. No LVGL, no Arduino:
// the host tests and the desktop preview build use it unchanged.
// Every effect here is decoration. Sweep angle is not a bearing, and a
// ripple only marks that a packet from that device was just received.
#include <stdint.h>
#include <math.h>

namespace dragon {
struct RadarRect {
  int x1=0,y1=0,x2=-1,y2=-1;
  bool empty()const{return x2<x1||y2<y1;}
  long area()const{return empty()?0:long(x2-x1+1)*long(y2-y1+1);}
};
inline RadarRect radarRect(int x1,int y1,int x2,int y2){RadarRect r;r.x1=x1;r.y1=y1;r.x2=x2;r.y2=y2;return r;}
inline RadarRect rectAround(int x,int y,int radius){return radarRect(x-radius,y-radius,x+radius,y+radius);}
inline RadarRect rectUnion(const RadarRect& a,const RadarRect& b){
  if(a.empty())return b;if(b.empty())return a;
  return radarRect(a.x1<b.x1?a.x1:b.x1,a.y1<b.y1?a.y1:b.y1,a.x2>b.x2?a.x2:b.x2,a.y2>b.y2?a.y2:b.y2);
}
inline bool rectsTouch(const RadarRect& a,const RadarRect& b,int gap=0){
  return !a.empty()&&!b.empty()&&a.x1<=b.x2+gap&&b.x1<=a.x2+gap&&a.y1<=b.y2+gap&&b.y1<=a.y2+gap;
}
inline RadarRect rectClip(const RadarRect& a,int size){
  if(a.empty())return a;
  RadarRect r=radarRect(a.x1<0?0:a.x1,a.y1<0?0:a.y1,a.x2>=size?size-1:a.x2,a.y2>=size?size-1:a.y2);
  return r.empty()?RadarRect{}:r;
}

// atan2 within about 0.0015 rad, without a library call per pixel.
inline float fastAtan2(float y,float x){
  const float ax=fabsf(x),ay=fabsf(y);
  if(ax==0&&ay==0)return 0;
  const float a=(ax<ay?ax/ay:ay/ax),s=a*a;
  float r=((-0.0464964749f*s+0.15931422f)*s-0.327622764f)*s*a+a;
  if(ay>ax)r=1.57079637f-r;
  if(x<0)r=3.14159274f-r;
  return y<0?-r:r;
}
constexpr float RadarTau=6.28318531f;
inline float wrapAngle(float a){a=fmodf(a,RadarTau);return a<0?a+RadarTau:a;}

// Box of every point within `radius` of the centre whose screen angle lies in
// [lead-span, lead] (screen angles: x=cos, y=sin, so they grow clockwise).
inline RadarRect sectorBounds(int cx,int cy,float radius,float lead,float span){
  if(span>=RadarTau-0.001f)return rectAround(cx,cy,int(ceilf(radius))+1);
  const float a0=lead-span;
  float x1=float(cx),x2=float(cx),y1=float(cy),y2=float(cy);
  auto take=[&](float a){float x=cx+cosf(a)*radius,y=cy+sinf(a)*radius;if(x<x1)x1=x;if(x>x2)x2=x;if(y<y1)y1=y;if(y>y2)y2=y;};
  take(a0);take(lead);
  for(int k=0;k<4;++k){const float axis=k*1.57079633f;if(wrapAngle(axis-a0)<=span)take(axis);}
  return radarRect(int(floorf(x1))-2,int(floorf(y1))-2,int(ceilf(x2))+2,int(ceilf(y2))+2);
}

// Convex outline of every pixel one sweep frame can paint: the trail wedge
// (span under 90 degrees), the glow band `ahead` px in front of the leading
// edge, and a hub of radius `hub` around the centre.
struct RadarOutline {float x[40],y[40];unsigned n=0;};
inline RadarOutline sweepOutline(float cx,float cy,float lead,float span,float reach,float ahead,float hub){
  float px[40],py[40];unsigned m=0;
  const float ux=cosf(lead),uy=sinf(lead),ax=-uy*ahead,ay=ux*ahead;
  auto add=[&](float x,float y){px[m]=x;py[m]=y;++m;};
  constexpr int Steps=16;const float out=reach/cosf(span/(2*Steps)); // chords circumscribe the arc
  add(cx+ux*out+ax,cy+uy*out+ay);
  for(int k=0;k<=Steps;++k){const float a=lead-span*k/Steps;add(cx+cosf(a)*out,cy+sinf(a)*out);}
  for(int k=0;k<8;++k){const float a=k*0.785398163f,r=hub*1.0824f;add(cx+cosf(a)*r,cy+sinf(a)*r);} // octagon around the hub
  add(cx+ax,cy+ay);
  // Andrew's monotone chain: sort by x then y, build lower and upper hulls.
  unsigned order[40];for(unsigned i=0;i<m;++i)order[i]=i;
  for(unsigned i=1;i<m;++i){unsigned v=order[i],j=i;while(j&&(px[order[j-1]]>px[v]||(px[order[j-1]]==px[v]&&py[order[j-1]]>py[v]))){order[j]=order[j-1];--j;}order[j]=v;}
  auto cross=[&](unsigned o,unsigned a,unsigned b){return (px[a]-px[o])*(py[b]-py[o])-(py[a]-py[o])*(px[b]-px[o]);};
  unsigned hull[80];unsigned h=0;
  for(unsigned i=0;i<m;++i){while(h>=2&&cross(hull[h-2],hull[h-1],order[i])<=0)--h;hull[h++]=order[i];}
  for(unsigned i=m-1,t=h+1;i-->0;){while(h>=t&&cross(hull[h-2],hull[h-1],order[i])<=0)--h;hull[h++]=order[i];}
  RadarOutline o;for(unsigned i=0;i+1<h&&o.n<40;++i){o.x[o.n]=px[hull[i]];o.y[o.n]=py[hull[i]];++o.n;}
  return o;
}
// Horizontal bands `height` rows tall (aligned to multiples of height) that
// cover the union of convex outlines, padded by `pad`. Returns the count.
inline unsigned outlineBands(const RadarOutline* outlines,unsigned count,int height,int pad,int size,RadarRect* out,unsigned maxOut){
  unsigned n=0;
  for(int y0=0;y0<size&&n<maxOut;y0+=height){
    const float lo=float(y0-pad),hi=float(y0+height-1+pad);float x1=1e9f,x2=-1e9f;
    for(unsigned k=0;k<count;++k){const auto& o=outlines[k];
      for(unsigned i=0;i<o.n;++i){const unsigned j=(i+1)%o.n;float ax=o.x[i],ay=o.y[i],bx=o.x[j],by=o.y[j];
        if(ay>by){float t=ax;ax=bx;bx=t;t=ay;ay=by;by=t;}
        if(by<lo||ay>hi)continue;
        // Clip the edge to the band and take both ends.
        auto at=[&](float y){return by==ay?ax:ax+(bx-ax)*(y-ay)/(by-ay);};
        const float ya=ay<lo?lo:ay,yb=by>hi?hi:by,xa=at(ya),xb=at(yb);
        x1=fminf(x1,fminf(xa,xb));x2=fmaxf(x2,fmaxf(xa,xb));
      }
    }
    if(x2<x1)continue;
    const auto r=rectClip(radarRect(int(floorf(x1))-pad,y0,int(ceilf(x2))+pad,y0+height-1),size);
    if(!r.empty())out[n++]=r;
  }
  return n;
}

// Phosphor trail: 1 at the leading edge, 0 at the end of the trail. A short
// bright shoulder then a long soft tail reads as light, not as wedge bands.
inline float sweepTrail(float t){
  if(t<=0)return 1;if(t>=1)return 0;
  const float u=1-t;return u*u*(0.35f+0.65f*u);
}

// A received packet sends one expanding ring out from its dot.
constexpr uint32_t RadarRippleMs=1100;
struct RadarRipple {float grow=0;uint8_t opa=0;};
inline RadarRipple radarRipple(uint32_t sinceSample){
  RadarRipple r;if(sinceSample>=RadarRippleMs)return r;
  const float p=float(sinceSample)/float(RadarRippleMs),out=1-(1-p)*(1-p);
  r.grow=out;const float fade=1-p;r.opa=uint8_t(200*fade*fade);
  return r;
}
// Slow 0..1 breathing for small static jewels (the centre pivot).
inline float radarBreath(uint32_t now,uint32_t periodMs=3200){
  const float p=float(now%periodMs)/float(periodMs);return 0.5f-0.5f*cosf(p*RadarTau);
}

// Areas a radar frame must repaint. Overlapping or nearly touching areas are
// merged; past the slot count, or when the repaint would cover most of the
// panel anyway, the whole radar is repainted instead.
template<unsigned Slots=20> struct RadarDirty {
  RadarRect rects[Slots];unsigned count=0;bool full=false;
  void clear(){count=0;full=false;}
  void add(RadarRect r,int size){
    if(full)return;r=rectClip(r,size);if(r.empty())return;
    for(bool merged=true;merged;){merged=false;
      for(unsigned i=0;i<count;++i)if(rectsTouch(rects[i],r,6)){r=rectUnion(r,rects[i]);rects[i]=rects[--count];merged=true;break;}
    }
    if(count==Slots){full=true;return;}
    rects[count++]=r;
    if(area()*10>long(size)*long(size)*6)full=true;
  }
  long area()const{long a=0;for(unsigned i=0;i<count;++i)a+=rects[i].area();return a;}
};
}
