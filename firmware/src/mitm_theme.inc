// Supplied photograph stays unchanged; only the dial accents cycle.
extern lv_img_dsc_t watch_logo_mitm;
static constexpr unsigned MitmTheme=39,MitmWatch=42;
static uint32_t mitmColor(uint32_t frame,unsigned offset=0){
  static constexpr uint32_t colors[]={0xff169c,0x973cff,0x00eaff,0x8aff16,0xffbd32};
  const unsigned phase=(frame%15000+offset%15000)%15000,step=phase/3000,t=phase%3000;
  const uint32_t a=colors[step],b=colors[(step+1)%5];uint32_t out=0;
  for(unsigned shift:{0u,8u,16u})out|=((((a>>shift)&255)*(3000-t)+((b>>shift)&255)*t)/3000)<<shift;
  return out;
}
static void mitmWatch(lv_draw_ctx_t* ctx,const dragon::Calendar& date,bool valid,uint32_t frame){
  disc(ctx,CENTER,CENTER,S(180),0x050719);
  const int x=(SCREEN_RES-watch_logo_mitm.header.w)/2,y=(SCREEN_RES-watch_logo_mitm.header.h)/2;
  lv_area_t area{lv_coord_t(x),lv_coord_t(y),lv_coord_t(x+watch_logo_mitm.header.w-1),lv_coord_t(y+watch_logo_mitm.header.h-1)};
  lv_draw_mask_radius_param_t mask;lv_draw_mask_radius_init(&mask,&area,LV_RADIUS_CIRCLE,false);
  const int16_t id=lv_draw_mask_add(&mask,nullptr);
  lv_draw_img_dsc_t image;lv_draw_img_dsc_init(&image);lv_draw_img(ctx,&image,&area,&watch_logo_mitm);
  lv_draw_mask_remove_id(id);lv_draw_mask_free_param(&mask);
  // Bounded, allocation-free color interpolation: one smooth cycle per 15 s.
  for(unsigned i=0;i<60;++i)capsuleRing(ctx,174,mitmColor(frame,i*250),3,i*6,i*6+4);
  const uint32_t accent=mitmColor(frame);
  // A translucent panel preserves the photo while keeping every time readable.
  lv_draw_rect_dsc_t panel;lv_draw_rect_dsc_init(&panel);panel.bg_color=color(0x050719);panel.bg_opa=195;
  panel.radius=S(12);panel.border_width=std::max(1,S(1));panel.border_color=color(accent);
  lv_area_t box{lv_coord_t(S(62)),lv_coord_t(S(219)),lv_coord_t(S(298)),lv_coord_t(S(303))};lv_draw_rect(ctx,&panel,&box);
#if SCREEN_RES>=400
  const lv_font_t *digits=&lv_font_montserrat_48,*small=&lv_font_montserrat_14;
#elif SCREEN_RES>=360
  const lv_font_t *digits=&lv_font_montserrat_40,*small=&lv_font_montserrat_10;
#else
  const lv_font_t *digits=&lv_font_montserrat_22,*small=&lv_font_montserrat_8;
#endif
  char text[40];
  if(valid)snprintf(text,sizeof(text),"%02d:%02d",date.hour,date.minute);else strcpy(text,"--:--");
  capsuleText(ctx,text,65,228,230,digits,0xf5f7ff);
  if(valid)snprintf(text,sizeof(text),"%04d / %02d / %02d",date.year,date.month,date.day);else strcpy(text,"SET DATE / TIME");
  capsuleText(ctx,text,70,278,220,small,accent);
  if(valid){float a=date.second*.104719755f;disc(ctx,CENTER+sinf(a)*S(165),CENTER-cosf(a)*S(165),std::max(1,S(3)),0xffffff);}
}
