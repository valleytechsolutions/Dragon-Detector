// Argyle's supplied emblem, native lockpick hands and one dimple-flag sweep.
extern lv_img_dsc_t radar_logo_argyle,watch_logo_argyle;
static constexpr unsigned ArgyleTheme=42,ArgyleWatch=45;
#if SCREEN_RES>=400
static const lv_font_t *ArgyleSmall=&lv_font_montserrat_14,*ArgyleMedium=&lv_font_montserrat_20;
#elif SCREEN_RES>=360
static const lv_font_t *ArgyleSmall=&lv_font_montserrat_10,*ArgyleMedium=&lv_font_montserrat_16;
#else
static const lv_font_t *ArgyleSmall=&lv_font_montserrat_8,*ArgyleMedium=&lv_font_montserrat_10;
#endif
static void argyleArtwork(lv_draw_ctx_t* ctx,const lv_img_dsc_t& image,int y){
  int x=(SCREEN_RES-image.header.w)/2,inset=std::max(1,int(image.header.w)/100);
  lv_area_t circle{lv_coord_t(x+inset),lv_coord_t(S(y)+inset),lv_coord_t(x+image.header.w-inset-1),lv_coord_t(S(y)+image.header.h-inset-1)};
  lv_draw_mask_radius_param_t mask;lv_draw_mask_radius_init(&mask,&circle,LV_RADIUS_CIRCLE,false);
  int16_t id=lv_draw_mask_add(&mask,nullptr);themeArtwork(ctx,image,y);
  if(id>=0)lv_draw_mask_remove_id(id);lv_draw_mask_free_param(&mask);
}
static void argyleRim(lv_draw_ctx_t* ctx){
  capsuleRing(ctx,176,0x111417,3);capsuleRing(ctx,170,0x8b9092);
  for(int i=0;i<60;++i){float a=i*.104719755f;int inner=i%5?163:156;
    themeStroke(ctx,180+sinf(a)*inner,180-cosf(a)*inner,180+sinf(a)*166,180-cosf(a)*166,i%5?0x8b9092:0x111417,i%5?1:2);
  }
}
static void argylePick(lv_draw_ctx_t* ctx,float turns,int length,bool dimple,bool minute=false){
  float a=turns*6.28318530718f,dx=sinf(a),dy=-cosf(a);
  auto p=[&](float r,float side){return lv_point_t{lv_coord_t(CENTER+dx*S(r)+cosf(a)*S(side)),lv_coord_t(CENTER+dy*S(r)+sinf(a)*S(side))};};
  auto stroke=[&](float r,float s,float rr,float ss,uint32_t ink,int w){auto x=p(r,s),y=p(rr,ss);line(ctx,x.x,x.y,y.x,y.y,ink,std::max(1,S(w)));};
  // White edging keeps the tool legible while it crosses the black emblem.
  stroke(-14,0,43,0,0xffffff,dimple?19:17);
  stroke(-14,0,43,0,0x111417,dimple?14:12);
  stroke(-11,-3,40,-3,dimple?0x727b82:minute?0xd3dce1:0x818d96,3);
  if(dimple){
    // Cylindrical, banded grip; slim round shaft and short sideways flag.
    for(int r=0;r<=32;r+=8)stroke(r,-5,r,5,0x9aa4ab,1);
    stroke(38,-5,38,5,0xd6dce0,2);
    stroke(44,0,length-4,0,0xffffff,7);stroke(44,0,length-4,0,0x252b30,4);stroke(45,-1,length-4,-1,0xb4c4cf,1);
    stroke(length-4,0,length,3,0xffffff,8);stroke(length,3,length,10,0xffffff,8);
    stroke(length-4,0,length,3,0x252b30,4);stroke(length,3,length,10,0x252b30,5);
    stroke(length-1,3,length-1,9,0xc2d0d8,2);
  }else{
    // Flat handles and a short hook, with two lengths for hour/minute reading.
    stroke(-8,0,3,0,0xd8e1e6,3);
    for(int r:{14,24,34})stroke(r,-3,r+3,2,0xd8e1e6,2);
    for(int pass=0;pass<3;++pass){uint32_t ink=pass==0?0xffffff:pass==1?0x171d21:minute?0xc2d0d8:0x758793;int w=pass==0?9:pass==1?5:2;
      stroke(43,0,length-12,0,ink,w);stroke(length-12,0,length-5,-3,ink,w);stroke(length-5,-3,length,-9,ink,w);
    }
  }
}
static void argyleRadarField(lv_draw_ctx_t* ctx){
  disc(ctx,CENTER,CENTER,S(179),0xf5f5f2);argyleRim(ctx);
  for(int r:{42,84,126})capsuleRing(ctx,r,0xc3c7c8);
  themeStroke(ctx,25,180,335,180,0xd6d8d8);themeStroke(ctx,180,100,180,278,0xd6d8d8);
  argyleArtwork(ctx,watch_logo_argyle,90);
  capsuleText(ctx,"ARGYLE",60,32,240,ArgyleMedium,0x111417);
}
static void argyleRadarSweep(lv_draw_ctx_t* ctx,float turns){
  argylePick(ctx,turns,151,true);
  disc(ctx,CENTER,CENTER,S(5),0xffffff);disc(ctx,CENTER,CENTER,S(3),0x111417);
}
static void argyleWatch(lv_draw_ctx_t* ctx,const dragon::Calendar& date,bool valid){
  disc(ctx,CENTER,CENTER,S(179),0xf5f5f2);argyleRim(ctx);
  argyleArtwork(ctx,watch_logo_argyle,90);
  capsuleText(ctx,"ARGYLE",75,55,210,ArgyleMedium,0x111417);
  capsuleText(ctx,"12",163,24,34,ArgyleMedium,0x111417);
  capsuleText(ctx,"3",315,172,24,ArgyleMedium,0x111417);capsuleText(ctx,"9",21,172,24,ArgyleMedium,0x111417);
  if(valid){
    argylePick(ctx,(date.hour%12+date.minute/60.f+date.second/3600.f)/12.f,88,false);
    argylePick(ctx,(date.minute+date.second/60.f)/60.f,132,false,true);
    disc(ctx,CENTER,CENTER,S(7),0xffffff);disc(ctx,CENTER,CENTER,S(5),0x171d21);disc(ctx,CENTER,CENTER,S(2),0xb5c6d2);
    int angle=(int(date.second)*6+270)%360;capsuleRing(ctx,173,0x111417,3,angle,(angle+4)%360);
  }
  capsulePanel(ctx,110,286,140,38,0x111417,0x111417,4);char value[32];
  if(valid)snprintf(value,sizeof(value),"%02d:%02d:%02d",date.hour,date.minute,date.second);else strcpy(value,"SET TIME");
  capsuleText(ctx,value,110,289,140,ArgyleMedium,0xffffff);
  if(valid)snprintf(value,sizeof(value),"%02d.%02d.%04d",date.month,date.day,date.year);else strcpy(value,"CLOCK NOT SYNCED");
  capsuleText(ctx,value,108,310,144,ArgyleSmall,0xc9d1d6);
}
