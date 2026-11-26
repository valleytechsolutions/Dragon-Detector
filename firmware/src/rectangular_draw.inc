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
#include "rectangular_art.inc"
static void rectangularRadarField(lv_draw_ctx_t* ctx,uint32_t now){
  const auto& p=radarPalette(radarTheme);const int w=lv_disp_get_hor_res(nullptr),h=lv_disp_get_ver_res(nullptr);
  panelBox(ctx,0,0,w,h,p.bottom,p.bottom,0);
  const auto b=dragon::rectangularRadarField(w,h);
  panelBox(ctx,b.x-6,b.y-6,b.w+12,b.h+12,p.top,p.ring);
  for(int i=1;i<6;++i){int x=b.x+i*b.w/6,y=b.y+i*b.h/6;line(ctx,x,b.y,x,b.y+b.h,p.grid,1,180);line(ctx,b.x,y,b.x+b.w,y,p.grid,1,180);}
  const int cx=b.x+b.w/2,cy=b.y+b.h/2;
  const float turn=.25f+(radarSweepAt%6000)/6000.f;
  auto edge=[&](float at){return dragon::rectangularRay(at,cx,cy,b.w/2-6,b.h/2-6);};
  const auto beam=edge(turn);
  const unsigned style=radarTheme==dragon::PersonalThemeId?personalTheme().sweep:0;
  if(style==2){
    const PanelInstrument ray{{cx,cy},beam};
    for(int i=1;i<=10;++i){const auto pt=ray.at(i/10.f,0);disc(ctx,pt.x,pt.y,i==10?5:3,i%2?p.ring:p.sweep);}
  }
  else{
    for(int i=7;i>0;--i){
      const auto tail=edge(turn-i*.009f);
      line(ctx,cx,cy,tail.x,tail.y,p.sweep,2,96-i*12);
    }
    const auto instrumentTip=panelLocksport(radarTheme)||radarTheme==29||radarTheme==33
      ?dragon::PanelPoint{cx+(beam.x-cx)*8/10,cy+(beam.y-cy)*8/10}:beam;
    panelThemeSweep(ctx,{{cx,cy},instrumentTip},radarTheme,p,radarSweepAt);
    if(style==1){const auto second=edge(turn+.5f);line(ctx,cx,cy,second.x,second.y,personalTheme().secondary,2,200);}
  }
  line(ctx,cx-6,cy,cx+6,cy,p.text);line(ctx,cx,cy-6,cx,cy+6,p.text);
  (void)now;
}
