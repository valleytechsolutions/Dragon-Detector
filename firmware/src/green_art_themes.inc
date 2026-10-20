// Supplied Ogg / XomBi artwork with simple, allocation-free round layouts.
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
static void greenWatch(lv_draw_ctx_t* ctx,const dragon::Calendar& date,bool valid,bool zombie){
  const uint32_t bg=zombie?0x090f0b:0x000000,accent=zombie?0xa7e76b:0x3ee052;
  disc(ctx,CENTER,CENTER,S(179),bg);greenRim(ctx,zombie);
  greenArtwork(ctx,zombie?watch_logo_xombi:watch_logo_ogg,zombie?25:48,!zombie);
  capsuleText(ctx,zombie?"XomBi":"OGG",80,zombie?201:181,200,GreenTitle,accent);
  char text[40];
  if(valid)snprintf(text,sizeof(text),"%02d:%02d",date.hour,date.minute);else strcpy(text,"--:--");
  capsuleText(ctx,text,60,zombie?225:209,240,GreenTime,zombie?0xdceccc:0xe0f5e0);
  if(valid)snprintf(text,sizeof(text),"%04d / %02d / %02d",date.year,date.month,date.day);else strcpy(text,"SET DATE / TIME");
  capsuleText(ctx,text,70,zombie?274:263,220,GreenSmall,zombie?0xabc49b:0x9ac9a0);
  // A small seconds marker is the only clock animation.
  if(valid){float angle=date.second*.104719755f;disc(ctx,CENTER+sinf(angle)*S(174),CENTER-cosf(angle)*S(174),std::max(1,S(3)),accent);}
}
