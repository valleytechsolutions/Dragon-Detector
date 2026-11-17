#include "radar_fx.h"
// Radar effect geometry: the repaint bookkeeping relies on these bounds being
// conservative, so a missed pixel would leave a stale trail on the panel.
static void radarFxTests(){
  using namespace dragon;
  // fastAtan2 stays within 0.002 rad of atan2 in every quadrant.
  for(int y=-60;y<=60;++y)for(int x=-60;x<=60;++x){if(!x&&!y)continue;assert(fabsf(fastAtan2(float(y),float(x))-atan2f(float(y),float(x)))<0.002f);}
  assert(fastAtan2(0,0)==0);
  for(float a:{-20.f,-RadarTau,-0.001f,0.f,3.f,RadarTau,40.f}){const float w=wrapAngle(a);assert(w>=0&&w<RadarTau+1e-4f);}
  // Sector boxes hold every point of the sector, for any lead and span.
  for(int lead=0;lead<360;lead+=7)for(float span:{0.3f,1.3f,1.45f,2.5f,6.3f}){
    const float l=lead*.0174532925f;const auto box=sectorBounds(233,233,214,l,span);
    for(int k=0;k<=64;++k){const float a=l-span*k/64.f;for(float r:{0.f,60.f,140.f,214.f}){const float x=233+cosf(a)*r,y=233+sinf(a)*r;assert(x>=box.x1&&x<=box.x2&&y>=box.y1&&y<=box.y2);}}
    assert(box.x2-box.x1<=2*214+8&&box.y2-box.y1<=2*214+8);
  }
  // A short sector is a small box: a 0.3 rad sweep near the top never spans the panel.
  {const auto box=sectorBounds(233,233,214,-1.5707963f,0.3f);assert(box.area()<214L*90);}
  // Sweep outline bands hold every pixel the sweep can paint, in two frames.
  {uint32_t seed=11;auto rnd=[&](){seed=seed*1664525u+1013904223u;return (seed>>8)/16777216.f;};
    long bandArea=0,boxArea=0;
    for(int lead=0;lead<360;lead+=5)for(float span:{0.3f,1.3f,1.45f}){
      const float a=lead*.0174532925f,b=a+0.07f,reach=230,ahead=11.25f,hub=ahead/sinf(span)+1;
      RadarOutline o[2]={sweepOutline(233,233,a,span,reach,ahead,hub),sweepOutline(233,233,b,span,reach,ahead,hub)};
      RadarRect bands[16];const unsigned n=outlineBands(o,2,40,2,466,bands,16);assert(n<=12);
      for(int k=0;k<4000;++k){const float x=rnd()*466,y=rnd()*466,dx=x-233,dy=y-233,r=hypotf(dx,dy);
        for(float lead2:{a,b}){const float ux=cosf(lead2),uy=sinf(lead2),along=dx*ux+dy*uy,across=uy*dx-ux*dy;
          const bool painted=along>0&&across>=-ahead&&r<=reach&&((across>0&&atan2f(across,along)<span)||fabsf(across)<ahead);
          if(!painted)continue;bool in=false;for(unsigned i=0;i<n;++i)if(x>=bands[i].x1&&x<=bands[i].x2+1&&y>=bands[i].y1&&y<=bands[i].y2+1)in=true;assert(in);}
      }
      if(span>1.4f){for(unsigned i=0;i<n;++i)bandArea+=bands[i].area();boxArea+=rectUnion(sectorBounds(233,233,reach+ahead,a+ahead/reach,span+2*ahead/reach),sectorBounds(233,233,reach+ahead,b+ahead/reach,span+2*ahead/reach)).area();}
    }
    assert(bandArea*10<boxArea*8); // tighter than the two bounding boxes
  }
  // Trail: full at the edge, gone at its end, never brighter further back.
  assert(sweepTrail(0)==1&&sweepTrail(1)==0&&sweepTrail(-1)==1&&sweepTrail(2)==0);
  for(int i=0;i<100;++i)assert(sweepTrail(i/100.f)>=sweepTrail((i+1)/100.f));
  // Ripple: grows out and fades, ending exactly at RadarRippleMs.
  assert(radarRipple(0).opa==200&&radarRipple(0).grow==0&&radarRipple(RadarRippleMs).opa==0&&radarRipple(0xffffffffu).opa==0);
  for(uint32_t ms=0;ms+1<RadarRippleMs;++ms){assert(radarRipple(ms).opa>=radarRipple(ms+1).opa);assert(radarRipple(ms).grow<=radarRipple(ms+1).grow);}
  for(uint32_t ms=0;ms<7000;ms+=13){const float b=radarBreath(ms);assert(b>=0&&b<=1);}
  // Dirty areas: every added box stays covered, touching boxes merge.
  {RadarDirty<4> d;d.add(radarRect(10,10,20,20),466);d.add(radarRect(24,10,30,20),466);assert(d.count==1&&d.rects[0].x1==10&&d.rects[0].x2==30);
    d.add(radarRect(200,200,210,210),466);assert(d.count==2&&!d.full);
    d.add(radarRect(-50,-50,-10,-10),466);assert(d.count==2); // off panel
    d.add(radarRect(460,460,500,500),466);assert(d.count==3&&d.rects[2].x2==465);}
  {RadarDirty<3> d;for(int i=0;i<4;++i)d.add(rectAround(30+i*100,30,5),466);assert(d.full);}
  {RadarDirty<20> d;d.add(radarRect(0,0,465,300),466);assert(d.full);} // most of the panel: repaint it all
  // Random boxes: the merged set covers each one.
  {uint32_t seed=7;auto rnd=[&](int n){seed=seed*1103515245u+12345u;return int((seed>>16)%unsigned(n));};
    for(int round=0;round<200;++round){RadarDirty<20> d;RadarRect added[12];unsigned n=0;
      for(int i=0;i<12;++i){const int x=rnd(430),y=rnd(430);added[n++]=radarRect(x,y,x+rnd(30),y+rnd(30));d.add(added[n-1],466);}
      if(d.full)continue;
      for(unsigned i=0;i<n;++i){bool covered=false;for(unsigned k=0;k<d.count;++k){const auto& r=d.rects[k];if(added[i].x1>=r.x1&&added[i].x2<=r.x2&&added[i].y1>=r.y1&&added[i].y2<=r.y2)covered=true;}assert(covered);}
      for(unsigned a=0;a<d.count;++a)for(unsigned b=a+1;b<d.count;++b)assert(!rectsTouch(d.rects[a],d.rects[b],6));
    }
  }
}
