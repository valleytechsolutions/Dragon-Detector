extern lv_img_dsc_t watch_logo_capsule;
static void capsulePanel(lv_draw_ctx_t* ctx,int x,int y,int w,int h,uint32_t fill,uint32_t border,int radius=5){
  lv_draw_rect_dsc_t d;lv_draw_rect_dsc_init(&d);d.bg_color=color(fill);d.border_color=color(border);d.border_width=std::max(1,S(1));d.radius=S(radius);
  lv_area_t a{lv_coord_t(S(x)),lv_coord_t(S(y)),lv_coord_t(S(x+w)-1),lv_coord_t(S(y+h)-1)};lv_draw_rect(ctx,&d,&a);
}
static void capsuleRing(lv_draw_ctx_t* ctx,int radius,uint32_t ink,int width=1,int start=0,int end=359){
  lv_draw_arc_dsc_t d;lv_draw_arc_dsc_init(&d);d.color=color(ink);d.width=std::max(1,S(width));lv_point_t center{CENTER,CENTER};lv_draw_arc(ctx,&d,&center,S(radius),start,end);
}
static void capsuleText(lv_draw_ctx_t* ctx,const char* text,int x,int y,int width,const lv_font_t* font,uint32_t ink){
  lv_draw_label_dsc_t d;lv_draw_label_dsc_init(&d);d.color=color(ink);d.font=font;d.align=LV_TEXT_ALIGN_CENTER;
  lv_area_t a{lv_coord_t(S(x)),lv_coord_t(S(y)),lv_coord_t(S(x+width)-1),lv_coord_t(S(y)+font->line_height+2)};lv_draw_label(ctx,&d,&a,text,nullptr);
}
static void capsuleShell(lv_draw_ctx_t* ctx){
  disc(ctx,CENTER,CENTER,S(179),0x142330);disc(ctx,CENTER,CENTER,S(176),0x687f91);
  disc(ctx,CENTER,CENTER,S(173),0xf1f5f6);disc(ctx,CENTER,CENTER,S(162),0xb5c4cd);
  disc(ctx,CENTER,CENTER,S(157),0x152c3c);capsuleRing(ctx,175,0xffffff,2,202,332);
  capsuleRing(ctx,166,0x718b9b,2,20,154);
  for(int side:{-1,1}){int x=180+side*167;disc(ctx,S(x),CENTER,S(3),0x4c6374);line(ctx,S(x-2),CENTER,S(x+2),CENTER,0xe3edf2,std::max(1,S(1)));}
}
static void capsuleRadarField(lv_draw_ctx_t* ctx,uint32_t frame){
  capsuleShell(ctx);disc(ctx,CENTER,CENTER,S(153),0x29b359);disc(ctx,CENTER,CENTER,S(150),0x22a953);
  constexpr int radius=149;
  for(int offset=-120;offset<=120;offset+=24){int reach=int(sqrtf(float(radius*radius-offset*offset)));
    themeStroke(ctx,180+offset,180-reach,180+offset,180+reach,offset?0x146c3b:0x0d5630);
    themeStroke(ctx,180-reach,180+offset,180+reach,180+offset,offset?0x146c3b:0x0d5630);
  }
  // A quiet refresh band decorates the screen; contacts still come from the detector.
  int x=int(frame%5000)*288/5000-144,reach=int(sqrtf(float(radius*radius-x*x)));
  line(ctx,S(180+x),S(180-reach),S(180+x),S(180+reach),0xc7f6a6,std::max(1,S(2)),35);
  capsulePanel(ctx,166,7,28,9,0xd64034,0x5b1d25,3);
}
static void capsuleHand(lv_draw_ctx_t* ctx,float turns,int length,int width,uint32_t face,uint32_t tip){
  float a=turns*6.28318530718f;auto point=[&](int r){return lv_point_t{lv_coord_t(S(180)+sinf(a)*S(r)),lv_coord_t(S(180)-cosf(a)*S(r))};};
  auto p=point(-12),q=point(length);line(ctx,p.x,p.y,q.x,q.y,0x163348,std::max(1,S(width+4)));
  line(ctx,p.x,p.y,q.x,q.y,face,std::max(1,S(width)));p=point(length-26);line(ctx,p.x,p.y,q.x,q.y,tip,std::max(1,S(width-2)));
}
static void capsuleWatch(lv_draw_ctx_t* ctx,const dragon::Calendar& date,bool valid){
#if SCREEN_RES>=466
  const lv_font_t *small=&lv_font_montserrat_14,*medium=&lv_font_montserrat_20,*digits=&lv_font_montserrat_40;
#elif SCREEN_RES>=360
  const lv_font_t *small=&lv_font_montserrat_10,*medium=&lv_font_montserrat_16,*digits=&lv_font_montserrat_32;
#else
  const lv_font_t *small=&lv_font_montserrat_8,*medium=&lv_font_montserrat_10,*digits=&lv_font_montserrat_22;
#endif
  capsuleShell(ctx);disc(ctx,CENTER,CENTER,S(153),0xf1f5f6);capsuleRing(ctx,147,0x2e6e94,2);
  for(int i=0;i<60;++i){float a=i*.104719755f;int r=i%5?139:132;
    line(ctx,S(180)+sinf(a)*S(r),S(180)-cosf(a)*S(r),S(180)+sinf(a)*S(143),S(180)-cosf(a)*S(143),i%5?0x8aa3b5:0x1b4d70,std::max(1,S(i%5?1:3)));
  }
  for(int i:{12,3,9}){float a=i*.523598776f;char n[3];snprintf(n,sizeof(n),"%d",i);capsuleText(ctx,n,162+lroundf(sinf(a)*119),172-lroundf(cosf(a)*119),36,medium,0x17394f);}
  themeArtwork(ctx,watch_logo_capsule,72);
  if(valid){
    capsuleHand(ctx,(date.hour%12+date.minute/60.f+date.second/3600.f)/12.f,77,9,0xffffff,0x287da8);
    capsuleHand(ctx,(date.minute+date.second/60.f)/60.f,111,6,0x3b92bc,0xffffff);
    disc(ctx,CENTER,CENTER,S(8),0x183c55);disc(ctx,CENTER,CENTER,S(4),0xe9f3f7);
    int degrees=(int(date.second)*6+270)%360;capsuleRing(ctx,155,0xffb62e,3,degrees,(degrees+4)%360);
  }
  capsulePanel(ctx,112,238,136,44,0x0c472c,0x15394c,8);
  char value[30];if(valid)snprintf(value,sizeof(value),"%02d:%02d",date.hour,date.minute);else strcpy(value,"--:--");capsuleText(ctx,value,116,242,128,digits,0xe5ff9e);
  if(valid)snprintf(value,sizeof(value),"%04d.%02d.%02d",date.year,date.month,date.day);else strcpy(value,"SET TIME");capsuleText(ctx,value,102,286,156,small,0x2d5d7e);
}
