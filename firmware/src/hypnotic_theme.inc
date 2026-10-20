// Native spiral: a steady 12-second rotation, with no image or frame allocation.
static constexpr unsigned HypnoticTheme=40,HypnoticWatch=43;
static void hypnoticField(lv_draw_ctx_t* ctx,uint32_t frame,bool radar){
  disc(ctx,CENTER,CENTER,S(179),0x776990);
  disc(ctx,CENTER,CENTER,S(176),radar?0x030509:0xf2f0e8);
  lv_area_t area{lv_coord_t(S(4)),lv_coord_t(S(4)),lv_coord_t(S(356)),lv_coord_t(S(356))};
  lv_draw_mask_radius_param_t mask;lv_draw_mask_radius_init(&mask,&area,LV_RADIUS_CIRCLE,false);
  const int16_t id=lv_draw_mask_add(&mask,nullptr);
  lv_draw_line_dsc_t stroke;lv_draw_line_dsc_init(&stroke);
  stroke.color=color(radar?0x272932:0x060609);stroke.width=std::max(1,S(21));
  stroke.round_start=true;stroke.round_end=true;
  // One trig pair per render; a fixed rotation step generates five turns.
  const float phase=(frame%12000)*.000523598776f;
  float x=cosf(phase),y=sinf(phase);
  lv_point_t previous{lv_coord_t(CENTER),lv_coord_t(CENTER)};
  for(unsigned i=1;i<=240;++i){
    const float nextX=x*.991444861f-y*.130526192f;
    y=x*.130526192f+y*.991444861f;x=nextX;
    const float radius=i*(210.f/240.f)*(SCREEN_RES/360.f);
    lv_point_t next{lv_coord_t(CENTER+lroundf(x*radius)),lv_coord_t(CENTER+lroundf(y*radius))};
    lv_draw_line(ctx,&stroke,&previous,&next);previous=next;
  }
  lv_draw_mask_remove_id(id);lv_draw_mask_free_param(&mask);
}
static void hypnoticWatch(lv_draw_ctx_t* ctx,const dragon::Calendar& date,bool valid,uint32_t frame){
  hypnoticField(ctx,frame,false);
  lv_draw_rect_dsc_t panel;lv_draw_rect_dsc_init(&panel);panel.bg_color=color(0x08080e);panel.bg_opa=245;
  panel.radius=S(14);panel.border_width=std::max(1,S(2));panel.border_color=color(0xc5b4eb);
  lv_area_t box{lv_coord_t(S(69)),lv_coord_t(S(137)),lv_coord_t(S(291)),lv_coord_t(S(223))};
  lv_draw_rect(ctx,&panel,&box);
#if SCREEN_RES>=466
  const lv_font_t *digits=&lv_font_montserrat_48,*small=&lv_font_montserrat_14;
#elif SCREEN_RES>=360
  const lv_font_t *digits=&lv_font_montserrat_40,*small=&lv_font_montserrat_10;
#else
  const lv_font_t *digits=&lv_font_montserrat_22,*small=&lv_font_montserrat_8;
#endif
  char text[40];
  if(valid)snprintf(text,sizeof(text),"%02d:%02d",date.hour,date.minute);else strcpy(text,"--:--");
  capsuleText(ctx,text,72,145,216,digits,0xf5f3ed);
  if(valid)snprintf(text,sizeof(text),"%04d / %02d / %02d",date.year,date.month,date.day);else strcpy(text,"SET DATE / TIME");
  capsuleText(ctx,text,74,199,212,small,0xc5b4eb);
}
