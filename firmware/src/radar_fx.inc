// Shared radar effects drawn straight into LVGL's current tile. Included by
// radar_renderer.inc after line()/disc(); the desktop preview includes it too.
// Each routine touches only pixels inside ctx->clip_area.
#include "radar_fx.h"

struct FxSweepStyle {
  uint32_t near=0xffffff,far=0xffffff; // trail colour at the leading edge and at its end
  uint32_t edge=0xffffff,core=0xffffff; // leading-edge glow and its hot centre line
  uint8_t trailOpa=110,edgeOpa=150,coreOpa=230;
  float span=1.2f;      // trail length, radians
  float radius=200;     // px
  float edgeWidth=5;    // px, glow half width
  float coreWidth=0.9f; // px, hot line half width
  float lift=0.45f;     // trail strength at the centre relative to the rim
  // Rings in the backdrop that brighten while the trail passes over them.
  const float* rings=nullptr;uint8_t ringCount=0;uint32_t ringColor=0xffffff;uint8_t ringOpa=0;
  // A spark where the leading edge meets the chapter ring.
  float rim=0;uint32_t rimColor=0xffffff;uint8_t rimOpa=0;float rimTail=0.35f;
};

static inline lv_color_t fxLerp(lv_color_t a,lv_color_t b,float t){
  return lv_color_mix(b,a,lv_opa_t(t<=0?0:t>=1?255:int(t*255.f+.5f)));
}
// Angle behind the leading edge after which the trail can no longer change a
// pixel: LVGL's 16-bit mix rounds an opacity under 4 to nothing. Beyond it
// the trail is not drawn, and nothing there needs repainting.
static float fxVisibleSpan(const FxSweepStyle& s){
  const float span=s.span<1.55f?s.span:1.55f,peak=float(std::max(s.trailOpa,s.ringOpa));
  if(peak<4)return 0.05f;
  float lo=0,hi=1;for(int i=0;i<24;++i){const float t=(lo+hi)/2;if(peak*dragon::sweepTrail(t)>=3.5f)lo=t;else hi=t;}
  return std::min(span,std::max(hi*span+0.02f,(s.rimOpa?s.rimTail:0)+0.05f)); // the rim spark stays inside
}
// Everything one frame of this sweep can paint, for the repaint bands.
static dragon::RadarOutline fxSweepOutline(int cx,int cy,float lead,const FxSweepStyle& s){
  const float reach=(s.rim>s.radius?s.rim+3:s.radius)+s.edgeWidth*2+1,ahead=s.edgeWidth*2+1.25f,span=fxVisibleSpan(s);
  return dragon::sweepOutline(float(cx),float(cy),lead,span,reach,ahead,ahead/sinf(span)+1);
}
// Box around that outline: fxSweep paints nothing outside it, so painting and
// the repaint bands agree, including the glow that wraps round the centre.
static dragon::RadarRect fxSweepBounds(int cx,int cy,float lead,const FxSweepStyle& s){
  const auto o=fxSweepOutline(cx,cy,lead,s);float x1=float(cx),y1=float(cy),x2=x1,y2=y1;
  for(unsigned i=0;i<o.n;++i){x1=std::min(x1,o.x[i]);y1=std::min(y1,o.y[i]);x2=std::max(x2,o.x[i]);y2=std::max(y2,o.y[i]);}
  return dragon::radarRect(int(floorf(x1))-1,int(floorf(y1))-1,int(ceilf(x2))+1,int(ceilf(y2))+1);
}
// Polar map of the panel, built once in PSRAM: for each pixel its angle
// around the centre (65536 per turn) and its radius in eighths of a pixel.
// The sweep then needs no square root, division or arctangent per pixel.
static constexpr float FxTurn=65536.f/6.28318531f;
static inline uint32_t fxPolarAt(int dx,int dy){
  const float r=sqrtf(float(dx*dx+dy*dy))*8.f+.5f;
  return uint32_t(uint16_t(int32_t(lroundf(atan2f(float(dy),float(dx))*FxTurn))))|uint32_t(r>65535?65535:r)<<16;
}
static uint32_t* fxPolar=nullptr;static int fxPolarX=-1,fxPolarY=-1;static bool fxPolarFailed=false;
static const uint32_t* fxPolarTable(int cx,int cy){
  if(fxPolar&&fxPolarX==cx&&fxPolarY==cy)return fxPolar;
  if(!fxPolar&&!fxPolarFailed){fxPolar=(uint32_t*)ps_malloc(sizeof(uint32_t)*SCREEN_RES*SCREEN_RES);fxPolarFailed=!fxPolar;}
  if(!fxPolar)return nullptr; // fxSweep then computes each pixel itself
  // One octant (0 <= b <= a) is computed and mirrored to the other seven, so
  // building the map costs an eighth of the arctangents and square roots.
  const int reach=std::max(std::max(cx,SCREEN_RES-1-cx),std::max(cy,SCREEN_RES-1-cy));
  auto put=[&](int dx,int dy,uint32_t angle,uint32_t radius){const int x=cx+dx,y=cy+dy;if(x>=0&&y>=0&&x<SCREEN_RES&&y<SCREEN_RES)fxPolar[y*SCREEN_RES+x]=(angle&0xffffu)|radius;};
  for(int a=0;a<=reach;++a)for(int b=0;b<=a;++b){
    const uint32_t e=fxPolarAt(a,b),A=e&0xffffu,r=e&0xffff0000u;
    put(a,b,A,r);put(b,a,16384-A,r);put(-b,a,16384+A,r);put(-a,b,32768-A,r);
    put(-a,-b,32768+A,r);put(-b,-a,49152-A,r);put(b,-a,49152+A,r);put(a,-b,65536-A,r);
  }
  fxPolarX=cx;fxPolarY=cy;return fxPolar;
}
// Lookup tables for one sweep style, built once per style in PSRAM.
struct FxSweepLut {
  static constexpr int Slots=4*(SCREEN_RES/2+24); // radius in quarter pixels
  uint32_t key=0;int slots=0,edgeSlots=0,coreSlots=0;
  uint8_t trailR[Slots],edgeR[Slots],ringR[Slots],rimR[Slots]; // by radius: trail strength (lift, outer edge, centre fade), edge strength, ring proximity, spark band
  uint8_t position[1024],rimA[1024]; // by angle behind the leading edge (65536 per turn, >>6): trail position 0..254 (255 past the trail), spark brightness
  uint8_t trailA[256],ringA[256];lv_color_t trailC[64]; // by trail position
  uint8_t edgeA[64],coreA[16]; // by distance from the leading edge, quarter pixels
};
static uint32_t fxSweepKey(const FxSweepStyle& s){
  uint32_t h=2166136261u;auto mix=[&](uint32_t v){h=(h^v)*16777619u;};auto mixf=[&](float f){uint32_t v;memcpy(&v,&f,4);mix(v);};
  for(uint32_t v:{s.near,s.far,s.edge,s.core,uint32_t(s.trailOpa|s.edgeOpa<<8|s.coreOpa<<16|s.ringOpa<<24),uint32_t(s.ringCount),s.ringColor,s.rimColor,uint32_t(s.rimOpa)})mix(v);
  for(float f:{s.span,s.radius,s.edgeWidth,s.coreWidth,s.lift,s.rim,s.rimTail})mixf(f);
  for(unsigned k=0;k<s.ringCount;++k)mixf(s.rings[k]);
  return h;
}
static const FxSweepLut* fxSweepTables(const FxSweepStyle& s){
  static FxSweepLut* lut=nullptr;static bool failed=false;
  // PSRAM only: internal RAM is kept for the radios. Without it the sweep is skipped.
  if(!lut&&!failed){lut=(FxSweepLut*)ps_malloc(sizeof(FxSweepLut));failed=!lut;if(lut)lut->key=0;}
  if(!lut)return nullptr;
  const uint32_t key=fxSweepKey(s)|1;if(lut->key==key)return lut;
  auto& t=*lut;const float outer=s.radius,reach=(s.rim>outer?s.rim+3:outer)+s.edgeWidth*2+1,span=s.span<1.55f?s.span:1.55f,visible=fxVisibleSpan(s);
  t.slots=std::min(FxSweepLut::Slots,int(reach*4)+2);
  for(int q=0;q<t.slots;++q){
    const float r=(q+.5f)*.25f,centre=r<4?0:r<22?(r-4)/18.f:1.f,inside=outer-r+.5f,cover=inside<=0?0:inside>=1?1:inside;
    t.trailR[q]=uint8_t(255.f*(s.lift+(1-s.lift)*(r/outer))*cover*centre+.5f);t.edgeR[q]=uint8_t(255.f*cover*centre+.5f);
    float ring=0;for(unsigned k=0;k<s.ringCount;++k){const float d=fabsf(r-s.rings[k]);if(d<1.6f)ring=std::max(ring,1-d*0.625f);}
    t.ringR[q]=uint8_t(255.f*ring+.5f);
    const float dr=fabsf(r-s.rim),band=1-dr*0.2f;t.rimR[q]=s.rimOpa&&dr<5?uint8_t(255.f*band*band+.5f):0;
  }
  for(int i=0;i<1024;++i){
    const float behind=(i*64+32)/FxTurn,phi=i>=512?behind-6.28318531f:behind,pos=phi/span; // the upper half of the table is ahead of the edge
    t.position[i]=phi<0||phi>=visible||pos>=1?255:uint8_t(pos*254.f+.5f);
    const float lit=phi<=-0.03f||phi>=s.rimTail?0:phi<0?1+phi*33:1-phi/s.rimTail;t.rimA[i]=uint8_t(s.rimOpa*lit*lit+.5f);
  }
  for(int i=0;i<256;++i){const float pos=i/254.f,trail=dragon::sweepTrail(pos);t.trailA[i]=i==255?0:uint8_t(s.trailOpa*trail+.5f);t.ringA[i]=i==255?0:uint8_t(s.ringOpa*trail+.5f);}
  const lv_color_t near=lv_color_hex(s.near),far=lv_color_hex(s.far);for(int i=0;i<64;++i)t.trailC[i]=fxLerp(near,far,(i*4+2)/254.f);
  t.edgeSlots=std::min(64,int(s.edgeWidth*8)+1);
  for(int j=0;j<t.edgeSlots;++j){const float d=(j*.25f)/s.edgeWidth,g=1/(1+d*d*2.2f);t.edgeA[j]=d<2?uint8_t(s.edgeOpa*g*g+.5f):0;}
  t.coreSlots=std::min(16,int(s.coreWidth*6)+1);
  for(int j=0;j<t.coreSlots;++j){const float dc=(j*.25f)/s.coreWidth,k=dc<.5f?1:dc<1.5f?1.5f-dc:0;t.coreA[j]=uint8_t(s.coreOpa*k+.5f);}
  t.key=key;return lut;
}
static inline unsigned fxDiv255(unsigned v){return (v+128+((v+128)>>8))>>8;}
// Conic phosphor sweep: one pass per pixel, no wedge bands. The trail needs
// a span under 90 degrees, so every lit pixel lies ahead of the centre.
static void fxSweep(lv_draw_ctx_t* ctx,int cx,int cy,float lead,const FxSweepStyle& s){
  const auto bounds=fxSweepBounds(cx,cy,lead,s);
  lv_area_t area{(lv_coord_t)bounds.x1,(lv_coord_t)bounds.y1,(lv_coord_t)bounds.x2,(lv_coord_t)bounds.y2},clip;
  if(!_lv_area_intersect(&clip,ctx->clip_area,&area))return;
  const FxSweepLut* lut=fxSweepTables(s);if(!lut)return;const auto& t=*lut;
  const uint32_t* polar=fxPolarTable(cx,cy);
  auto* buf=(lv_color_t*)ctx->buf;const lv_area_t* ba=ctx->buf_area;const int stride=lv_area_get_width(ba);
  const float ux=cosf(lead),uy=sinf(lead),outer=s.radius,reach=(s.rim>outer?s.rim+3:outer)+s.edgeWidth*2+1;
  const float reach2=reach*reach,edgeLimit=t.edgeSlots*.25f,span=fxVisibleSpan(s);
  const unsigned reach8=unsigned(reach*8),edgeSlots=unsigned(t.edgeSlots),coreSlots=unsigned(t.coreSlots),maxQ=unsigned(t.slots-1);
  const uint16_t lead16=uint16_t(int32_t(lroundf(lead*FxTurn)));
  // Trailing edge direction; nothing is painted more than `margin` behind it.
  const float vx=cosf(lead-span),vy=sinf(lead-span),margin=edgeLimit*std::max(1.f,cosf(span)/sinf(span))+1;
  const lv_color_t edge=lv_color_hex(s.edge),core=lv_color_hex(s.core),ringColor=lv_color_hex(s.ringColor),rimColor=lv_color_hex(s.rimColor);
  for(int y=clip.y1;y<=clip.y2;++y){
    const float dy=float(y-cy),room=reach2-dy*dy;if(room<0)continue;
    // Columns this row can touch: inside the circle, ahead of the centre,
    // no further than the glow in front of the leading edge, and no further
    // than `margin` behind the trailing edge. One pixel of slack each side;
    // the per-pixel tests below stay exact.
    const float dyUy=dy*uy,dyUx=dy*ux,half=sqrtf(room);float lo=-half,hi=half;
    auto limit=[&](float k,float c,bool atLeast){ // k*dx >= c (atLeast) or k*dx <= c
      if(fabsf(k)<1e-4f){if(atLeast?0<c:0>c)lo=1,hi=0;return;}
      const float v=c/k;if((k>0)==atLeast)lo=std::max(lo,v);else hi=std::min(hi,v);
    };
    limit(ux,-dyUy,true);                 // ahead of the centre
    limit(uy,dyUx-edgeLimit,true);        // no further ahead than the glow
    limit(vy,margin+vx*dy,false);         // behind the trailing edge by at most margin
    if(lo>hi)continue;
    const int xa=std::max<int>(clip.x1,int(floorf(cx+lo))-1),xb=std::min<int>(clip.x2,int(ceilf(cx+hi))+1);
    lv_color_t* row=buf+(y-ba->y1)*stride-ba->x1;
    const uint32_t* polarRow=polar?polar+y*SCREEN_RES:nullptr;
    for(int x=xa;x<=xb;++x){
      const uint32_t e=polarRow?polarRow[x]:fxPolarAt(x-cx,y-cy);
      const unsigned r8=e>>16;if(r8>=reach8)continue;
      const uint16_t behind=uint16_t(lead16-uint16_t(e));const int sb=int16_t(behind); // >0: behind the leading edge
      if(sb<=-16384||sb>=16384)continue; // behind the centre
      // Distance from the leading edge as arc length, in quarter pixels.
      const unsigned across4=(((unsigned(sb<0?-sb:sb)*r8)>>10)*3217u)>>16;
      if(sb<0&&across4>=edgeSlots)continue;
      const unsigned q=std::min(r8>>1,maxQ),angle=behind>>6;
      lv_color_t c=row[x];bool changed=false;
      if(sb>0){ // trail
        const uint8_t pos=t.position[angle];
        if(pos!=255){
          const unsigned a=fxDiv255(unsigned(t.trailA[pos])*t.trailR[q]);if(a){c=lv_color_mix(t.trailC[pos>>2],c,a);changed=true;}
          if(t.ringR[q]){const unsigned ra=fxDiv255(unsigned(t.ringA[pos])*t.ringR[q]);if(ra){c=lv_color_mix(ringColor,c,ra);changed=true;}}
        }
      }
      // Leading edge: glow plus a hot core, constant pixel width along the line.
      if(across4<edgeSlots&&t.edgeR[q]){
        const unsigned a=fxDiv255(unsigned(t.edgeA[across4])*t.edgeR[q]);if(a){c=lv_color_mix(edge,c,a);changed=true;}
        if(across4<coreSlots){const unsigned k=fxDiv255(unsigned(t.coreA[across4])*t.edgeR[q]);if(k){c=lv_color_mix(core,c,k);changed=true;}}
      }
      // Spark on the chapter ring and a short lit arc behind it.
      if(t.rimR[q]&&t.rimA[angle]){const unsigned a=fxDiv255(unsigned(t.rimA[angle])*t.rimR[q]);if(a){c=lv_color_mix(rimColor,c,a);changed=true;}}
      if(changed)row[x]=c;
    }
  }
}
static void fxRing(lv_draw_ctx_t* ctx,int x,int y,int radius,int width,uint32_t c,uint8_t opa){
  if(!opa||radius<1)return;
  lv_draw_arc_dsc_t arc;lv_draw_arc_dsc_init(&arc);arc.color=lv_color_hex(c);arc.width=width;arc.opa=opa;
  lv_point_t p{(lv_coord_t)x,(lv_coord_t)y};lv_draw_arc(ctx,&arc,&p,radius,0,360);
}
// Ripple ring for a packet received sinceMs ago, scaled by the dot's opacity.
struct FxRipple {uint8_t opa=0,radius=0;};
static FxRipple fxRipple(uint32_t sinceMs,uint8_t dotOpa){
  const auto r=dragon::radarRipple(sinceMs);FxRipple out;
  out.opa=uint8_t(r.opa*dotOpa/255);out.radius=uint8_t(S(7)+int(r.grow*S(15)));return out;
}
// Live dot: soft glow, solid core and a small specular highlight, plus the
// ripple while a packet is fresh. The glow brightens with the ripple.
static void fxLiveDot(lv_draw_ctx_t* ctx,int x,int y,uint32_t c,uint8_t opa,FxRipple ripple){
  if(ripple.opa)fxRing(ctx,x,y,ripple.radius,std::max(1,S(2)-(ripple.radius>S(15)?1:0)),c,ripple.opa);
  const uint8_t boost=ripple.opa/3;
  disc(ctx,x,y,S(11),c,uint8_t((22+boost/2)*opa/255));disc(ctx,x,y,S(8),c,uint8_t((48+boost)*opa/255));
  disc(ctx,x,y,S(5),c,opa);disc(ctx,x-S(1),y-S(1),std::max(1,S(2)),0xffffff,uint8_t(opa*3/4));
}
// Rotating lock brackets around a tracked contact.
static void fxLockBrackets(lv_draw_ctx_t* ctx,int x,int y,uint32_t c,uint32_t now){
  lv_draw_arc_dsc_t arc;lv_draw_arc_dsc_init(&arc);arc.color=lv_color_hex(c);arc.width=std::max(2,S(2));arc.opa=230;arc.rounded=1;
  lv_point_t p{(lv_coord_t)x,(lv_coord_t)y};const int spin=int((now/8)%360);
  for(int q=0;q<360;q+=90)lv_draw_arc(ctx,&arc,&p,S(13),uint16_t((q+spin)%360),uint16_t((q+spin+42)%360));
}
// Callout plate: dark glass, a hairline border in the device colour and an
// accent bar, family name bright and the signal line muted.
static void fxCallout(lv_draw_ctx_t* ctx,int x,int y,int w,int h,const char* text,uint32_t c,bool primary,bool square,const lv_font_t* font,uint32_t plate,uint32_t muted){
  lv_draw_rect_dsc_t box;lv_draw_rect_dsc_init(&box);box.radius=square?0:S(5);box.bg_color=lv_color_hex(plate);box.bg_opa=LV_OPA_COVER; // opaque: a plain fill, no per-pixel blend
  box.border_color=lv_color_hex(c);box.border_opa=primary?210:120;box.border_width=1;
  lv_area_t area{(lv_coord_t)x,(lv_coord_t)y,(lv_coord_t)(x+w),(lv_coord_t)(y+h)};lv_draw_rect(ctx,&box,&area);
  lv_draw_rect_dsc_t bar;lv_draw_rect_dsc_init(&bar);bar.radius=square?0:1;bar.bg_color=lv_color_hex(c);bar.bg_opa=primary?255:190;
  const int bx=x+2;lv_area_t accent{(lv_coord_t)bx,(lv_coord_t)(y+S(6)),(lv_coord_t)(bx+std::max(1,S(2))-1),(lv_coord_t)(y+h-S(6))};lv_draw_rect(ctx,&bar,&accent);
  char title[40];const char* state=strchr(text,'\n');size_t n=state?size_t(state-text):strlen(text);n=std::min(n,sizeof(title)-1);memcpy(title,text,n);title[n]=0;
  // One line each: a long name is clipped by the plate, never wrapped over the signal line.
  lv_draw_label_dsc_t d;lv_draw_label_dsc_init(&d);d.color=lv_color_hex(c);d.font=font;d.flag=LV_TEXT_FLAG_EXPAND;
  lv_area_t textArea{(lv_coord_t)(x+S(5)+1),(lv_coord_t)(y+S(3)),(lv_coord_t)(x+w-S(3)),(lv_coord_t)(y+h-S(2))};
  lv_area_t clip;if(_lv_area_intersect(&clip,ctx->clip_area,&textArea)){const lv_area_t* saved=ctx->clip_area;ctx->clip_area=&clip;
    lv_draw_label(ctx,&d,&textArea,title,nullptr);
    if(state){d.color=lv_color_hex(muted);textArea.y1+=font->line_height-1;lv_draw_label(ctx,&d,&textArea,state+1,nullptr);}
    ctx->clip_area=saved;
  }
}
// Leader from a dot to its plate: dark casing, thin coloured core, anchor.
static void fxLeader(lv_draw_ctx_t* ctx,int x1,int y1,int x2,int y2,uint32_t c,uint32_t casing){
  line(ctx,x1,y1,x2,y2,casing,std::max(3,S(3)),235);
  line(ctx,x1,y1,x2,y2,c,std::max(1,S(1))+1,230);
  disc(ctx,x2,y2,std::max(2,S(2)),c);
}
static uint32_t fxBlend(uint32_t a,uint32_t b,float t){
  auto ch=[&](int s){const float x=float((a>>s)&255),y=float((b>>s)&255);return uint32_t(x+(y-x)*t+.5f)<<s;};
  return ch(16)|ch(8)|ch(0);
}
// Sweep for the shared radar field, from a theme's own colours: rings at
// 42/84/126/168 brighten under the trail and the outer ring carries the spark.
static FxSweepStyle fxThemeSweepStyle(uint32_t sweep,uint32_t bottom,uint32_t ring){
  static float rings[4];for(int i=0;i<4;++i)rings[i]=float(S(42*(i+1)));
  FxSweepStyle s;s.near=sweep;s.far=fxBlend(sweep,bottom,0.55f);s.edge=sweep;s.core=fxBlend(sweep,0xffffff,0.65f);
  s.trailOpa=150;s.edgeOpa=150;s.coreOpa=235;s.span=1.3f;s.radius=float(S(166));
  s.edgeWidth=std::max(2.f,4.f*SCREEN_RES/360.f);s.coreWidth=SCREEN_RES>=400?1.f:0.8f;s.lift=0.45f;
  s.rings=rings;s.ringCount=4;s.ringColor=fxBlend(ring,0xffffff,0.35f);s.ringOpa=110;
  s.rim=float(S(170));s.rimColor=s.core;s.rimOpa=190;s.rimTail=0.45f;
  return s;
}
// Shared centre pivot: a small ring and a breathing dot in the sweep colour.
static void fxCenter(lv_draw_ctx_t* ctx,uint32_t sweep,uint32_t plate,uint32_t now){
  const float b=dragon::radarBreath(now);
  disc(ctx,CENTER,CENTER,S(9),sweep,uint8_t(14+30*b));
  fxRing(ctx,CENTER,CENTER,S(6),std::max(1,S(1)),sweep,200);
  disc(ctx,CENTER,CENTER,S(4),plate);disc(ctx,CENTER,CENTER,S(2)+1,sweep,uint8_t(170+85*b));
}
static dragon::RadarRect fxCenterBounds(){return dragon::rectAround(CENTER,CENTER,S(9)+2);}
