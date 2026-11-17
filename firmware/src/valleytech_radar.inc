// Valleytech: enamel-black dial, machined gold bezel and minute track, electric
// cyan phosphor sweep. Matches the Valleytech watch. Sweep geometry is
// decorative, never a bearing measurement.
namespace valley {
constexpr uint32_t Gold=0xd6b46e,GoldDeep=0x6d5b38,GoldPale=0xf2dfad,Hot=0xfff6dc,Ice=0x9ff4ff;
constexpr uint32_t TrailNear=0x7ff4ff,TrailFar=0x0b5e8a,Edge=0x8ff0ff,Core=0xf2ffff,RingLit=0xbff6ff,Spark=0xe8fdff;
constexpr float RingA=53,RingB=106,TrackIn=159,TrackOut=171.6f,Slate=178.4f,Edge360=180;
}
// Vector field: boards without a PSRAM backdrop (Lite, Dial) and the
// allocation-failure fallback.
static void valleyRadarField(lv_draw_ctx_t* ctx){
  disc(ctx,CENTER,CENTER,S(178),valley::GoldDeep);
  disc(ctx,CENTER,CENTER,S(177),0x080a0b);
  lv_point_t center{lv_coord_t(CENTER),lv_coord_t(CENTER)};
  lv_draw_arc_dsc_t arc;lv_draw_arc_dsc_init(&arc);arc.width=std::max(1,S(1));
  for(int radius:{171,160,106,53}){
    arc.color=color(radius==171?0xb89a5c:radius==160?0x3d3930:0x24231e);
    lv_draw_arc(ctx,&arc,&center,S(radius),0,359);
  }
  for(int i=0;i<60;++i){float a=i*.104719755f;int inner=i%5?165:162;
    line(ctx,CENTER+int(cosf(a)*S(inner)),CENTER+int(sinf(a)*S(inner)),CENTER+int(cosf(a)*S(168)),CENTER+int(sinf(a)*S(168)),i%5?0x584b33:0xc7ab73,std::max(1,S(1)));
  }
  for(int i=0;i<4;++i){float a=i*1.570796327f;line(ctx,CENTER+int(cosf(a)*S(120)),CENTER+int(sinf(a)*S(120)),CENTER+int(cosf(a)*S(153)),CENTER+int(sinf(a)*S(153)),0x302b21,std::max(1,S(1)));}
}
// One-time per-pixel dial for PSRAM boards: lit gold bezel, minute track,
// engraved range rings and crosshair graduations, a softly lit face and a
// warm halo under the emblem. Ordered dithering hides RGB565 banding in the
// dark gradients. About 0.2 Mpixel of float work when the radar page opens.
static void valleyBackdrop(lv_color_t* px,int res){
  using namespace valley;
  const float k=res/360.f;const int c=res/2;
  const float rEdge=Edge360*k,rSlate=Slate*k,rBezel=TrackOut*k,rTrack=TrackIn*k,rA=RingA*k,rB=RingB*k;
  const float logoY=S(17)+28.5f,glowR=46*k,minute=6.28318531f/60;
  static const uint8_t bayer[16]={0,8,2,10,12,4,14,6,3,11,1,9,15,7,13,5};
  auto cov=[](float d,float half){float v=half-d+.5f;return v<=0?0.f:v>=1?1.f:v;};
  auto band=[](float r,float lo,float hi){float a=r-lo+.5f,b=hi-r+.5f;a=a<0?0:a>1?1:a;b=b<0?0:b>1?1:b;return a*b;};
  for(int y=0;y<res;++y)for(int x=0;x<res;++x){
    const float dx=float(x-c),dy=float(y-c),r2=dx*dx+dy*dy;
    lv_color_t& out=px[y*res+x];
    if(r2>=(rEdge+1)*(rEdge+1)){out=lv_color_black();continue;}
    const float r=sqrtf(r2),inv=r>0?1/r:0;
    float R,G,B;auto over=[&](float cr,float cg,float cb,float a){if(a<=0)return;if(a>1)a=1;R+=(cr-R)*a;G+=(cg-G)*a;B+=(cb-B)*a;};
    // Light from the upper left lifts one side of every curved surface.
    const float light=(-0.7071f*dx-0.7071f*dy)*inv;
    // Enamel face.
    {const float t=r/rTrack,e=t>1?1:t,base=14-10*powf(e,1.6f),ray=1+0.09f*e*(-2*dx*dy)*(r2>0?1/r2:0);
      float shade=base*ray;if(r>150*k){const float s=(r-150*k)/(rTrack-150*k);shade*=1-0.5f*(s>1?1:s);}
      R=shade;G=shade*1.05f;B=shade*1.18f;}
    {const float gx=float(x-c),gy=float(y)-logoY,d=sqrtf(gx*gx+gy*gy)/glowR;if(d<1){const float g=(1-d*d);over(214,180,110,0.12f*g*g);}}
    if(r<rTrack+1){
      over(0x8d,0x7a,0x52,cov(fabsf(r-rA),0.5f)*0.32f);
      if(fabsf(r-rB)<2){const float a=atan2f(dy,dx),dash=fmodf(a+7,0.0523599f)/0.0523599f;over(0xb8,0x9a,0x5c,cov(fabsf(r-rB),0.5f)*(dash<0.62f?0.42f:0.10f));}
      over(0x6e,0x60,0x40,cov(fabsf(r-9*k),0.45f)*0.4f);
      // Crosshair hairlines with graduations every quarter ring.
      if(r>24*k&&r<150*k){
        const float ax=fabsf(dx),ay=fabsf(dy);
        over(0x8d,0x7a,0x52,(cov(ax,0.45f)+cov(ay,0.45f))*0.16f);
        const float step=rA/4;
        if(ax<3.2f*k){const float g=fabsf(ay-roundf(ay/step)*step);over(0xb8,0x9a,0x5c,cov(g,0.45f)*cov(ax,(fmodf(roundf(ay/step),4)==0?3.2f:1.9f)*k)*0.42f);}
        if(ay<3.2f*k){const float g=fabsf(ax-roundf(ax/step)*step);over(0xb8,0x9a,0x5c,cov(g,0.45f)*cov(ay,(fmodf(roundf(ax/step),4)==0?3.2f:1.9f)*k)*0.42f);}
      }
    }
    // Minute track.
    if(r>rTrack-1&&r<rBezel+1){
      over(8,8,9,band(r,rTrack,rBezel));
      over(0xb8,0x9a,0x5c,cov(fabsf(r-rTrack),0.5f)*0.55f);
      const float a=atan2f(dy,dx),m=a/minute,n=roundf(m);const int i=(int(n)%60+60)%60;
      const float off=fabsf(m-n)*minute*r;const bool major=i%5==0,cardinal=i%15==0;
      const float r1=(major?160.6f:164.2f)*k,r2b=(major?170:169.2f)*k,half=(cardinal?1.35f:major?1.05f:0.5f)*k;
      const float radial=band(r,r1,r2b);
      if(radial>0){const float a2=cov(off,half)*radial;if(major)over(cardinal?255:236,cardinal?228:202,cardinal?168:134,a2);else over(0x90,0x78,0x4a,a2*0.95f);}
    }
    // Machined bezel: dark bevel, convex gold band, slate rim.
    if(r>rBezel-1.2f*k){
      over(3,3,3,cov(fabsf(r-(rBezel-0.6f*k)),0.6f*k));
      if(r>rBezel-1){
        const float u=(r-rBezel)/(rSlate-rBezel),lit=0.5f+0.5f*light,shine=powf(lit,1.5f);
        const float hl=expf(-((u-0.28f)/0.17f)*((u-0.28f)/0.17f)),prof=0.72f+0.38f*hl-0.30f*u*u*u;
        float gr=(88+(248-88)*shine)*prof,gg=(70+(216-70)*shine)*prof,gb=(38+(146-38)*shine)*prof;
        if(gr>255)gr=255;if(gg>255)gg=255;if(gb>255)gb=255;
        over(gr,gg,gb,band(r,rBezel,rSlate));
      }
      if(r>rSlate-1){const float s=0.5f+0.5f*light;over(30+50*s,33+53*s,36+56*s,band(r,rSlate,rEdge));}
      if(r>rEdge-.5f){float a=rEdge+.5f-r;a=a<0?0:a>1?1:a;R*=a;G*=a;B*=a;}
    }
    const float d=(bayer[(y&3)*4+(x&3)]+0.5f)/16;
    auto q=[&](float v,int levels){int n=int(v*levels/255.f+d);return n<0?0:n>levels?levels:n;};
    out=lv_color_make(uint8_t(q(R,31)<<3),uint8_t(q(G,63)<<2),uint8_t(q(B,31)<<3));
  }
}
static FxSweepStyle valleySweepStyle(){
  using namespace valley;
  static float rings[3];rings[0]=S(int(RingA));rings[1]=S(int(RingB));rings[2]=TrackIn*SCREEN_RES/360.f;
  FxSweepStyle s;s.near=TrailNear;s.far=TrailFar;s.edge=Edge;s.core=Core;
  s.trailOpa=160;s.edgeOpa=150;s.coreOpa=245;s.span=1.45f;s.radius=TrackIn*SCREEN_RES/360.f-1;
  s.edgeWidth=std::max(2.f,4.2f*SCREEN_RES/360.f);s.coreWidth=SCREEN_RES>=400?1.1f:0.8f;s.lift=0.5f;
  s.rings=rings;s.ringCount=3;s.ringColor=RingLit;s.ringOpa=150;
  s.rim=166.5f*SCREEN_RES/360.f;s.rimColor=Spark;s.rimOpa=235;s.rimTail=0.55f;
  return s;
}
static void valleyRadarSweep(lv_draw_ctx_t* ctx,float angle){fxSweep(ctx,CENTER,CENTER,angle,valleySweepStyle());}
// Jeweled pivot with a slow breathing glow. Static size, so its repaint area
// is fixed: valleyCenterBounds().
static void valleyCenter(lv_draw_ctx_t* ctx,uint32_t now){
  const float b=dragon::radarBreath(now);
  disc(ctx,CENTER,CENTER,S(13),valley::Gold,uint8_t(10+26*b));
  disc(ctx,CENTER,CENTER,S(8),valley::Gold,uint8_t(22+40*b));
  disc(ctx,CENTER,CENTER,S(5),0x1a1408);
  disc(ctx,CENTER,CENTER,S(4),valley::GoldDeep);
  disc(ctx,CENTER,CENTER,S(3),valley::GoldPale);
  disc(ctx,CENTER-S(1),CENTER-S(1),std::max(1,S(1)),valley::Hot,uint8_t(150+105*b));
}
static dragon::RadarRect valleyCenterBounds(){return dragon::rectAround(CENTER,CENTER,S(13)+2);}
