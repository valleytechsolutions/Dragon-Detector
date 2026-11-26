// Full-panel drawing for the 2.06-inch AMOLED, in native rotated coordinates.
static void panelBox(lv_draw_ctx_t* ctx,int x,int y,int w,int h,uint32_t fill,uint32_t edge,int radius=8){
  lv_draw_rect_dsc_t d;lv_draw_rect_dsc_init(&d);d.radius=radius;d.bg_color=color(fill);d.border_color=color(edge);d.border_width=1;
  lv_area_t a{lv_coord_t(x),lv_coord_t(y),lv_coord_t(x+w-1),lv_coord_t(y+h-1)};lv_draw_rect(ctx,&d,&a);
}
static void panelText(lv_draw_ctx_t* ctx,const char* text,int x,int y,int w,const lv_font_t* font,uint32_t ink){
  lv_draw_label_dsc_t d;lv_draw_label_dsc_init(&d);d.color=color(ink);d.font=font;d.align=LV_TEXT_ALIGN_CENTER;
  lv_area_t a{lv_coord_t(x),lv_coord_t(y),lv_coord_t(x+w-1),lv_coord_t(y+font->line_height*2)};lv_draw_label(ctx,&d,&a,text,nullptr);
}
static void panelLogo(lv_draw_ctx_t* ctx,const lv_img_dsc_t* logo,int cx,int y,int zoom=256){
  if(!logo||!logo->data)return;
  // Indexed artwork uses its native pixels; LVGL cannot zoom indexed images.
  lv_draw_img_dsc_t d;lv_draw_img_dsc_init(&d);d.zoom=zoom;d.pivot={0,0};const int x=cx-int(logo->header.w)*zoom/512;
  lv_area_t a{lv_coord_t(x),lv_coord_t(y),lv_coord_t(x+logo->header.w-1),lv_coord_t(y+logo->header.h-1)};lv_draw_img(ctx,&d,&a,logo);
}
static void rectangularRadarField(lv_draw_ctx_t* ctx,uint32_t now){
  const auto& p=radarPalette(radarTheme);const int w=lv_disp_get_hor_res(nullptr),h=lv_disp_get_ver_res(nullptr);
  panelBox(ctx,0,0,w,h,p.bottom,p.bottom,0);
  const auto b=dragon::rectangularRadarField(w,h);
  panelBox(ctx,b.x-6,b.y-6,b.w+12,b.h+12,p.top,p.ring);
  for(int i=1;i<6;++i){int x=b.x+i*b.w/6,y=b.y+i*b.h/6;line(ctx,x,b.y,x,b.y+b.h,p.grid,1,180);line(ctx,b.x,y,b.x+b.w,y,p.grid,1,180);}
  const int sweep=b.x+int((radarSweepAt%6000)*b.w/6000);
  const unsigned style=radarTheme==dragon::PersonalThemeId?personalTheme().sweep:0;
  if(style==2){for(int i=12;i>=0;--i)if(sweep-i*5>=b.x)disc(ctx,sweep-i*5,b.y+b.h/2,3,i?p.grid:p.sweep);}
  else{
    for(int i=18;i>=0;--i)if(sweep-i>=b.x)line(ctx,sweep-i,b.y,sweep-i,b.y+b.h,p.sweep,1,(19-i)*5);
    line(ctx,sweep,b.y,sweep,b.y+b.h,p.sweep,2,200);
    if(style==1){const int second=b.x+b.w-(sweep-b.x);line(ctx,second,b.y,second,b.y+b.h,personalTheme().secondary,2,200);}
  }
  const int cx=b.x+b.w/2,cy=b.y+b.h/2;
  line(ctx,cx-6,cy,cx+6,cy,p.text);line(ctx,cx,cy-6,cx,cy+6,p.text);
  (void)now;
}
