// Original native Ronin-inspired tools; supplied LLT artwork, see CREDITS.md.
extern lv_img_dsc_t radar_logo_llt,watch_logo_llt;
static constexpr unsigned LawLockToolsTheme=43,LawLockToolsWatch=46;
static constexpr uint32_t LltBlue=0x008bc9,LltInk=0x074567;
#if SCREEN_RES>=400
static const lv_font_t *LltSmall=&lv_font_montserrat_14,*LltMedium=&lv_font_montserrat_20;
#elif SCREEN_RES>=360
static const lv_font_t *LltSmall=&lv_font_montserrat_10,*LltMedium=&lv_font_montserrat_16;
#else
static const lv_font_t *LltSmall=&lv_font_montserrat_8,*LltMedium=&lv_font_montserrat_10;
#endif
static void lltArtwork(lv_draw_ctx_t* ctx,const lv_img_dsc_t& image,int y,int opacity=255){
  lv_draw_img_dsc_t d;lv_draw_img_dsc_init(&d);d.recolor=color(LltBlue);d.recolor_opa=255;d.opa=opacity;
  int x=(SCREEN_RES-image.header.w)/2;lv_area_t a{lv_coord_t(x),lv_coord_t(S(y)),lv_coord_t(x+image.header.w-1),lv_coord_t(S(y)+image.header.h-1)};
  lv_draw_img(ctx,&d,&a,&image);
}
static void lltPinRim(lv_draw_ctx_t* ctx,uint64_t seconds,bool valid=true){
  capsuleRing(ctx,176,LltBlue,2);capsuleRing(ctx,148,0xc4e3f3);
  unsigned current=seconds%60;
  for(unsigned i=0;i<60;++i){float a=i*.104719755f,dx=sinf(a),dy=-cosf(a);bool active=valid&&i==current;int drop=active?4:0;
    auto stroke=[&](int r,int s,int rr,int ss,uint32_t c,int w){themeStroke(ctx,180+dx*r+cosf(a)*s,180+dy*r+sinf(a)*s,180+dx*rr+cosf(a)*ss,180+dy*rr+sinf(a)*ss,c,w);};
    uint32_t driver=active?LltBlue:0xa9d6ee,key=active?LltInk:0x5a9fc4;int w=i%5?2:3;
    stroke(160-drop,0,166-drop,0,driver,w);stroke(153-drop,0,158-drop,0,key,w);
    disc(ctx,S(180+dx*(153-drop)),S(180+dy*(153-drop)),std::max(1,S(w)/2),key);
    // Twelve readable spring chambers mark the five-second stations.
    if(i%5==0){stroke(168-drop,0,170,-2,0x79acc8,1);stroke(170,-2,172,2,0x79acc8,1);stroke(172,2,174,0,0x79acc8,1);}
  }
}
static void lltPick(lv_draw_ctx_t* ctx,float turns,int length,bool gem){
  float a=turns*6.28318530718f,dx=sinf(a),dy=-cosf(a);
  auto p=[&](float r,float side){return lv_point_t{lv_coord_t(CENTER+dx*S(r)+cosf(a)*S(side)),lv_coord_t(CENTER+dy*S(r)+sinf(a)*S(side))};};
  auto stroke=[&](float r,float s,float rr,float ss,uint32_t ink,int w){auto x=p(r,s),y=p(rr,ss);line(ctx,x.x,x.y,y.x,y.y,ink,std::max(1,S(w)));};
  // Ronin PRO: laminated steel handle, oval opening, long slot, three diagonal
  // grip openings, taper to a slender hook. No generic arrow-shaped hands.
  stroke(-18,0,48,0,0xffffff,20);stroke(-18,0,48,0,LltInk,16);stroke(-17,-1,47,-1,0xb8c9d4,12);stroke(-16,-4,46,-4,0xf8fcff,2);
  stroke(-10,0,-7,0,LltInk,4);stroke(0,0,13,0,LltInk,3);
  for(int r:{23,32,41})stroke(r,-3,r+3,3,LltInk,2);
  for(int pass=0;pass<3;++pass){uint32_t c=pass==0?0xffffff:pass==1?LltInk:0xb7d9eb;int w=pass==0?8:pass==1?5:2;
    stroke(49,0,length-15,0,c,w);stroke(length-15,0,length-7,-4,c,w);
    stroke(length-7,-4,length,-(gem?13:8),c,w);
    if(gem)stroke(length,-13,length+3,-10,c,w);
  }
}
static void lltRadarField(lv_draw_ctx_t* ctx){
  disc(ctx,CENTER,CENTER,S(179),0xffffff);capsuleRing(ctx,176,LltBlue,2);
  for(int r:{42,84,126,166})capsuleRing(ctx,r,0xc4e3f3);
  themeStroke(ctx,25,180,335,180,0xdceef8);themeStroke(ctx,180,105,180,278,0xdceef8);
  lltArtwork(ctx,radar_logo_llt,32);lltArtwork(ctx,watch_logo_llt,110,60);
}
static void lltRadarSweep(lv_draw_ctx_t* ctx,float turns){lltPick(ctx,turns,150,false);disc(ctx,CENTER,CENTER,S(5),LltBlue);disc(ctx,CENTER,CENTER,S(2),0xffffff);}
static void lltWatch(lv_draw_ctx_t* ctx,const dragon::Calendar& d,bool valid){
  disc(ctx,CENTER,CENTER,S(179),0xffffff);lltPinRim(ctx,d.second,valid);
  lltArtwork(ctx,radar_logo_llt,59);lltArtwork(ctx,watch_logo_llt,110,55);
  capsuleText(ctx,"12",162,28,36,LltMedium,LltInk);capsuleText(ctx,"3",304,171,25,LltMedium,LltInk);capsuleText(ctx,"9",31,171,25,LltMedium,LltInk);
  if(valid){lltPick(ctx,(d.hour%12+d.minute/60.f+d.second/3600.f)/12.f,86,false);lltPick(ctx,(d.minute+d.second/60.f)/60.f,127,true);disc(ctx,CENTER,CENTER,S(7),LltInk);disc(ctx,CENTER,CENTER,S(4),LltBlue);disc(ctx,CENTER,CENTER,S(2),0xffffff);}
  capsulePanel(ctx,106,267,148,37,0xeaf6fc,0x9acfe9,4);char value[32];
  if(valid)snprintf(value,sizeof(value),"%02d:%02d:%02d",d.hour,d.minute,d.second);else strcpy(value,"SET TIME");capsuleText(ctx,value,106,270,148,LltMedium,LltInk);
  if(valid)snprintf(value,sizeof(value),"%02d.%02d.%04d",d.month,d.day,d.year);else strcpy(value,"CLOCK NOT SYNCED");capsuleText(ctx,value,104,291,152,LltSmall,0x376783);
}
