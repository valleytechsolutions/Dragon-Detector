// Supplied artwork and native vector dials. No additional image buffers or
// per-frame allocations; coordinates use the shared 360-pixel design space.
extern lv_img_dsc_t radar_logo_ogg,watch_logo_ogg,radar_logo_xombi,watch_logo_xombi;
static constexpr unsigned OggTheme=36,XombiTheme=37,OggWatch=39,XombiWatch=40;
static bool greenArtTheme(unsigned theme){return theme==OggTheme||theme==XombiTheme;}
#if SCREEN_RES>=466
static const lv_font_t *GreenSmall=&lv_font_montserrat_14,*GreenTitle=&lv_font_montserrat_20,*GreenTime=&lv_font_montserrat_48;
#elif SCREEN_RES>=360
static const lv_font_t *GreenSmall=&lv_font_montserrat_10,*GreenTitle=&lv_font_montserrat_16,*GreenTime=&lv_font_montserrat_40;
#else
static const lv_font_t *GreenSmall=&lv_font_montserrat_8,*GreenTitle=&lv_font_montserrat_10,*GreenTime=&lv_font_montserrat_22;
#endif
static void greenArtwork(lv_draw_ctx_t* ctx,const lv_img_dsc_t& image,int y,bool ogg){
  // Retain the supplied pixels. A circular display mask hides the square JPEG
  // corners (and Ogg's white outer margin) without changing the source artwork.
  int x=(SCREEN_RES-image.header.w)/2,inset=ogg?std::max(1,int(image.header.w)*2/100):0;
  lv_area_t area{lv_coord_t(x),lv_coord_t(S(y)),lv_coord_t(x+image.header.w-1),lv_coord_t(S(y)+image.header.h-1)};
  lv_area_t circle{lv_coord_t(area.x1+inset),lv_coord_t(area.y1+inset),lv_coord_t(area.x2-inset),lv_coord_t(area.y2-inset)};
  lv_draw_mask_radius_param_t mask;lv_draw_mask_radius_init(&mask,&circle,LV_RADIUS_CIRCLE,false);
  int16_t id=lv_draw_mask_add(&mask,nullptr);
  lv_draw_img_dsc_t d;lv_draw_img_dsc_init(&d);lv_draw_img(ctx,&d,&area,&image);
  if(id>=0)lv_draw_mask_remove_id(id);
  lv_draw_mask_free_param(&mask);
}
static void greenRim(lv_draw_ctx_t* ctx,bool zombie){
  capsuleRing(ctx,174,zombie?0x416a45:0x146823,2);
  if(zombie){
    // A few worn circuit traces echo the portrait without filling the field.
    for(int side:{-1,1}){
      themeStroke(ctx,180+side*148,107,180+side*137,118,0x293d29);
      themeStroke(ctx,180+side*137,118,180+side*137,140,0x293d29);
      themeStroke(ctx,180+side*151,238,180+side*139,249,0x293d29);
      themeStroke(ctx,180+side*139,249,180+side*139,260,0x293d29);
    }
  }
}
static void greenRadarField(lv_draw_ctx_t* ctx,bool zombie){
  disc(ctx,CENTER,CENTER,S(179),zombie?0x090f0b:0x000000);
  greenRim(ctx,zombie);
  for(int r:{42,84,126})capsuleRing(ctx,r,zombie?0x233826:0x0c2812);
  greenArtwork(ctx,zombie?radar_logo_xombi:radar_logo_ogg,zombie?10:17,!zombie);
}
static void greenPoly(lv_draw_ctx_t* ctx,const lv_point_t* vertices,unsigned count,uint32_t ink){
  lv_point_t points[8];if(count>8)return;
  for(unsigned i=0;i<count;++i)points[i]={lv_coord_t(S(vertices[i].x)),lv_coord_t(S(vertices[i].y))};
  lv_draw_rect_dsc_t d;lv_draw_rect_dsc_init(&d);d.bg_color=color(ink);lv_draw_polygon(ctx,&d,points,count);
}
static void greenRay(lv_draw_ctx_t* ctx,float turns,int inner,int outer,uint32_t ink,int width=1){
  float a=turns*6.28318530718f;
  themeStroke(ctx,180+sinf(a)*inner,180-cosf(a)*inner,180+sinf(a)*outer,180-cosf(a)*outer,ink,width);
}
static void oggKnife(lv_draw_ctx_t* ctx,float turns,int length,int width,bool minute){
  // Distinct short/broad hour knife and long/slim minute knife. Each blade is
  // two convex facets, so LVGL can draw the bevel without a temporary bitmap.
  float a=turns*6.28318530718f,dx=sinf(a),dy=-cosf(a);
  auto p=[&](int r,int side){return lv_point_t{lv_coord_t(std::lround(180+dx*r-dy*side)),lv_coord_t(std::lround(180+dy*r+dx*side))};};
  auto stroke=[&](int r,int side,int rr,int ss,uint32_t ink,int weight=1){auto v=p(r,side),w=p(rr,ss);themeStroke(ctx,v.x,v.y,w.x,w.y,ink,weight);};
  lv_point_t blade[]={p(16,-width),p(length-22,-width),p(length,0),p(length-18,width),p(16,width)};
  greenPoly(ctx,blade,5,0x020b04);
  lv_point_t spine[]={p(19,-width+2),p(length-23,-width+2),p(length-3,0),p(19,0)};
  greenPoly(ctx,spine,4,minute?0xc9e1cb:0x75ba80);
  lv_point_t bevel[]={p(19,0),p(length-3,0),p(length-20,width-2),p(19,width-2)};
  greenPoly(ctx,bevel,4,minute?0x86a98c:0x3a7345);
  stroke(20,-width+2,length-23,-width+2,0xe4f7e6);
  stroke(24,1,length-24,1,0x24442c);
  stroke(-24,0,14,0,0x020702,width*2+3);
  stroke(-23,0,13,0,0x22492b,width*2-1);
  for(int r:{-18,-10,-2,6})stroke(r,-width+1,r+2,width-1,0x0a170e,2);
  stroke(15,-width-4,15,width+4,0x080f09,5);
  stroke(15,-width-4,15,width+4,0x8ec896,2);
  for(int r:{-18,7}){auto at=p(r,0);disc(ctx,S(at.x),S(at.y),std::max(1,S(2)),0xa4e2ac);}
}
static void oggWatch(lv_draw_ctx_t* ctx,const dragon::Calendar& date,bool valid){
  disc(ctx,CENTER,CENTER,S(179),0x000000);
  capsuleRing(ctx,175,0x123b1a,3);capsuleRing(ctx,172,0x45bb58);capsuleRing(ctx,166,0x12341a);
  capsuleRing(ctx,137,0x122d18);capsuleRing(ctx,135,0x08160c);
  for(int i=0;i<60;++i)greenRay(ctx,i/60.f,i%5?158:151,162,i%5?0x34563b:0x91d99c,i%5?1:3);
  // Quiet facets and the original emblem stay below the high-contrast hands.
  for(int side:{-1,1}){
    themeStroke(ctx,180+side*108,118,180+side*91,100,0x15341d);
    themeStroke(ctx,180+side*108,242,180+side*91,260,0x15341d);
  }
  greenArtwork(ctx,watch_logo_ogg,120,true);
  capsuleText(ctx,"OGG",120,80,120,GreenTitle,0x62c774);
  capsuleText(ctx,"12",157,27,46,GreenTitle,0xd8eddb);
  capsuleText(ctx,"3",303,172,32,GreenTitle,0xd8eddb);
  capsuleText(ctx,"6",164,305,32,GreenTitle,0xd8eddb);
  capsuleText(ctx,"9",25,172,32,GreenTitle,0xd8eddb);
  char text[40];
  if(valid)snprintf(text,sizeof(text),"%02d:%02d",date.hour,date.minute);else strcpy(text,"--:--");
  capsulePanel(ctx,143,258,74,22,0x051109,0x254b2f,3);
  capsuleText(ctx,text,143,262,74,GreenSmall,0xc6e6cd);
  if(valid)snprintf(text,sizeof(text),"%04d / %02d / %02d",date.year,date.month,date.day);else strcpy(text,"SET DATE / TIME");
  capsuleText(ctx,text,80,285,200,GreenSmall,0x73947b);
  // An unset clock deliberately has no hands or moving seconds marker.
  if(valid){
    oggKnife(ctx,(date.hour%12+date.minute/60.f)/12.f,94,10,false);
    oggKnife(ctx,(date.minute+date.second/60.f)/60.f,129,7,true);
    disc(ctx,CENTER,CENTER,S(8),0x030a05);disc(ctx,CENTER,CENTER,S(5),0x5fbf71);disc(ctx,CENTER,CENTER,S(2),0xcfe8d4);
    greenRay(ctx,date.second/60.f,165,171,0x73ff8a,3);
  }
}
static void xombiFigure(lv_draw_ctx_t* ctx,int x,int y,int facing,bool goggles){
  // Two original shambling figures flank the time window. The torn coats,
  // uneven jaws, missing teeth and reaching fingers remain clear at 240px.
  auto stroke=[&](int a,int b,int c,int d,uint32_t ink,int w=1){themeStroke(ctx,x+facing*a,y+b,x+facing*c,y+d,ink,w);};
  auto dot=[&](int a,int b,int r,uint32_t ink){disc(ctx,S(x+facing*a),S(y+b),std::max(1,S(r)),ink);};
  auto poly=[&](const lv_point_t* v,unsigned n,uint32_t ink){lv_point_t p[8];for(unsigned i=0;i<n;++i)p[i]={lv_coord_t(x+facing*v[i].x),lv_coord_t(y+v[i].y)};greenPoly(ctx,p,n,ink);};
  const lv_point_t coat[]={{-16,23},{13,20},{22,53},{9,60},{-22,55}};
  poly(coat,5,0x2e3e30);
  stroke(-16,23,-22,55,0x54674b,2);stroke(13,22,22,53,0x465d40,2);
  stroke(-5,26,1,55,0x0b180e,3);stroke(3,25,1,55,0x71915a);
  const lv_point_t tear[]={{-16,54},{-9,47},{-6,57}};poly(tear,3,0x090f0b);
  stroke(-12,58,-14,68,0x42553d,5);stroke(12,59,16,68,0x42553d,5);
  // Bent arms reach away from the digital readout.
  stroke(-12,27,-25,15,0x43583d,7);stroke(-25,15,-20,-2,0x638453,5);
  stroke(13,29,24,24,0x43583d,7);stroke(24,24,25,8,0x638453,5);
  for(int finger=0;finger<3;++finger){stroke(-23+finger*3,-1,-25+finger*3,-10-finger%2*3,0x91b76e,2);stroke(23+finger*2,9,22+finger*3,2-finger%2*3,0x91b76e,2);}
  stroke(-2,12,-1,25,0x6a8d55,8);
  dot(0,0,16,0x102015);dot(-1,0,14,0x7c9d62);
  const lv_point_t jaw[]={{-11,5},{10,5},{9,17},{-2,21},{-12,13}};poly(jaw,5,0x739358);
  stroke(-9,-11,2,-15,0x2a3d2b,4);stroke(2,-15,11,-9,0x2a3d2b,4);
  dot(-7,0,goggles?6:5,0x15291b);dot(6,-1,goggles?6:5,0x15291b);
  if(goggles){stroke(-2,-2,1,-2,0xa3ae77,2);stroke(-12,-4,-3,-5,0x93a473);stroke(1,-5,10,-5,0x93a473);}
  dot(-7,0,2,0xc7ff79);dot(6,-1,2,0xc7ff79);
  stroke(0,3,-2,7,0x334b30,2);
  stroke(-8,11,7,12,0x101c13,5);
  for(int i:{-6,1,6})stroke(i,10,i,12,0xd7d5a1,2);
  stroke(7,-12,5,-8,0x334b30);stroke(5,-8,9,-5,0x334b30);
  stroke(-13,6,-7,7,0x3b522f);stroke(-10,4,-11,9,0xb8c48b);
}
static void xombiWatch(lv_draw_ctx_t* ctx,const dragon::Calendar& date,bool valid){
  disc(ctx,CENTER,CENTER,S(179),0x090f0b);greenRim(ctx,true);
  capsuleRing(ctx,168,0x253c2a);capsuleRing(ctx,164,0x111f16);
  for(int i=0;i<60;++i){
    const bool lit=valid&&i<=date.second;
    greenRay(ctx,i/60.f,i%5?169:165,172,lit?(i%5?0x568348:0xadc67b):0x2d4030,i%5?1:2);
  }
  // Worn slash marks on the bezel echo the supplied zombie-hacker portrait.
  for(int side:{-1,1})for(int i=0;i<3;++i){int x=180+side*(113+i*6);themeStroke(ctx,x,96-i*5,x-side*5,110-i*5,0x425b3b);}
  greenArtwork(ctx,watch_logo_xombi,29,false);
  xombiFigure(ctx,67,229,1,false);xombiFigure(ctx,293,229,-1,true);
  capsuleText(ctx,"XomBi",100,204,160,GreenTitle,0xa7e76b);
  capsulePanel(ctx,98,229,164,45,0x07100b,0x4e7044,7);
  // Moss hangs from the frame; it never crosses the numeric display.
  for(int i:{0,1,2,3}){int x=105+i*47,h=3+i%3*3;themeStroke(ctx,x,230,x,230+h,0x638943,3);}
  char text[40];
  if(valid)snprintf(text,sizeof(text),"%02d:%02d",date.hour,date.minute);else strcpy(text,"--:--");
  capsuleText(ctx,text,101,235,158,GreenTime,0xe3efce);
  if(valid)snprintf(text,sizeof(text),"%04d / %02d / %02d",date.year,date.month,date.day);else strcpy(text,"SET DATE / TIME");
  capsuleText(ctx,text,82,285,196,GreenSmall,0xabc49b);
  themeStroke(ctx,133,310,149,314,0x355032);themeStroke(ctx,211,314,227,310,0x355032);
  for(int i=0;i<3;++i){int x=171+i*9;themeStroke(ctx,x,309,x+4,319,0x63844a,2);}
}
static void greenWatch(lv_draw_ctx_t* ctx,const dragon::Calendar& date,bool valid,bool zombie){
  if(zombie)xombiWatch(ctx,date,valid);else oggWatch(ctx,date,valid);
}
