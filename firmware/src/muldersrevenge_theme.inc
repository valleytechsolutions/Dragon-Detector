// Original supplied artwork, black field and quiet brass details.
extern lv_img_dsc_t radar_logo_muldersrevenge,watch_logo_muldersrevenge;
static constexpr unsigned MuldersRevengeTheme=38,MuldersRevengeWatch=41;
#if SCREEN_RES>=466
static const lv_font_t *MuldersSmall=&lv_font_montserrat_14,*MuldersTitle=&lv_font_montserrat_20,*MuldersTime=&lv_font_montserrat_48;
#elif SCREEN_RES>=360
static const lv_font_t *MuldersSmall=&lv_font_montserrat_10,*MuldersTitle=&lv_font_montserrat_16,*MuldersTime=&lv_font_montserrat_40;
#else
static const lv_font_t *MuldersSmall=&lv_font_montserrat_8,*MuldersTitle=&lv_font_montserrat_10,*MuldersTime=&lv_font_montserrat_22;
#endif
static void muldersArtwork(lv_draw_ctx_t* ctx,const lv_img_dsc_t& image,int y){
  // Draw the complete emblem, including the wordmark, without cropping it.
  const int x=(SCREEN_RES-image.header.w)/2;
  lv_area_t area{lv_coord_t(x),lv_coord_t(S(y)),lv_coord_t(x+image.header.w-1),lv_coord_t(S(y)+image.header.h-1)};
  lv_draw_img_dsc_t d;lv_draw_img_dsc_init(&d);lv_draw_img(ctx,&d,&area,&image);
}
static void muldersRim(lv_draw_ctx_t* ctx){
  capsuleRing(ctx,174,0x98602a,2);
  for(float angle:{.785398163f,2.35619449f,3.92699082f,5.49778714f})
    disc(ctx,CENTER+sinf(angle)*S(174),CENTER-cosf(angle)*S(174),std::max(1,S(2)),0xf2b95f);
}
static void muldersRadarField(lv_draw_ctx_t* ctx){
  disc(ctx,CENTER,CENTER,S(179),0x000000);muldersRim(ctx);
  for(int r:{42,84,126})capsuleRing(ctx,r,0x332311);
  muldersArtwork(ctx,radar_logo_muldersrevenge,12);
}
static void muldersWatch(lv_draw_ctx_t* ctx,const dragon::Calendar& date,bool valid){
  disc(ctx,CENTER,CENTER,S(179),0x000000);
  muldersArtwork(ctx,watch_logo_muldersrevenge,22);muldersRim(ctx);
  capsuleText(ctx,"MuldersreVenge",55,211,250,MuldersTitle,0xf2b95f);
  char text[40];
  if(valid)snprintf(text,sizeof(text),"%02d:%02d",date.hour,date.minute);else strcpy(text,"--:--");
  capsuleText(ctx,text,60,237,240,MuldersTime,0xffe8bc);
  if(valid)snprintf(text,sizeof(text),"%04d / %02d / %02d",date.year,date.month,date.day);else strcpy(text,"SET DATE / TIME");
  capsuleText(ctx,text,70,285,220,MuldersSmall,0xcab48e);
  if(valid){float angle=date.second*.104719755f;disc(ctx,CENTER+sinf(angle)*S(174),CENTER-cosf(angle)*S(174),std::max(1,S(3)),0xf2b95f);}
}
