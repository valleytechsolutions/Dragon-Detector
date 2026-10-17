// Native vector dials. Coordinates use a 360px design space. No per-frame
// framebuffer allocation; theme names belong to the gallery, never the dial.
static uint8_t watchFace=0;
static lv_obj_t* watchDial=nullptr;
static uint32_t watchFrameAt=0;
extern lv_img_dsc_t boot_dragon_logo;
extern "C" { LV_FONT_DECLARE(dragon_watch_digits); }
#define FWATCH (&dragon_watch_digits)
struct WatchPalette {const char* name;uint32_t bg,ink,accent,muted;bool analog;};
static const WatchPalette watches[]={
  {"DRAGON BALL / BWM",0x000000,0xfff0cf,0xffb000,0x76532b,false},
  {"VALLEYTECH",0x000000,0xe9edef,0xd9ba79,0x5d6265,true},
  {"JCMK MILITARY",0x000000,0xe6f0d0,0xa7b68b,0x475645,false},
  {nullptr,0,0,0,0,false}, // Retired ID: preserve later saved selections.
  {nullptr,0,0,0,0,false}, // Retired ID: preserve later saved selections.
  {nullptr,0,0,0,0,false}, // Retired ID: preserve later saved selections.
  {nullptr,0,0,0,0,false}, // Retired ID: preserve later saved selections.
  {nullptr,0,0,0,0,false}, // Retired ID: preserve later saved selections.
  {"BISCUIT SHOP",0x070707,0xf1f1f1,0xbfbfbf,0x555555,true},
  {"TOOOL",0xffffff,0x381821,0xc8102e,0xb4828d,true},
  {"CHURCH OF MALWARE",0x000000,0xffffff,0xffffff,0x777777,false},
  {"TOOOL DARK",0x0b0c0f,0xf4e9ed,0xf24e6d,0x785560,true},
  {"COVERT / MCNALLY",0x000000,0xf4f5f6,0xc5cdd3,0x535c64,true},
  {"AWOK",0x070d24,0xffffff,0xef75ff,0x61528b,true},
  {"BAPH0MAU5",0x0b070f,0xffefd2,0xd1a0ed,0x60446f,true},
  {"WDGWARS",0x000000,0xe9fcff,0x43e9f4,0x2a687b,true},
  {"GHOSTSTRATS",0x08070c,0xf3eef8,0xd7a5ff,0x5b456b,true},
  {"N3RDSEC",0x050505,0xf7f4ec,0xe7c775,0x6f5e3c,true},
  {"HALEHOUND",0x000000,0xffeef7,0xff8ad4,0x79516f,true},
  {"WAVESHARE",0x030803,0xf4f8ed,0xafd95e,0x50693c,true},
  {"M5STACK",0x000000,0xf0f7ff,0x68caff,0x3d6076,true},
  {"LAB5",0x070819,0xf4fbff,0x5ce8fa,0x6e468c,false},
  {"TALKING SASQUACH",0x040e0d,0xffe7bd,0x87dece,0x486b5e,true},
  {"NYAN DEVICES",0x080817,0xfff3fb,0xff82d9,0x77548b,true},
  {"DMAC",0x040d14,0xf5f8fc,0xff8acb,0x355e70,true},
  {"SEEED STUDIO",0x021116,0xf7fbef,0xa7d52c,0x46766c,true},
  {"DAGNAZTY",0xf7f7f5,0x101010,0x161616,0x73736d,true},
  {"0ct0's Pig",0x080808,0xffdbe9,0xff89b6,0x9b5c78,false},
  {"DEFLOCK",0x152131,0xf0f6fb,0xb1d8ec,0x56788f,false},
  {"CI / LOCKPICKINGLAWYER",0x030914,0xfff3d4,0xecc75b,0x8b7137,true},
  {"GHOSTBUSTERS",0x000000,0xffffff,0xff4b48,0x82414b,true},
  {"LOCKPICKERS UNITED",0x000000,0xffffff,0xe5e9ec,0x687177,true},
  {"HACKAZILLAREX",0x000000,0xffedcf,0xff7073,0x8e525c,false},
  {"m33k_t3ch",0x060818,0xfff2fc,0x4ae9ff,0x9677af,false}
};
static constexpr uint8_t WatchFaces[]={0,1,2,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32,33};
static constexpr unsigned WatchCount=sizeof(WatchFaces)/sizeof(WatchFaces[0]);
static bool watchFaceAllowed(unsigned face){for(auto id:WatchFaces)if(id==face)return true;return false;}
static unsigned watchFacePosition(){for(unsigned i=0;i<WatchCount;++i)if(WatchFaces[i]==watchFace)return i;return 0;}
static constexpr float WatchTau=6.28318530718f;
static void watchText(lv_draw_ctx_t* ctx,const char* text,int x,int y,int w,const lv_font_t* font,uint32_t ink){
  lv_draw_label_dsc_t d;lv_draw_label_dsc_init(&d);d.color=color(ink);d.font=font;d.align=LV_TEXT_ALIGN_CENTER;
  lv_area_t a{(lv_coord_t)S(x),(lv_coord_t)S(y),(lv_coord_t)S(x+w),(lv_coord_t)(S(y)+std::max(S(60),int(font->line_height)+S(4)))};lv_draw_label(ctx,&d,&a,text,nullptr);
}
static void watchLine(lv_draw_ctx_t* ctx,int x1,int y1,int x2,int y2,uint32_t ink,int width=1){line(ctx,S(x1),S(y1),S(x2),S(y2),ink,std::max(1,S(width)));}
static void watchCircle(lv_draw_ctx_t* ctx,int x,int y,int r,uint32_t ink,int width=1){
  lv_draw_arc_dsc_t d;lv_draw_arc_dsc_init(&d);d.color=color(ink);d.width=std::max(1,S(width));
  lv_point_t center{(lv_coord_t)S(x),(lv_coord_t)S(y)};lv_draw_arc(ctx,&d,&center,S(r),0,360);
}
static void watchRing(lv_draw_ctx_t* ctx,int r,uint32_t ink,int width=1){watchCircle(ctx,180,180,r,ink,width);}
static lv_point_t watchPolar(float turns,int radius,int x=180,int y=180){float a=turns*WatchTau;return {(lv_coord_t)(x+std::lround(radius*sinf(a))),(lv_coord_t)(y-std::lround(radius*cosf(a)))};}
static void watchRay(lv_draw_ctx_t* ctx,float turns,int inner,int outer,uint32_t ink,int width=1){auto a=watchPolar(turns,inner),b=watchPolar(turns,outer);watchLine(ctx,a.x,a.y,b.x,b.y,ink,width);}
static void watchPoly(lv_draw_ctx_t* ctx,const lv_point_t* points,unsigned count,uint32_t ink){
  lv_point_t scaled[8];if(count>8)return;for(unsigned i=0;i<count;++i)scaled[i]={(lv_coord_t)S(points[i].x),(lv_coord_t)S(points[i].y)};
  lv_draw_rect_dsc_t d;lv_draw_rect_dsc_init(&d);d.bg_color=color(ink);lv_draw_polygon(ctx,&d,scaled,count);
}
static void watchStar(lv_draw_ctx_t* ctx,int x,int y,int size,uint32_t ink){
  // Two convex diamonds, compatible with LVGL's polygon renderer.
  lv_point_t v[]={{(lv_coord_t)x,(lv_coord_t)(y-size)},{(lv_coord_t)(x+1),(lv_coord_t)y},{(lv_coord_t)x,(lv_coord_t)(y+size)},{(lv_coord_t)(x-1),(lv_coord_t)y}};
  lv_point_t h[]={{(lv_coord_t)(x-size),(lv_coord_t)y},{(lv_coord_t)x,(lv_coord_t)(y-1)},{(lv_coord_t)(x+size),(lv_coord_t)y},{(lv_coord_t)x,(lv_coord_t)(y+1)}};watchPoly(ctx,v,4,ink);watchPoly(ctx,h,4,ink);
}
static void watchLogo(lv_draw_ctx_t* ctx,const lv_img_dsc_t& logo,int y,uint32_t tint=0,bool recolor=false,bool circular=false,int shift=0){
  lv_draw_img_dsc_t d;lv_draw_img_dsc_init(&d);if(recolor){d.recolor=color(tint);d.recolor_opa=LV_OPA_COVER;}
  int x=(SCREEN_RES-logo.header.w)/2+S(shift);lv_area_t a{(lv_coord_t)x,(lv_coord_t)S(y),(lv_coord_t)(x+logo.header.w-1),(lv_coord_t)(S(y)+logo.header.h-1)};
  if(circular){
    lv_draw_mask_radius_param_t mask;lv_draw_mask_radius_init(&mask,&a,LV_RADIUS_CIRCLE,false);
    int16_t id=lv_draw_mask_add(&mask,nullptr);lv_draw_img(ctx,&d,&a,&logo);
    if(id>=0)lv_draw_mask_remove_id(id);
    // LVGL removal only detaches the mask; release its circle-cache reference too.
    lv_draw_mask_free_param(&mask);
  }
  else lv_draw_img(ctx,&d,&a,&logo);
}
static void watchScrew(lv_draw_ctx_t* ctx,int x,int y,uint32_t ink,uint32_t bg){disc(ctx,S(x),S(y),S(3),ink);watchLine(ctx,x-2,y+1,x+2,y-1,bg);}
static uint32_t watchShade(uint32_t ink,int change){return (uint32_t(constrain(int(ink>>16)+change,0,255))<<16)|(uint32_t(constrain(int((ink>>8)&255)+change,0,255))<<8)|uint32_t(constrain(int(ink&255)+change,0,255));}
static void watchGear(lv_draw_ctx_t* ctx,int x,int y,int r,int teeth,float phase,uint32_t ink,uint32_t dark){
  uint32_t shine=watchShade(ink,57),shade=watchShade(ink,-30);
  for(int tooth=0;tooth<teeth;++tooth){lv_point_t points[4];for(int j=0;j<4;++j)points[j]=watchPolar(phase+(tooth+j/4.f)/teeth,j==1||j==2?r:r-5,x,y);watchPoly(ctx,points,4,shade);watchLine(ctx,points[1].x,points[1].y,points[2].x,points[2].y,shine);}
  watchCircle(ctx,x,y,r-5,ink,3);watchCircle(ctx,x,y,r-4,shine);watchCircle(ctx,x,y,r-8,shade);watchCircle(ctx,x,y,r-10,ink);
  for(int i=0;i<6;++i){auto a=watchPolar(phase+i/6.f,8,x,y),b=watchPolar(phase+i/6.f,r-10,x,y);watchLine(ctx,a.x,a.y,b.x,b.y,ink,4);watchLine(ctx,a.x+1,a.y,b.x+1,b.y,shine);
    auto c=watchPolar(phase+(i+.5f)/6.f,r-13,x,y);disc(ctx,S(c.x),S(c.y),std::max(1,S(1)),shine);
  }
  for(int i=0;i<teeth*2;++i){auto a=watchPolar(phase+float(i)/(teeth*2),r-7,x,y),b=watchPolar(phase+float(i)/(teeth*2),r-8,x,y);watchLine(ctx,a.x,a.y,b.x,b.y,shine);}
  disc(ctx,S(x),S(y),S(8),shade);watchCircle(ctx,x,y,8,shine);watchCircle(ctx,x,y,5,ink,2);watchScrew(ctx,x,y,shine,dark);
}
static void watchCurve(lv_draw_ctx_t* ctx,lv_point_t a,lv_point_t b,lv_point_t c,lv_point_t d,uint32_t ink,int width=1){
  auto previous=a;for(int i=1;i<=24;++i){float t=i/24.f,u=1-t;lv_point_t next{(lv_coord_t)std::lround(u*u*u*a.x+3*u*u*t*b.x+3*u*t*t*c.x+t*t*t*d.x),(lv_coord_t)std::lround(u*u*u*a.y+3*u*u*t*b.y+3*u*t*t*c.y+t*t*t*d.y)};watchLine(ctx,previous.x,previous.y,next.x,next.y,ink,width);previous=next;}
}
static void watchValley(lv_draw_ctx_t* ctx,const WatchPalette& p){
  // Quiet enamel dial, a fine chapter ring and the original VTS hallmark.
  // This face intentionally has no gears, bridges or animated mechanism.
  disc(ctx,CENTER,CENTER,S(119),0x080a0b);watchRing(ctx,120,0x3d3930);watchRing(ctx,117,0x1b2024);
  watchLogo(ctx,radar_logo_valleytech,77,p.accent,true);
  watchLine(ctx,165,126,195,126,0x6d5b38);watchStar(ctx,180,132,3,p.accent);
}
static void watchPins(lv_draw_ctx_t* ctx,int y,uint32_t ink,uint32_t muted){
  // Five pin chambers, individual spring coils and a fine shear line.
  for(int i=0;i<5;++i){int x=140+i*20,tip=y+18+(i%3)*3;
    watchLine(ctx,x-5,y,x-5,y+36,muted);watchLine(ctx,x+5,y,x+5,y+36,muted);
    for(int n=0;n<4;++n)watchLine(ctx,x-3,y+2+n*3,x+3,y+4+n*3,muted);
    watchLine(ctx,x,tip,x,y+31,ink,4);watchCircle(ctx,x,y+32,2,ink);
  }
  watchLine(ctx,127,y+36,233,y+36,ink);watchLine(ctx,127,y+39,233,y+39,muted);
}
static void watchToool(lv_draw_ctx_t* ctx,const WatchPalette& p){
  const bool dark=watchFace==11;
  watchRing(ctx,123,dark?0x39212c:0xf2dfe3);watchRing(ctx,120,dark?0x22171e:0xf7eef0);
  watchLogo(ctx,radar_logo_toool,75,0,false,dark);
  watchPins(ctx,210,dark?0xb63451:0xba5266,dark?0x462935:0xead2d8);
  // Small machined case screws, separate from the Arabic hour chapter.
  for(float angle:{.125f,.375f,.625f,.875f}){auto at=watchPolar(angle,169);watchScrew(ctx,at.x,at.y,p.accent,p.bg);}
}
static void watchMechanism(lv_draw_ctx_t* ctx,const WatchPalette& p){
  const bool mono=watchFace==8;
  uint32_t metal=mono?0x757575:0x9b6c39,steel=mono?0x4b4b4b:0x664d36;
  // A common whole-turn period keeps all meshed gear ratios continuous.
  float phase=(watchFrameAt%2520000)/30000.f;
  disc(ctx,CENTER,CENTER,S(119),mono?0x101010:0x21180f);
  for(int i=0;i<9;++i){int x=93+i*21;watchLine(ctx,x,137,x,229,mono?0x1b1b1b:0x2d2114);}
  watchGear(ctx,122,199,40,16,phase,metal,p.bg);watchGear(ctx,190,220,31,12,-phase*16/12,steel,p.bg);watchGear(ctx,242,181,35,14,phase*16/14,metal,p.bg);
  watchCircle(ctx,180,180,116,steel);watchCircle(ctx,180,180,112,mono?0x2a2a2a:0x363024);
  watchLine(ctx,94,164,155,184,steel,7);watchLine(ctx,205,186,267,150,steel,7);
  watchLine(ctx,94,162,155,182,metal);watchLine(ctx,205,184,267,148,metal);
  for(auto point:{lv_point_t{94,164},lv_point_t{155,184},lv_point_t{205,186},lv_point_t{267,150}})watchScrew(ctx,point.x,point.y,p.ink,p.bg);
  if(mono)watchLogo(ctx,radar_logo_biscuit,77);else {watchGear(ctx,180,99,19,12,0,p.accent,p.bg);watchCircle(ctx,180,99,12,p.ink);}
}
static void watchFairytale(lv_draw_ctx_t* ctx,const WatchPalette& p){
  disc(ctx,CENTER,CENTER,S(120),0x10241d);watchRing(ctx,123,0x486448);watchRing(ctx,118,0x314c36);
  for(int side:{-1,1})for(int i=0;i<14;++i){float a=.12f+i*.020f;auto stem=watchPolar(side*a,114),tip=watchPolar(side*(a+.010f),106),next=watchPolar(side*(a+.020f),114);
    watchLine(ctx,stem.x,stem.y,next.x,next.y,0x708861);auto leaf=watchPolar(side*(a+.010f),i%2?101:122);lv_point_t points[]={stem,leaf,tip};watchPoly(ctx,points,3,i%2?0x7e9a73:0xb9aa75);if(i%3==0)disc(ctx,S(stem.x),S(stem.y),S(2),p.accent);
  }
  watchLine(ctx,165,112,195,112,p.accent,2);watchLine(ctx,166,111,166,96,p.accent,3);watchLine(ctx,194,111,194,96,p.accent,3);watchLine(ctx,180,112,180,85,p.ink,4);
  lv_point_t roof1[]={{159,96},{166,84},{173,96}},roof2[]={{187,96},{194,84},{201,96}},roof3[]={{172,86},{180,73},{188,86}};
  watchPoly(ctx,roof1,3,p.accent);watchPoly(ctx,roof2,3,p.accent);watchPoly(ctx,roof3,3,p.ink);
  disc(ctx,S(180),S(103),S(3),0x10241d);disc(ctx,S(218),S(86),S(8),p.accent);disc(ctx,S(221),S(83),S(7),0x10241d);
  watchStar(ctx,145,90,4,p.ink);watchStar(ctx,212,117,3,p.accent);
  for(int side:{-1,1}){watchLine(ctx,180,239,180+side*24,230,0x789469);watchLine(ctx,180+side*24,230,180+side*34,237,0x789469);watchStar(ctx,180+side*36,239,4,p.accent);}
}
static void watchGalaxy(lv_draw_ctx_t* ctx,const WatchPalette& p){
  disc(ctx,CENTER,CENTER,S(121),0x0b1028);disc(ctx,S(194),S(176),S(96),0x10132d);disc(ctx,S(209),S(164),S(68),0x14152f);
  for(unsigned i=0;i<65;++i){unsigned n=i*2654435761u+719u;int x=80+int(n%201),y=80+int((n>>12)%201);if((x-180)*(x-180)+(y-180)*(y-180)>115*115)continue;disc(ctx,S(x),S(y),1,i%4?0x4e5c8a:0xb3bddd);}
  const lv_point_t stars[]={{98,176},{119,151},{133,173},{117,202},{146,220}};
  for(unsigned i=1;i<5;++i)watchLine(ctx,stars[i-1].x,stars[i-1].y,stars[i].x,stars[i].y,0x47577f);for(auto s:stars)watchStar(ctx,s.x,s.y,3,p.accent);
  disc(ctx,S(180),S(98),S(17),0xa7b0d9);disc(ctx,S(186),S(94),S(15),0x505c86);
  for(int i=0;i<48;++i){float a=i*WatchTau/48,b=(i+1)*WatchTau/48;int x=32*cosf(a),y=9*sinf(a),u=32*cosf(b),v=9*sinf(b);watchLine(ctx,180+x,98+y-x/3,180+u,98+v-u/3,0xc3b4e8);}
  watchStar(ctx,232,137,7,p.ink);watchStar(ctx,215,228,5,p.accent);watchStar(ctx,151,96,4,p.ink);watchCircle(ctx,180,180,122,0x424c75);
}
static void watchCyber(lv_draw_ctx_t* ctx,const WatchPalette& p){
  watchRing(ctx,122,0x21314a);watchRing(ctx,118,0x162438);
  if(!p.analog){
    watchStar(ctx,180,99,18,p.accent);watchCircle(ctx,180,99,23,0x7952a1);watchCircle(ctx,180,99,8,p.bg,3);
    for(int side:{-1,1}){int x=180+side*135;watchLine(ctx,x,146,x,213,p.muted);watchLine(ctx,x,146,x-side*12,146,p.accent);watchLine(ctx,x,213,x-side*12,213,p.accent);}
    return;
  }
  for(int side:{-1,1})for(int i=0;i<3;++i){int x=180+side*(67+i*12);watchLine(ctx,x,116,x,142-i*8,p.muted);watchLine(ctx,x,142-i*8,x+side*14,156-i*8,p.muted);disc(ctx,S(x+side*14),S(156-i*8),S(2),p.accent);}
  watchStar(ctx,180,99,18,p.accent);watchCircle(ctx,180,99,23,0x7952a1);watchCircle(ctx,180,99,8,p.bg,3);
  for(int i=0;i<7;++i){watchLine(ctx,103+i*5,221,103+i*5,228+i%3*4,0x5b4d80);watchLine(ctx,227+i*5,221,227+i*5,228+i%3*4,p.muted);}
}
static void watchRoman(lv_draw_ctx_t* ctx,const char* text,int cx,int cy,int height,uint32_t ink,uint32_t shadow){
  // Slender engraved Roman capitals with serifs, not a sans-serif UI font.
  int width=0;for(const char* p=text;*p;++p)width+=(*p=='I'?height/3:height*2/3)+3;int x=cx-(width-3)/2,y=cy-height/2;
  for(const char* p=text;*p;++p){int w=*p=='I'?height/3:height*2/3;
    auto stroke=[&](int a,int b,int c,int d,int weight){watchLine(ctx,a+1,b+1,c+1,d+1,shadow,weight);watchLine(ctx,a,b,c,d,ink,weight);};
    if(*p=='I'){stroke(x+w/2,y,x+w/2,y+height,2);stroke(x,y,x+w,y,1);stroke(x,y+height,x+w,y+height,1);}
    else if(*p=='V'){stroke(x,y,x+w/2,y+height,2);stroke(x+w/2,y+height,x+w,y,1);stroke(x-2,y,x+3,y,1);stroke(x+w-3,y,x+w+2,y,1);}
    else {stroke(x,y,x+w,y+height,2);stroke(x+w,y,x,y+height,1);for(int edge:{0,height}){stroke(x-2,y+edge,x+3,y+edge,1);stroke(x+w-3,y+edge,x+w+2,y+edge,1);}}
    x+=w+3;
  }
}
static void watchIndices(lv_draw_ctx_t* ctx,const WatchPalette& p){
  static const char* roman[]={"XII","I","II","III","IV","V","VI","VII","VIII","IX","X","XI"};bool classical=watchFace==0||watchFace==5||watchFace==6||watchFace==7||watchFace==14||watchFace==18;
  for(int i=0;i<60;++i)watchRay(ctx,i/60.f,i%5?156:150,161,i%5?p.muted:p.accent,i%5?1:2);
  if(!p.analog)return;
  for(int i=0;i<12;++i){auto at=watchPolar(i/12.f,136);char number[4];snprintf(number,sizeof(number),"%d",i?i:12);
    if(classical)watchRoman(ctx,roman[i],at.x,at.y,i%3?18:21,p.ink,p.muted);
    else if(watchFace==1||watchFace==9||watchFace==11||watchFace==17||watchFace==25)watchText(ctx,number,at.x-25,at.y-11,50,F20,i%3?p.ink:p.accent);
    else if(i%3==0)watchText(ctx,number,at.x-23,at.y-8,46,F16,p.ink);
    else {watchRay(ctx,i/12.f,130,141,p.ink,2);if(watchFace==8)watchRay(ctx,i/12.f,130,139,p.bg);}
  }
}
static void watchHand(lv_draw_ctx_t* ctx,float turns,int length,int tail,int width,uint32_t ink){watchRay(ctx,turns,-tail,length,ink,width);}
static void watchSpear(lv_draw_ctx_t* ctx,float turns,int length,int width,uint32_t ink,uint32_t inset){
  float a=turns*WatchTau,dx=sinf(a),dy=-cosf(a);lv_point_t points[4];const int along[]={-15,12,length,12},across[]={0,width,0,-width};
  for(int i=0;i<4;++i)points[i]={(lv_coord_t)std::lround(180+along[i]*dx+across[i]*-dy),(lv_coord_t)std::lround(180+along[i]*dy+across[i]*dx)};
  watchPoly(ctx,points,4,ink);watchRay(ctx,turns,15,length-17,inset);
}
static lv_point_t watchHandPoint(float turns,int along,int across){float a=turns*WatchTau;return {(lv_coord_t)std::lround(180+along*sinf(a)+across*cosf(a)),(lv_coord_t)std::lround(180-along*cosf(a)+across*sinf(a))};}
static void watchDauphine(lv_draw_ctx_t* ctx,float turns,int length,int width,uint32_t light,uint32_t shade){
  lv_point_t border[]={watchHandPoint(turns,-16,0),watchHandPoint(turns,23,width+2),watchHandPoint(turns,length+2,0),watchHandPoint(turns,23,-width-2)};
  watchPoly(ctx,border,4,0x000000);
  auto tail=watchHandPoint(turns,-14,0),tip=watchHandPoint(turns,length,0);
  lv_point_t left[]={tail,watchHandPoint(turns,23,width),tip},right[]={tail,tip,watchHandPoint(turns,23,-width)};
  watchPoly(ctx,left,3,shade);watchPoly(ctx,right,3,light);watchRay(ctx,turns,12,length-12,light);
}
static void watchOrnateHand(lv_draw_ctx_t* ctx,float turns,int length,uint32_t ink,uint32_t bg){
  watchHand(ctx,turns,length,16,4,bg);watchHand(ctx,turns,length,16,2,ink);
  lv_point_t shell[]={watchHandPoint(turns,length,0),watchHandPoint(turns,length-15,5),watchHandPoint(turns,length-34,0),watchHandPoint(turns,length-15,-5)};watchPoly(ctx,shell,4,ink);
  lv_point_t hollow[]={watchHandPoint(turns,length-7,0),watchHandPoint(turns,length-16,2),watchHandPoint(turns,length-26,0),watchHandPoint(turns,length-16,-2)};watchPoly(ctx,hollow,4,bg);
  int base=length-39;
  for(int side:{-1,1})watchCurve(ctx,watchHandPoint(turns,base-10,0),watchHandPoint(turns,base+7,side*9),watchHandPoint(turns,base+12,side*8),watchHandPoint(turns,base+7,side*3),ink);
  auto counter=watchHandPoint(turns,-13,0);watchCircle(ctx,counter.x,counter.y,3,ink);
}
static void watchBinaryBank(lv_draw_ctx_t* ctx,unsigned value,unsigned bits,float degrees,float step,bool valid){
  for(unsigned i=0;i<bits;++i){unsigned weight=1u<<(bits-1-i);float turn=(degrees+i*step)/360.f;auto at=watchPolar(turn,157),inside=watchPolar(turn,139);
    bool lit=valid&&(value&weight);watchLine(ctx,inside.x,inside.y,at.x,at.y,lit?0x999999:0x252525);
    disc(ctx,S(at.x),S(at.y),S(11),lit?0xffffff:0x151515);watchCircle(ctx,at.x,at.y,11,lit?0xffffff:0x606060);
    char number[4];snprintf(number,sizeof(number),"%u",weight);watchText(ctx,number,at.x-12,at.y-6,24,F12,lit?0x000000:0x9c9c9c);
  }
}
static void watchBinary(lv_draw_ctx_t* ctx,const dragon::Calendar& d,bool valid){
  // The complete emblem occupies the center; weighted bits stay outside it.
  watchLogo(ctx,watch_logo_church,54);watchRing(ctx,175,0x656565);watchRing(ctx,172,0x252525);
  watchBinaryBank(ctx,dragon::hour12(d.hour),4,-42,28,valid);
  watchBinaryBank(ctx,d.minute,6,70,18,valid);watchBinaryBank(ctx,d.second,6,200,18,valid);
  watchText(ctx,"H",160,6,40,F12,0xffffff);watchText(ctx,valid?(d.hour>=12?"PM":"AM"):"SET",162,36,36,F12,0xbdbdbd);
  auto m=watchPolar(60.f/360,164),s=watchPolar(300.f/360,164);
  watchText(ctx,"M",m.x-10,m.y-6,20,F12,0xffffff);watchText(ctx,"S",s.x-10,s.y-6,20,F12,0xffffff);
  for(unsigned i=0;i<WatchCount;++i)disc(ctx,S(180+(int(i)*2-int(WatchCount-1))*3),S(338),std::max(1,S(i==watchFacePosition()?2:1)),i==watchFacePosition()?0xffffff:0x555555);
}
#include "pick_hands.inc"
template<size_t N,size_t T> static void watchPick(lv_draw_ctx_t* ctx,float turns,const lv_point_t (&profile)[N],const uint8_t (&triangles)[T][3],bool minute,float scale=1.f){
  auto point=[turns,scale](int along,int across){return watchHandPoint(turns,lroundf(along*scale),lroundf(across*scale));};
  lv_point_t points[N];for(unsigned i=0;i<N;++i)points[i]=point(profile[i].x,profile[i].y);
  // Solid silver silhouettes, dark separation and polished edges remain clear
  // when the hands cross. No transformed bitmap or per-frame allocation.
  for(unsigned i=0;i<N;++i){auto a=points[i],b=points[(i+1)%N];watchLine(ctx,a.x,a.y,b.x,b.y,0x000000,4);}
  for(auto& triangle:triangles){lv_point_t p[]={points[triangle[0]],points[triangle[1]],points[triangle[2]]};watchPoly(ctx,p,3,minute?0xaebcc6:0x82919c);}
  for(unsigned i=0;i<N;++i){auto a=points[i],b=points[(i+1)%N];watchLine(ctx,a.x,a.y,b.x,b.y,i<N/2?0xffffff:0x66727c);}
  const int end=minute?73:40;
  for(int offset=-5;offset<=5;offset+=2){auto a=point(9,offset),b=point(end,offset);watchLine(ctx,a.x,a.y,b.x,b.y,offset<0?0xc8d0d6:0x9aa8b1);}
  for(int along:{14,end-5}){auto p=point(along,0);disc(ctx,S(p.x),S(p.y),std::max(1,S(2)),0x29333b);watchCircle(ctx,p.x,p.y,2,0xe0e6ea);}
  // A small door-shaped engraving follows the metal handle.
  int mark=(14+end)/2;auto a=point(mark-6,-3),b=point(mark+6,-3),c=point(mark+6,3),d=point(mark-6,3);
  watchLine(ctx,a.x,a.y,b.x,b.y,0x43525d);watchLine(ctx,b.x,b.y,c.x,c.y,0x43525d);watchLine(ctx,d.x,d.y,c.x,c.y,0x43525d);
}
static void watchCovert(lv_draw_ctx_t* ctx,const dragon::Calendar& d,bool valid){
  const auto& p=watches[12];watchRing(ctx,176,0x444d55);watchRing(ctx,173,0xc4cbd0);watchRing(ctx,169,0x22292e);watchRing(ctx,119,0x1b2228);
  for(int i=0;i<120;++i)watchRay(ctx,i/120.f,171,172,0x434e56);
  // Pin chambers sit below the paired logos.
  for(int i=0;i<6;++i){int x=130+i*20;watchLine(ctx,x-5,133,x-5,166,0x293139);watchLine(ctx,x+5,133,x+5,166,0x293139);
    for(int j=0;j<4;++j)watchLine(ctx,x-3,134+j*3,x+3,136+j*3,0x3a444d);
    watchLine(ctx,x,151+i%3*3,x,164,0x58656f,3);
  }
  watchLine(ctx,115,168,245,168,0x59636b);watchLine(ctx,115,171,245,171,0x1d252c);
  watchLogo(ctx,watch_logo_ci,65,0,false,false,-43);watchLogo(ctx,watch_logo_mcnally,65,0,false,false,25);
  for(int i=0;i<60;++i)watchRay(ctx,i/60.f,i%5?156:149,161,i%5?0x69747d:0xe7ecef,i%5?1:2);
  for(int i=0;i<12;++i){auto at=watchPolar(i/12.f,137);char value[3];snprintf(value,sizeof(value),"%02d",i?i:12);watchText(ctx,value,at.x-20,at.y-8,40,F16,i%3?0xaeb8c0:0xffffff);}
  for(float angle:{.125f,.375f,.625f,.875f}){auto at=watchPolar(angle,166);watchScrew(ctx,at.x,at.y,0xb9c2c9,0x000000);}
  if(valid){
    watchPick(ctx,(d.hour%12+d.minute/60.f+d.second/3600.f)/12.f,pickShortHook,pickShortHookTriangles,false);
    watchPick(ctx,(d.minute+d.second/60.f)/60.f,pickMcTickler,pickMcTicklerTriangles,true);
    watchRay(ctx,d.second/60.f,143,153,0xffffff,2);
    disc(ctx,CENTER,CENTER,S(7),0x000000);watchCircle(ctx,180,180,6,0xe2e8ec,2);watchScrew(ctx,180,180,0x7e8b95,0x10161b);
  }
  // Digital window is drawn after the hands to remain legible at every angle.
  lv_draw_rect_dsc_t panel;lv_draw_rect_dsc_init(&panel);panel.bg_color=color(0x070a0d);panel.border_color=color(0x505b64);panel.border_width=1;panel.radius=S(4);
  lv_area_t window{(lv_coord_t)S(125),(lv_coord_t)S(276),(lv_coord_t)S(235),(lv_coord_t)S(303)};lv_draw_rect(ctx,&panel,&window);
  char text[20];if(valid)snprintf(text,sizeof(text),"%02d:%02d",d.hour,d.minute);else strcpy(text,"--:--");watchText(ctx,text,128,279,104,F20,0xf4f7f9);
  for(unsigned i=0;i<WatchCount;++i)disc(ctx,S(180+(int(i)*2-int(WatchCount-1))*3),S(338),std::max(1,S(i==watchFacePosition()?2:1)),i==watchFacePosition()?p.accent:p.muted);
}
#include "community_dials.inc"
#include "octopig.inc"
#include "lpl_watch.inc"
#include "containment_watch.inc"
#include "cyber_runner_watch.inc"
static void watchDraw(lv_event_t* e){
  auto* ctx=lv_event_get_draw_ctx(e);const auto& p=watches[watchFace];dragon::Calendar d;clockLocal(d);bool valid=wallTime()!=0;char text[64];
  if(watchFace==0){
    // Reuse the startup asset: the emblem remains unobscured by hands or text.
    watchLogo(ctx,boot_dragon_logo,28);
    for(int i=0;i<60;++i)watchRay(ctx,i/60.f,173,i%5?175:177,p.muted);
    if(valid){auto s=watchPolar(d.second/60.f,175);disc(ctx,S(s.x),S(s.y),S(3),p.accent);}
    if(valid)snprintf(text,sizeof(text),"%02d:%02d",d.hour,d.minute);else strcpy(text,"--:--");
    watchText(ctx,text,70,250,220,FTIMER,p.ink);
    if(valid)snprintf(text,sizeof(text),"%04d-%02d-%02d",d.year,d.month,d.day);else strcpy(text,"SYNC IN SETTINGS");
    watchText(ctx,text,70,294,220,F12,p.accent);return;
  }
  if(watchFace==32||watchFace==33){watchRunnerCyber(ctx,d,valid);return;}
  if(watchFace==30||watchFace==31){watchContainment(ctx,d,valid);return;}
  if(watchFace==29){watchLpl(ctx,d,valid);return;}
  if(watchFace==28){watchDeflock(ctx,d,valid);return;}
  if(watchFace==27){watchPig(ctx,d,valid);return;}
  if(watchFace==10){watchBinary(ctx,d,valid);return;}
  if(watchFace==12){watchCovert(ctx,d,valid);return;}
  watchRing(ctx,177,p.muted,2);watchRing(ctx,173,p.accent,2);watchRing(ctx,168,p.muted);watchRing(ctx,164,p.muted);for(int i=0;i<120;++i)watchRay(ctx,i/120.f,170,171,p.muted);

  if(watchFace==13)watchAwok(ctx);else if(watchFace==14)watchBaph(ctx);else if(watchFace==15)watchWdg(ctx);else if(watchFace==16)watchGhost(ctx);else if(watchFace==17)watchN3rd(ctx);else if(watchFace==18)watchHale(ctx);else if(watchFace==19)watchWave(ctx);else if(watchFace==20)watchM5(ctx);else if(watchFace==21)watchLab(ctx);else if(watchFace==22)watchSasquach(ctx);else if(watchFace==23)watchNyan(ctx);else if(watchFace==24)watchDmac(ctx);else if(watchFace==25)watchSeeed(ctx);else if(watchFace==26)watchDag(ctx);else if(watchFace==9||watchFace==11)watchToool(ctx,p);else if(watchFace==1)watchValley(ctx,p);else if(watchFace==8)watchMechanism(ctx,p);
  else if(watchFace==0){watchRing(ctx,119,0x332c23);watchRing(ctx,116,0x211e19);for(int i=0;i<48;++i)watchRay(ctx,i/48.f,109,112,0x2e2921);watchLogo(ctx,radar_logo,78);}
  else {watchLogo(ctx,radar_logo_marauder,75);for(int side:{-1,1}){int x=180+side*135;watchLine(ctx,x,146,x,213,p.muted);watchLine(ctx,x,146,x-side*12,146,p.accent);watchLine(ctx,x,213,x-side*12,213,p.accent);}}
  watchIndices(ctx,p);
  if(p.analog){
    if(valid){if(watchFace==20){snprintf(text,sizeof(text),"%d:%02d %s",dragon::hour12(d.hour),d.minute,d.hour>=12?"PM":"AM");watchText(ctx,text,102,71,156,F20,p.accent);}
      snprintf(text,sizeof(text),"%02d / %02d",d.month,d.day);watchText(ctx,text,112,258,136,F12,p.accent);
      float hours=(d.hour%12+d.minute/60.f+d.second/3600.f)/12.f,minutes=(d.minute+d.second/60.f)/60.f;
      if(watchFace==1){watchDauphine(ctx,hours,94,10,0xf5d996,0x9d773b);watchDauphine(ctx,minutes,122,7,0xf3f6f7,0x87959d);}
      else if(watchFace==9){watchDauphine(ctx,hours,89,9,0xcf1434,0x8c162c);watchDauphine(ctx,minutes,121,6,0x78626b,0x351922);}
      else if(watchFace==11){watchDauphine(ctx,hours,89,9,0xf24e6d,0x9f1e38);watchDauphine(ctx,minutes,121,6,0xf4e9ed,0x95858f);}
      else if(watchFace==13){watchDauphine(ctx,hours,88,9,0xffb8fa,0xb53ce5);watchDauphine(ctx,minutes,120,6,0xffffff,0x46d7e4);}
      else if(watchFace==16){watchDauphine(ctx,hours,91,9,0xe3b9ff,0x73508c);watchDauphine(ctx,minutes,121,6,0xffffff,0x92909f);}
      else if(watchFace==18){watchDauphine(ctx,hours,89,9,0xffd0ea,0xb34287);watchDauphine(ctx,minutes,121,6,0xffffff,0x9d67ca);}
      else if(watchFace==23){watchDauphine(ctx,hours,90,9,0xffb2e7,0xb94094);watchDauphine(ctx,minutes,120,6,0xe0f8ff,0x208fca);}
      else if(watchFace==19){watchDauphine(ctx,hours,91,9,0xd8f79c,0x63862d);watchDauphine(ctx,minutes,121,6,0xffffff,0x8eaaa1);}
      else if(watchFace==20){watchDauphine(ctx,hours,91,9,0x92ddff,0x287cac);watchDauphine(ctx,minutes,121,6,0xffffff,0x99abb7);}
      else if(watchFace==25){watchDauphine(ctx,hours,90,8,0xc3e966,0x648a2c);watchDauphine(ctx,minutes,121,6,0xffffff,0x719b9f);}
      else if(watchFace==26){watchDauphine(ctx,hours,88,8,0x343434,0x090909);watchDauphine(ctx,minutes,120,5,0x656560,0x151515);}
      else if(watchFace==24){dmacPick(ctx,hours,false);dmacPick(ctx,minutes,true);}
      else if(watchFace==22){sasquachArm(ctx,hours,82,false);sasquachArm(ctx,minutes,117,true);}
      else if(watchFace==17){watchDauphine(ctx,hours,91,10,0xffe5a3,0x9e7735);watchDauphine(ctx,minutes,121,7,0xffffff,0x9b9ca1);}
      else if(watchFace==15){watchDauphine(ctx,hours,88,8,0xec87f0,0x7a246e);watchDauphine(ctx,minutes,120,5,0xe9fcff,0x32a9bc);}
      else if(watchFace==0||watchFace==5||watchFace==6||watchFace==14){watchOrnateHand(ctx,hours,83,p.ink,p.bg);watchOrnateHand(ctx,minutes,117,p.accent,p.bg);}
      else {watchSpear(ctx,hours,84,7,p.bg,p.bg);watchSpear(ctx,minutes,118,5,p.bg,p.bg);watchSpear(ctx,hours,82,5,p.ink,p.accent);watchSpear(ctx,minutes,116,3,p.ink,p.accent);}
      if(watchFace==22||watchFace==24||watchFace==26){auto second=watchPolar(d.second/60.f,163);disc(ctx,S(second.x),S(second.y),S(2),p.accent);}
      else {watchHand(ctx,d.second/60.f,131,25,1,p.accent);auto counter=watchPolar(d.second/60.f,-23);watchCircle(ctx,counter.x,counter.y,3,p.accent);}
      disc(ctx,CENTER,CENTER,S(7),p.bg);disc(ctx,CENTER,CENTER,S(5),p.accent);disc(ctx,CENTER,CENTER,S(2),p.ink);
    }else watchText(ctx,"SET YOUR TIME",70,169,220,F16,p.ink);
  }else{
    if(valid)snprintf(text,sizeof(text),"%02d:%02d",watchFace==2?d.hour:dragon::hour12(d.hour),d.minute);else strcpy(text,"--:--");watchText(ctx,text,45,144,270,FWATCH,p.ink);
    if(valid){if(watchFace==2)snprintf(text,sizeof(text),"%02d",d.second);else snprintf(text,sizeof(text),"%02d  %s",d.second,d.hour>=12?"PM":"AM");}else strcpy(text,"--");watchText(ctx,text,115,220,130,F16,p.accent);
    watchLine(ctx,137,247,223,247,p.muted);if(valid)snprintf(text,sizeof(text),"%02d / %02d / %04d",d.month,d.day,d.year);else strcpy(text,"SYNC IN SETTINGS");watchText(ctx,text,70,260,220,F12,p.accent);
    if(watchFace==2)for(int x:{83,277})for(int y:{108,245})watchScrew(ctx,x,y,p.muted,p.bg);
  }
  for(unsigned i=0;i<WatchCount;++i)disc(ctx,S(180+(int(i)*2-int(WatchCount-1))*3),S(338),std::max(1,S(i==watchFacePosition()?2:1)),i==watchFacePosition()?p.accent:p.muted);
}
static void chooseWatch(int delta){watchFace=WatchFaces[(watchFacePosition()+WatchCount+delta)%WatchCount];settings.putUChar("watch-face",watchFace);pending=Clock;page=Boot;}
static void watchGesture(lv_event_t*){
  auto* input=lv_indev_get_act();if(!input)return;auto direction=lv_indev_get_gesture_dir(input);if(direction==LV_DIR_LEFT)chooseWatch(1);else if(direction==LV_DIR_RIGHT)chooseWatch(-1);else if(direction==LV_DIR_BOTTOM)go(Menu);lv_indev_wait_release(input);
}
static void buildWatch(){
  dismissNotice();const auto& p=watches[watchFace];lv_obj_set_style_bg_color(root,color(p.bg),0);watchFrameAt=millis();
  watchDial=lv_obj_create(root);lv_obj_remove_style_all(watchDial);lv_obj_set_size(watchDial,SCREEN_RES,SCREEN_RES);lv_obj_clear_flag(watchDial,LV_OBJ_FLAG_SCROLLABLE|LV_OBJ_FLAG_GESTURE_BUBBLE);
  lv_obj_add_event_cb(watchDial,watchDraw,LV_EVENT_DRAW_MAIN,nullptr);lv_obj_add_event_cb(watchDial,watchGesture,LV_EVENT_GESTURE,nullptr);
  if(watchFace==27){pigFrameAt=millis();pigNextAt=millis();lv_obj_add_event_cb(watchDial,pigTap,LV_EVENT_CLICKED,nullptr);}
  bool compact=watchFace==0||watchFace==10||watchFace==12||watchFace==27||watchFace==28||watchFace>=29;int y=watchFace==0?316:watchFace==27?315:(watchFace==12||watchFace==28||watchFace>=29)?326:watchFace==10?317:279,h=(watchFace==0||watchFace==12||watchFace==28||watchFace>=29)?21:27;
  auto* b=button(watchDial,watchFace==27?"[+]":LV_SYMBOL_SETTINGS,watchFace==27?126:compact?145:108,y,watchFace==27?48:compact?34:44,h,160);auto* home=button(watchDial,watchFace==27?"[=]":LV_SYMBOL_LIST,watchFace==27?186:compact?181:208,y,watchFace==27?48:compact?34:44,h,2);
  for(auto* o:{b,home}){
    if(watchFace>=29)lv_obj_set_y(o,S(310));
    lv_obj_set_style_pad_all(o,0,0);lv_obj_set_style_bg_opa(o,LV_OPA_TRANSP,0);lv_obj_set_style_border_width(o,0,0);
    auto* icon=lv_obj_get_child(o,0);lv_obj_set_style_text_font(icon,uiFont(F12),0);lv_obj_set_style_text_color(icon,color(p.muted),0);lv_obj_center(icon);
  }
}
static void watchTick(){
  if(!watchDial)return;
  if(watchFace==27)pigTick(millis());
  static uint32_t lastEpoch=0;uint32_t epoch=wallTime(),now=millis();bool mechanism=watchFace==8||watchFace==18||watchFace>=30;
  if(epoch!=lastEpoch){lastEpoch=epoch;watchFrameAt=now;lv_obj_invalidate(watchDial);}
  else if(mechanism&&now-watchFrameAt>=(watchFace==32?100u:200u)){watchFrameAt=now;
    // The clock hands are redrawn over animated ornament in the same clipped pass.
    if(watchFace==18){
      for(int x:{61,247}){lv_area_t flame{(lv_coord_t)S(x),(lv_coord_t)S(132),(lv_coord_t)S(x+52),(lv_coord_t)S(253)};lv_obj_invalidate_area(watchDial,&flame);}return;
    }
    if(watchFace==32){lv_area_t a{lv_coord_t(S(60)),lv_coord_t(S(205)),lv_coord_t(S(300)),lv_coord_t(S(307))};lv_obj_invalidate_area(watchDial,&a);return;}
    if(watchFace>=30){lv_obj_invalidate(watchDial);return;}
    // Redraw only the mechanism between seconds, with the hands on top.
    lv_area_t area{(lv_coord_t)S(78),(lv_coord_t)S(141),(lv_coord_t)S(282),(lv_coord_t)S(253)};lv_obj_invalidate_area(watchDial,&area);
  }
}
static void updateTimerOrbit(uint64_t elapsed){
  if(!timerOrbit)return;
  if(lpu()){lv_obj_invalidate(timerOrbit);return;}
  // A ten-second lap follows monotonic attempt time: pause/OPEN freeze it,
  // resume continues from that position, and reset returns it to twelve.
  float turn=(elapsed%10000000ull)/10000000.f;
  for(unsigned i=0;i<lv_obj_get_child_cnt(timerOrbit);++i){
    auto* dot=lv_obj_get_child(timerOrbit,i);auto at=watchPolar(turn-(6-i)/120.f,174);
    int half=S(i==6?6:i>3?4:3)/2;
    lv_obj_set_pos(dot,S(at.x)-half,S(at.y)-half);
  }
}
static void buildChronographFrame(){
  auto* frame=lv_obj_create(root);lv_obj_remove_style_all(frame);lv_obj_set_size(frame,SCREEN_RES,SCREEN_RES);lv_obj_clear_flag(frame,LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_event_cb(frame,[](lv_event_t* e){auto* ctx=lv_event_get_draw_ctx(e);watchRing(ctx,174,uiColors().border);watchRing(ctx,168,uiColors().raised);watchLine(ctx,126,177,234,177,uiColors().accent);
    if(covert()&&page==Stopwatch){
      // Keep the static dual-brand header in the dial's own drawing pass.
      watchLogo(ctx,radar_logo_ci,20,0,false,false,-26);watchLogo(ctx,watch_logo_mcnally,14,0,false,false,25);
      watchText(ctx,"LOCKSPORT TIMER",48,74,264,F20,uiColors().accent);
    }
    if(lpl()&&page==Stopwatch){
      watchLogo(ctx,radar_logo_ci,20,0,false,false,-28);watchLogo(ctx,watch_logo_lpl,14,0,false,true,25);
      watchText(ctx,"LOCKSPORT TIMER",48,74,264,F20,uiColors().accent);
    }
    if(lpu()&&page==Stopwatch){watchLogo(ctx,radar_logo_lpu,14);watchText(ctx,"LOCKSPORT TIMER",48,74,264,F20,uiColors().accent);}
    if(dmac()&&page==Stopwatch){watchLogo(ctx,radar_logo_dmac,20,0,false,true);watchText(ctx,"LOCKSPORT COUNTER",38,74,284,F20,uiColors().accent);}
    if(locksport())for(float angle:{.125f,.375f,.625f,.875f}){auto at=watchPolar(angle,171);watchScrew(ctx,at.x,at.y,uiColors().accent,locksportBg());}
  },LV_EVENT_DRAW_MAIN,nullptr);
  if(locksport()&&page==Stopwatch){
    timerOrbit=lv_obj_create(frame);lv_obj_remove_style_all(timerOrbit);lv_obj_set_size(timerOrbit,SCREEN_RES,SCREEN_RES);
    lv_obj_clear_flag(timerOrbit,LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_SCROLLABLE);
    if(lpu())lv_obj_add_event_cb(timerOrbit,[](lv_event_t* e){locksportPinRim(lv_event_get_draw_ctx(e),stopwatch.elapsed(stopwatchNow()));},LV_EVENT_DRAW_MAIN,nullptr);
    else for(unsigned i=0;i<7;++i){
      auto* dot=lv_obj_create(timerOrbit);lv_obj_remove_style_all(dot);int size=S(i==6?6:i>3?4:3);lv_obj_set_size(dot,size,size);
      lv_obj_set_style_radius(dot,LV_RADIUS_CIRCLE,0);lv_obj_set_style_bg_color(dot,color(uiColors().accent),0);lv_obj_set_style_bg_opa(dot,lv_opa_t(45+i*35),0);
      lv_obj_clear_flag(dot,LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_SCROLLABLE);
    }
    updateTimerOrbit(stopwatch.elapsed(stopwatchNow()));
  }
}
